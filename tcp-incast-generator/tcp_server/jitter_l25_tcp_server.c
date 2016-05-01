#include "l25_tcp_server.h"
#include <fcntl.h>
#include "sys/sendfile.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define BUF_SIZE 100000000
#define period_time 200 

char send_buf[BUF_SIZE];

// prototypes
                                                        
void *listenClient(void *ptr);

int main (int argc, char *argv[]) {

  socklen_t len;
  struct sockaddr_in servaddr;
  struct sockaddr_in cliaddr;
  int sock_opt = 1;
  int ret;

  // initialize socket

  listenfd = socket(AF_INET, SOCK_STREAM, 0);

  setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt));
  setsockopt(listenfd, IPPROTO_TCP, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(APP_PORT);
  ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  if (ret != 0) {
    perror("Unable to bind!");
  } 

  listen(listenfd, 20);

  printf("TCP test application server started...\n");

  while(1) {
    // wait for connections                                                                                    
    len = sizeof(cliaddr);
    int *sockfd = malloc(sizeof(int));
    pthread_t thread;
    //printf("Listening port %d!\n", APP_PORT);


    *sockfd = accept(listenfd, (struct sockaddr *) &cliaddr, &len);

    pthread_create(&thread, NULL, listenClient, (void *) sockfd);

    /*pid_t pid = fork();

    // child process
    if (pid == 0) {
      close(listenfd);
      getDataFromTheClient(sockfd);
      break;
    }
    // parent process
    else {
      close(sockfd);
      }*/
  }

  return 0;
}
/*
unsigned long do_work() {
    // Here the server is doing work
    char *WORK_STR = "Writing a work string to file";
    struct timeval tv;
    char buf[256];
    unsigned long micro_sec;
    //clock_t start = clock();
    gettimeofday(&tv, NULL);
    micro_sec = (1000000 * tv.tv_sec) + tv.tv_usec;
    int tid = pthread_self();
    sprintf(buf,"%d",tid);
    int len = strlen(buf);
    uint timestamp = (unsigned)time(NULL);
    sprintf(buf+len,"%u",timestamp);
    char *filename = strdup(buf);
    //printf("Filename: %s\n",filename);
    // make a unique string with tid
    int fd = open(filename, O_WRONLY | O_CREAT);
    write(fd, WORK_STR, strlen(WORK_STR));
    close(fd);
    unlink(filename);
    free(filename);
    gettimeofday(&tv, NULL);
    micro_sec = ((1000000 * tv.tv_sec) + tv.tv_usec) - micro_sec;
    //clock_t diff = clock()-start;
    //int msec = (diff * 1000) / CLOCKS_PER_SEC;
    //printf("Number of us: %u\n",micro_sec);
    return micro_sec; 
}*/

int get_random_jitter() {
   int wait = rand() % 200;
   int base = 100; // (0.5 * period_time)
   return base + wait; 
}


// listen client
void *listenClient(void *ptr) {
  int sockfd = *((int *)ptr);
  
  uint n;
  char buf[MAX_LINE];
  int tos;
  socklen_t tos_len;
  int l25;
  uint req_size;
  unsigned long micro_sec = 0;

  free(ptr);

  //printf("%d - Connection established!\n", sockfd);
  while(1) {
    memset(buf, 0, 20);
    n = read(sockfd, buf, 20);
    
    if (n <= 0)
      break;
    
    memcpy(&l25, buf, sizeof(int));
    l25 = ntohl(l25);

    if (l25 == 1) {
      getsockopt(sockfd, IPPROTO_IP, IP_TOS, &tos, &tos_len);
      tos |= 128;
      setsockopt(sockfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
    }

    memcpy(&req_size, buf + sizeof(int), sizeof(uint));
    req_size = ntohl(req_size);

    //micro_sec = do_work();
    micro_sec = 0;
    printf("%lu\n",micro_sec);

    while(req_size > 0) {
      int send_length = req_size;
      if (send_length > BUF_SIZE)
	send_length = BUF_SIZE;
	  //usleep(get_random_jitter());
      int ret = write(sockfd, send_buf, send_length);

      if (ret != send_length) {
	    perror("Unable to send data"); 
	    close(sockfd);
	    return 0;
      }
      
      req_size -= ret;
    }
    
  }
  //printf("\n%d - Connection closed! Bytes read: %u\n", sockfd, total);
    close(sockfd);
  return 0;
}
