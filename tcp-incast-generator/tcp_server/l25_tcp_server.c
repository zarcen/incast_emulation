#include "l25_tcp_server.h"
#include <fcntl.h>
#include "sys/sendfile.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <pthread.h>

#define BUF_SIZE 100000000
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
    // 20K for TCP rcvbuf
    int rcbf = 20 * 1024;
    // 40K for TCP sendbuf
    int sdbf = 40 * 1024;
    setsockopt(listenfd, IPPROTO_TCP, SO_RCVBUF, &rcbf, sizeof(rcbf));
    setsockopt(listenfd, IPPROTO_TCP, SO_SNDBUF, &sdbf, sizeof(sdbf));

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
    }

    return 0;
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
        int err_cnt = 0;
        while(req_size > 0) {
            int send_length = req_size;
            if (send_length > BUF_SIZE)
                send_length = BUF_SIZE;
            int ret = write(sockfd, send_buf, send_length);

            if (ret != send_length) {
                err_cnt++;
                perror("Unable to send data"); 
                if (err_cnt < 3) {
                    continue;
                } else {
                    close(sockfd);
                    return 0;
                }
            }

            req_size -= ret;
        }

    }
    //printf("\n%d - Connection closed! Bytes read: \n", sockfd);
    //printf("\n%d - Connection closed! Bytes read: %u\n", sockfd, total);
    close(sockfd);
    return 0;
}
