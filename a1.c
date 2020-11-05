#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/*
Created by: Sixiang Zhang
COMP112 - A1
Reference:

  https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/tcpclient.c
  https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/tcpserver.c
  http://acme.com/software/micro_proxy/
  https://www.programmingsimplified.com/c/source-code/c-program-insert-substring-into-string
  
TODO:
  Need to wait 1/2 second if read cache for the first time
  Not sure whether max-age can be accurately extracted or not
  max-age only works for the format - Cache-Contol: max-age=XXX
  Port :XXX after hostname should work, need more test
*/


#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define DEFAULT_TIME 36000
//36000
#define CACHE_SIZE 10

#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};
# endif

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}


char *substring(char *string, int position, int length)
{
   char *pointer;
   int c;
 
   pointer = malloc(length+1);
   
   if( pointer == NULL )
       exit(EXIT_FAILURE);
 
   for( c = 0 ; c < length ; c++ )
      *(pointer+c) = *((string+position-1)+c);      
       
   *(pointer+c) = '\0';
 
   return pointer;
}
// Struct for storing data
//====================================
struct Pack{
    char* key;
    char* data;
    unsigned int time_last; // bigger means it's recently GET
    time_t time_expire; // expire time
};
//====================================

int check_expire(struct Pack *p,int num_item);
int check_recent(struct Pack *p,int num_item);
int check_key(struct Pack *p, int num_item, char* key);
void run_server(int portno);
void send_get(char* buf,char* data, int child, char* hostname, int portno);
void put_command(char *data,struct Pack* p, int* num_item,int cache_size,int t, char* key, int* t_count);


int main(int argc, char **argv){


  int portno; /* port to listen on */


  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
  
  run_server(portno);


}



void run_server(int portno)
{
    int parentfd; /* parent socket */
  int childfd; /* child socket */
  
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  
  
  int num_elements = 0;
  struct Pack pack[CACHE_SIZE]; // struct used for storing all data
  int time_count = 0; //
  /* 
   * check command line arguments 
   */


  /* 
   * socket: create the parent socket 
   */
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
  if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");

  /* 
   * main loop: wait for a connection request, echo input line, 
   * then close connection.
   */
  clientlen = sizeof(clientaddr);

  while (1) {
      
      

    /* 
     * accept: wait for a connection request 
     */
    childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
    if (childfd < 0) 
      error("ERROR on accept");
    
    /* 
     * gethostbyaddr: determine who sent the message 
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server established connection with %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    
    /* 
     * read: read input string from the client
     */
    bzero(buf, BUFSIZE);
    n = read(childfd, buf, BUFSIZE);
    if (n < 0) 
      error("ERROR reading from socket");
    printf("server received %d bytes: %s\n", n, buf);
    
    
    

      
    //CONVERT GET COMMAND
    // Mian goal: get HOST AND PORT
    char method[BUFSIZE],url[BUFSIZE],protocol[BUFSIZE],path[BUFSIZE],hostname[BUFSIZE];
    int portno, iport;
    sscanf(buf, "%[^ ] %[^ ] %[^ ]\r", method, url, protocol);
    

    
   if ( strncasecmp( url, "http://", 7 ) == 0 )
    	{
    	(void) strncpy( url, "http", 4 );	/* make sure it's lower case */
    	if ( sscanf( url, "http://%[^:/]:%d%s", hostname, &iport, path ) == 3 )
    	    portno = (unsigned short) iport;
    	else if ( sscanf( url, "http://%[^/]%s", hostname, path ) == 2 )
    	    portno = 80;
    	else if ( sscanf( url, "http://%[^:/]:%d", hostname, &iport ) == 2 )
    	    {
    	    portno = (unsigned short) iport;
    	    *path = '\0';
    	    }
    	else if ( sscanf( url, "http://%[^/]", hostname ) == 1 )
    	    {
    	    portno = 80;
    	    *path = '\0';
    	    }

    	}
      



    
      
      
      
      
       int k = -1;
       
       
       k = check_key(pack,num_elements,url);

       


       if(k ==-1 || (k!=-1 && difftime(pack[k].time_expire,time(NULL))<=0)) // not exist or (exist but expired)
                {
                  char* data = (char*)malloc(1024*1024*10);
                  char* backup = (char*)malloc(1024*1024*10);
                  char* key = (char*)malloc(100); // 100 character
                  
                // if not found in the cache
              // the data which needed to be cached
                  send_get(buf,data,childfd,hostname,portno);
               
                  strcpy(backup,data);
                  
                  char *res;

                  int t= DEFAULT_TIME;
                   
                  res = strtok(data,"\r\n");
                  while( res != NULL ) {
                  
                  
                    if(strstr(res,"max-age")){
                        char max[BUFSIZE];
                        
                        sscanf(res,"Cache-Control: max-age=%s",max); // deal with max-age
                        
                        t = atoi(max);
                  

                        break;
                      }
                    else if(strstr(res,"<html") || strstr(res,"html>")) // when header is over
                       {
                       
                         break; // if didn't find, just break
                       
                       }
                      res = strtok(NULL, "\r\n");
                   }
                 

                    
                    
                    
                    
                    free(data);
                     data = NULL;
                    

                  
                  strcpy(key, url);
                  
                  
                  puts("Get from server\n");
                  put_command(backup,pack,&num_elements,CACHE_SIZE,t, key, &time_count);
                  

                  
                                          
                        
          
                  
                }
        else{
                    // key exist
                    
                    
                    // check whether expire
                    if(difftime(pack[k].time_expire,time(NULL))>0) {
                        // not expire then write to output file

                        
                        pack[k].time_last = time_count;
                        time_count++; // update the time_count, assign to the value which is called by GET
                        

                          puts("Read from cache\n");
                          
                          
                          int flag = 0;
                          char* backup = (char*)malloc(1024*1024*10);
                          strcpy(backup,pack[k].data);
        
                          
                          char *f, *e;
                          int length = strlen(backup);

                          char tp[BUFSIZE];
                          sscanf(backup,"%[^\r\n]",tp); // get lenght of first line of header
                      
                          
                      
                          f = substring(backup, 1, strlen(tp)+2 );      
                         e = substring(backup, strlen(tp)+3, length-strlen(tp)-3+1); // split header into 2 part
                         
                          int a = difftime(pack[k].time_expire,time(NULL));
                          char append[BUFSIZE];
                          sprintf(append,"Age: %d\r\n",a); // construct Age: XXX
                          
                          // create new header with Age
                          char* output = (char*)malloc(1024*1024*10);
                          strcpy(output, f);
                           strcat(output, append);
                           free(f);
                           strcat(output, e);
                           free(e);
                          
                        
                          
                          n = write(childfd, output, strlen(output));
                          
                           
                          
                          free(backup);
                          backup = NULL;
                          free(output);
                          output=NULL;
               
                        
                    }


                }
       
  
      
    
  /*
        int t=0;
        for(t=0;t<num_elements;++t)
        {
            printf("===%s %d %d====\n",pack[t].key,pack[t].time_last,pack[t].time_expire);

        }
        puts("\n");
   
   */     
       
    close(childfd);
    
    
  }
}

// send request and get data
void send_get(char* buf,char* data, int child, char* hostname, int portno)
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
    if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0) 
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



void put_command(char *data,struct Pack* p, int* num_item,int cache_size,int t, char* key, int* t_count)
{



    



                  //pack[num_elements].time_last = time_count;
                  //time_count++; // update the time_count, assign to the value which is called by GET





    // when empty, just insert
        if (*num_item == 0) {



            //insert pack
            p[*num_item].key = key;
            p[*num_item].data = data;


            p[*num_item].time_expire = time(NULL) + t;

           p[*num_item].time_last = *t_count;
           (*t_count)++;


            (*num_item)++;



        } else if (*num_item < cache_size) {
            // when not empty

            // first check
            int r = check_key(p,*num_item,key);
            if(r==-1)
            {

                // if not then insert

                p[*num_item].key = key;
                p[*num_item].data = data;
    


                p[*num_item].time_expire = time(NULL) + t;

           p[*num_item].time_last = *t_count;
           (*t_count)++;

                (*num_item)++;

            }
            else
            {
                // key exist, then update
                // remember to free previous data and key
                free(p[r].data);
                free(p[r].key);
                p[r].data = NULL;
                p[r].key = NULL;

                p[r].key = key;
                p[r].data = data;
     


                p[r].time_expire = time(NULL) + t;

              p[r].time_last = *t_count;
               (*t_count)++;


            }


        } else {
            // full
            // remove expired one first if there's no expire one, remove least recently token one
            // remember to free data and key

            //update
            int r = check_key(p,*num_item,key);
            if(r!=-1)
            {
                free(p[r].data);
                free(p[r].key);
                p[r].data = NULL;
                p[r].key = NULL;

                p[r].key = key;
                p[r].data = data;
           
              p[r].time_last = *t_count;
               (*t_count)++;

                p[r].time_expire = time(NULL) + t;


            }
            else
            {
                //remove
                // check expire, then replace
                int c = check_expire(p,*num_item);


                if(c==-1)
                {
                    free(p[c].data);
                    free(p[c].key);
                    p[c].data = NULL;
                    p[c].key = NULL;

                    p[c].key = key;
                    p[c].data = data;
                


                    p[c].time_expire = time(NULL) + t;

                                  p[c].time_last = *t_count;
               (*t_count)++;

                }
                else
                {
                    // if no expire, check least recently GET
                    // then replace
                    int c = check_recent(p,*num_item);



                    free(p[c].data);
                    free(p[c].key);
                    p[c].data = NULL;
                    p[c].key = NULL;

                    p[c].key = key;
                    p[c].data = data;
            


                    p[c].time_expire = time(NULL) + t;

                                                     p[c].time_last = *t_count;
               (*t_count)++;

                }


            }



        }



}
