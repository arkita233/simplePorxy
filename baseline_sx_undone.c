#include <stdio.h>
#include <string.h> /* memset() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h> 
#include <time.h>



#define BUFSIZE 1024
#define BACKLOG  20  /* Passed to listen() */
#define CACHE_SIZE 20
#define TIMEOUT 300

// ssh comp112-01.cs.tufts.edu
// curl -v -x comp112-01.cs.tufts.edu:9075 http://www.cs.tufts.edu/comp/112/


//========= Cache=====================
struct Pack{
    char* host; // context after GET, in front of HTTP version
    char* key;
    int port;
    char* data; // cache
    unsigned int time_last; // bigger means it's recently GET
    time_t time_expire; // expire time
};
//========= Cache=====================


//=========Function==================
void handle(int newsock, fd_set *set); // handle read and write between our sever and client
int check_expire(struct Pack *p,int num_item);
int check_recent(struct Pack *p,int num_item);
int check_key(struct Pack *p, int num_item, char* key);
void error(char *msg);
void forward_http_request(char* buf,char* data, int child, char* hostname, int portno);
int create_ssl_sock(char* hostname, int portno);
void proxy_ssl(int server, int client);
//=========Function==================



//==================Global==================
struct Pack pack[CACHE_SIZE]; // struct used for storing all data
int num_elements = 0; // # of items in cache
unsigned int time_count = 0; // time of GET from cache 
//=========================================








int main(int argc, char **argv){

  int portno; /* port to listen on */


  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
  
  

  
  fd_set tmp_set, master_set;
  struct Node* client_list = NULL;
  
  int parentfd; /* parent socket */
    int childfd; /* child socket */
  int fdmax;
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  
  
  
  parentfd = socket(AF_INET, SOCK_STREAM, 0);
  if (parentfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));

  /* this is an Internet address */
  serveraddr.sin_family = AF_INET;

  /* let the system figure out our IP address */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* this is the port we will listen on */
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(parentfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * listen: make this socket ready to accept connection requests 
   */
  if (listen(parentfd, 10) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");



	FD_ZERO(&tmp_set);  // Initiliaze the client reading socket list.
	FD_ZERO(&master_set);
	FD_SET(parentfd, &master_set); // Include the server sd into the reading set.
	fdmax = parentfd;


  while(1)
  {
    
    tmp_set = master_set;
    select(fdmax+1, &tmp_set, NULL, NULL, NULL);
    
    int i=0;
    for(i=0; i<= fdmax; i++)
		{
   
        if(FD_ISSET(i,&tmp_set))
        {
           if(i == parentfd) // server is ready, then accept, add to set but not add to client list
           {
              clientlen = sizeof(clientaddr);
               childfd = accept(parentfd, (struct sockaddr *)&clientaddr, (socklen_t *)&clientlen);
               
               
               
   					  if(childfd == -1)
      					{
      						printf("ERROR:- Unable to connect to client\n");
      					}	
      					else
      					{	
      						printf("Got a connection\n");
      						FD_SET(childfd, &master_set);
		         	    if(childfd >= fdmax)
      							fdmax = childfd; 
      					}
               
           }
          else // client ready, receive
          {
              
           handle(i, &master_set);

          }

        } // if ISSET
   
     } // end of for loop
  
  } // end of while




	close(parentfd);
	return(0);

}




void error(char *msg) {
  perror(msg);
}

void handle(int newsock, fd_set *set)
{
    int n = 0;
    char request[BUFSIZE]; // http request from client
     bzero(request, BUFSIZE);
     n = read(newsock, request,BUFSIZE);
     
     
    if(n<=0) // client close the socket
    {
           
     
           close(newsock);
           FD_CLR(newsock,set);
    
    }
    else // deal with message
    {
    printf("bytes: %d \n%s \n",n,request);
      
    // ==================================parse url=============================================
    // 0. parse request
    char method[BUFSIZE],url[BUFSIZE],protocol[BUFSIZE],path[BUFSIZE],host[BUFSIZE];
    int iport, ssl;
    unsigned short port;
    
    
    if(sscanf(request, "%[^ ] %[^ ] %[^\r\n]", method, url, protocol)!=3)
      printf("error prase request from client\n");
    
    
      
    
    if ( strncasecmp( url, "http://", 7 ) == 0 )
    	{
    	(void) strncpy( url, "http", 4 );	/* make sure it's lower case */
    	if ( sscanf( url, "http://%[^:/]:%d%s", host, &iport, path ) == 3 )
    	    port = (unsigned short) iport;
    	else if ( sscanf( url, "http://%[^/]%s", host, path ) == 2 )
    	    port = 80;
    	else if ( sscanf( url, "http://%[^:/]:%d", host, &iport ) == 2 )
    	    {
    	    port = (unsigned short) iport;
    	    *path = '\0';
    	    }
    	else if ( sscanf( url, "http://%[^/]", host ) == 1 )
    	    {
    	    port = 80;
    	    *path = '\0';
    	    }
    	else
    	    printf("error \n");
    	ssl = 0;
    	}
        else if ( strcmp( method, "CONNECT" ) == 0 )
    	{
    	if ( sscanf( url, "%[^:]:%d", host, &iport ) == 2 )
    	    port = (unsigned short) iport;
    	else if ( sscanf( url, "%s", host ) == 1 )
    	    port = 443;
    	else
    	     printf("error \n");
    	ssl = 1;
    	}
        else
     printf("error \n");
    // ==================================parse url=============================================
    
    
    printf("%s %s %s %s %d\n",method, url, protocol, host, port);
    
    
    

    // 1. check cache  
    int k = -1;  
    k = check_key(pack,num_elements,url);  
      
   
    // 2. not exist, or expired, forward request
    if(k ==-1 || (k!=-1 && difftime(pack[k].time_expire,time(NULL))<=0))
    {
       char* data = (char*)malloc(1024*1024*10);
       
      if(ssl!=1) {
        forward_http_request(request,data,newsock,host,port);
        }
      else
        {
          
          int tmp;
          char buf[BUFSIZE];
          sprintf(buf,"%s 200 Connection established\r\n\r\n",protocol);
          
          int server = create_ssl_sock(host,port);
          
          tmp = write( newsock, buf,strlen(buf));

          
          if(tmp>0)
            proxy_ssl(server, newsock);
         

          // "HTTP/1.0 200 Connection established\r\n\r\n"
          //forward_http_request(request,data,newsock,host,port);

        
        }

    }  
    else   // 3. if exist, send cache to client 
    {
    
    }  
     
      
     
      // 3-1. read response with a loop
      // 3-2. cache response
    
    }
    
           close(newsock);
           FD_CLR(newsock,set);
    

}





// check whether key exist or not
int check_key(struct Pack *p, int num_item, char* key)
{

    
    int i = 0;
   
    for(i=0;i<num_item;i++)
    {
        
        if(strcmp(key,p[i].key)==0)
            return i;
    }

    return -1;
}

// check whether element is expired or not
int check_expire(struct Pack *p,int num_item)
{
    int i = 0;
    for(i=0;i<num_item;i++)
    {

        if(difftime(p[i].time_expire,time(NULL)<0))
        {
            // expired
            return i;
        }

    }

    return -1;
}

// check which one is the least recently called
int check_recent(struct Pack *p,int num_item)
{
    int min=p[0].time_last;


    int i =0;
    for(i=0;i<num_item;i++)
    {

        if(p[i].time_last<min)
        {
            min = i;
        }

    }

    return min;

}


void forward_http_request(char* buf,char* data, int child, char* hostname, int portno)
{
   int sockfd, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;


    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");


  
    /* send the message line to the server */
    n = write(sockfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");
    
      

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    int flag = 0;
    while(read(sockfd, buf, BUFSIZE-1)!=0)
    {
    
      if(flag==0){
        strcpy(data, buf);
        flag++;
      
      }
      
      else{
        strcat(data, buf);
        }
        
              /* 
     * write: echo the input string back to the client 
     */
       n = write(child, buf, strlen(buf));
       if (n < 0) 
        error("ERROR writing to socket");
      
      
      bzero(buf, BUFSIZE);
    }

     
    
  

    
    close(sockfd);
    
    
}

int create_ssl_sock(char* hostname, int portno)
{

   int sockfd, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;


    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");
      
      
      
   // char buf[BUFSIZE];
   // sprintf(buf,"%s 200 Connection established\r\n\r\n",protocol);
   
   return sockfd;
    


}



void proxy_ssl(int server, int client)
{
 struct timeval timeout;
     timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
  fd_set fdset;
    int maxp1, r;
    char buf[10000];

   if ( client >= server )
	maxp1 = client + 1;
    else
	maxp1 = server + 1;


   for (;;)
	{
	FD_ZERO( &fdset );
	FD_SET( client, &fdset );
	FD_SET( server, &fdset );
	r = select( maxp1, &fdset, (fd_set*) 0, (fd_set*) 0, &timeout );
	if ( r == 0 ){
	    printf("error\n");
       break;
     }
	else if ( FD_ISSET( client, &fdset ) )
	    {
	    r = read( client, buf, sizeof( buf ) );
	    if ( r <= 0 )
		break;
	    r = write( server, buf, r );
	    if ( r <= 0 )
		break;
	    }
	else if ( FD_ISSET( server, &fdset ) )
	    {
	    r = read( server, buf, sizeof( buf ) );
	    if ( r <= 0 )
		break;
	    r = write( client, buf, r );
	    if ( r <= 0 )
		break;
	    }
	}




}