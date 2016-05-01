#include "l25_tcp_client.h"
#include "sys/sendfile.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <pthread.h>

#define MAX_READ 1000000

// prototypes
void *listen_connection(void *ptr);

int num_dest;
int *dest_port;
int *src_port;
char (*dest_addr)[20];
uint req_size;
uint req_size_n;
int iter;
int *sockets;            
pthread_t *threads;
int *indexes;
struct timeval *start_time;
struct timeval *stop_time; 
int *fmax_usec;
int *fmin_usec;
int *favg_usec; 
int l25;   

int histogram[4];    

int main (int argc, char *argv[]) {
    FILE *fd;
    int ret;
    //int max_sec;
    int max_usec;
    //int min_sec;
    int min_usec;
    //int avg_sec;
    //int avg_usec;
    double avg_msec;
    int numSent;
    int sock_opt = 1;
    // used for calculate the avg, stddev for n iterations
    int* usec_list;
    int* sec_list;

    for (int i = 0; i < 4; i++)
        histogram[i] = 0;

    if (argv[1] == NULL) {
        printf("Missing input file...\n");
        exit(-1);
    }

    printf("Reading file: %s\n", argv[1]);
    fd = fopen(argv[1], "r");

    if (fd == NULL) {
        perror("Unable to open input file");
        exit(-1);
    }

    fscanf (fd, "destinations %d\n", &num_dest);
    printf("Number of destinations: %d\n", num_dest);

    dest_addr = malloc(num_dest * sizeof(char[20]));
    dest_port = malloc(num_dest * sizeof(int));
    src_port = malloc(num_dest * sizeof(int));
    sockets = malloc(num_dest * sizeof(int));
    threads = malloc(num_dest * sizeof(pthread_t));
    indexes = malloc(num_dest * sizeof(int));
    stop_time = malloc(num_dest * sizeof(struct timeval));
    start_time = malloc(num_dest * sizeof(struct timeval));
    fmax_usec = malloc(num_dest * sizeof(int));
    fmin_usec = malloc(num_dest * sizeof(int));
    favg_usec = malloc(num_dest * sizeof(int));

    for (int i = 0; i < num_dest; i++) {
        int tmp;
        fscanf(fd, "%d dest %s %d %d\n", &tmp, dest_addr[i], &dest_port[i], &src_port[i]);
        printf("%d - Dest: %s, %d - Src: %d\n", tmp, dest_addr[i], dest_port[i], src_port[i]);
    }

    fscanf(fd, "size %u\n", &req_size);
    printf("Request size: %u\n", req_size);
    req_size_n = htonl(req_size);

    fscanf(fd, "iterations %d\n", &iter);
    printf("Iterations: %d\n", iter);
    usec_list = malloc(iter * sizeof(int));
    sec_list = malloc(iter * sizeof(int));

    fscanf(fd, "l25 %d\n", &l25);
    printf("L2.5: %d\n", l25);

    l25 = htonl(l25);

    for (int i = 0; i < num_dest; i++) {
        struct sockaddr_in servaddr;
        int sock;
        sock = socket(AF_INET, SOCK_STREAM, 0);

        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt));

        if (ntohl(l25) == 1) {
            int tos;
            socklen_t tos_len;

            getsockopt(sock, IPPROTO_IP, IP_TOS, &tos, &tos_len);
            tos |= 128;
            setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));

            printf("l25 protected!\n");
        }

        int quickack;
        socklen_t quickack_len;

        getsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &quickack, &quickack_len);
        printf("Quick ACK: %d\n", quickack);
        quickack = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &quickack, sizeof(quickack));

        //int rcvbuf = 100 * 1024;
        // setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        if (src_port[i] > 0) {
            memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
            servaddr.sin_port = htons(src_port[i]);
            bind(sock, (struct sockaddr *) &servaddr, sizeof(servaddr));
        }      

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(dest_port[i]);
        inet_pton(AF_INET, dest_addr[i], &servaddr.sin_addr);
        ret = connect(sock, (struct sockaddr *) &servaddr, sizeof(servaddr));

        if (ret < 0) {
            char err_msg[100];
            sprintf(err_msg, "Unable to connect %s:%d", dest_addr[i], dest_port[i]);
            perror(err_msg);
            sockets[i] = -1;
            exit(-1);
        }
        else {
            printf("Connected to %s:%d\n", dest_addr[i], dest_port[i]);
            sockets[i] = sock;
        }
        indexes[i] = i;

        fmax_usec[i] = 0;
        fmin_usec[i] = 999999999;
        favg_usec[i] = 0;
    }

    max_usec = 0;
    min_usec = 999999999;
    //avg_usec = 0;
    avg_msec = 0;
    numSent = 0;

    // start iterating 1000 iterations of requests to servers
    for (int i = 0; i < iter; i++) {
        struct timeval tstart;
        int usec;
        int sec;
        printf("Iteration: %d..", i);

        gettimeofday(&tstart, NULL);

        // create threads for receiving files
        for (int j = 0; j < num_dest; j++) {
            pthread_create(&threads[j], NULL, listen_connection, (void *) &indexes[j]);
        }

        // wait for transfers to complete
        for (int j = 0; j < num_dest; j++) {
            pthread_join(threads[j], NULL);
        }

        struct timeval *tv = &stop_time[0];
        for (int j = 1; j < num_dest; j++) {
            if (tv->tv_sec < stop_time[j].tv_sec || (tv->tv_sec == stop_time[j].tv_sec && tv->tv_usec < stop_time[j].tv_usec)) {
                tv = &stop_time[j];
            }
        }

        sec = tv->tv_sec - tstart.tv_sec;
        usec = tv->tv_usec - tstart.tv_usec;

        if (usec < 0) {
            usec += 1000000;
            sec--;
        }

        printf("Duration, sec: %u, usec: %u\n", sec, usec);

        int j;
        if (usec < 50000)
            j = 0;
        else if (usec < 100000)
            j = 1;
        /*
         *  Small mod to see if we can get an accurate count of drops on smaller tests
         *
         *   else if (usec < 150000)
         *     j = 2;
         */
        else if (usec < 199999)
            j = 2;
        else
            j = 3;
        histogram[j]++;

        usec += sec * 1000000;
        if (i > 0) {
            numSent++;
            if (usec < min_usec)
                min_usec = usec;
            if (usec > max_usec)
                max_usec = usec;
            //avg_usec += usec;
            avg_msec += (usec / 1000.0);

            for (int j = 0; j < num_dest; j++) {
                sec = stop_time[j].tv_sec - start_time[j].tv_sec;
                usec = stop_time[j].tv_usec - start_time[j].tv_usec;
                if (usec < 0) {
                    usec += 1000000;
                    sec--;
                }

                usec += sec * 1000000;
                if (usec < fmin_usec[j])
                    fmin_usec[j] = usec;
                if (usec > fmax_usec[j])
                    fmax_usec[j] = usec;
                favg_usec[j] += usec;
            }
        }
    }

    // close connections
    for (int i = 0; i < num_dest; i++) {
        close(sockets[i]);
    }

    if (numSent > 0) {
        for (int i = 0; i < num_dest; i++) {
            printf("Connection: %d\nAvg: %d sec, %d usec\nMin: %d sec, %d usec\nMax: %d sec, %d usec\n", i, (favg_usec[i] / numSent) / 1000000, (favg_usec[i] / numSent) % 1000000, fmin_usec[i] / 1000000, fmin_usec[i] % 1000000,  fmax_usec[i] / 1000000, fmax_usec[i] % 1000000);
        }
        printf("Overall\nAvg: %.2f msec\nMin: %d sec, %d usec\nMax: %d sec, %d usec\n", (avg_msec / numSent), min_usec / 1000000, min_usec % 1000000,  max_usec / 1000000, max_usec % 1000000);

        /*double totalsent = iter * num_dest * file_stat.st_size;
          double avg_gsec = (double)avg_msec / 1000;
          double goodput = totalsent / avg_gsec;
          double ratio = 1000000000 / 8;
          printf("Goodput: %u Bps\n", goodput/ ratio);
          */

        double totalsent = ((double)req_size * num_dest * numSent) / 1000000;

        printf("Total sent: %.2f MB\n", totalsent);
        double total_sec = avg_msec / 1000;
        printf("Total duration: %.2f sec\n", total_sec);

        printf("Goodput: %.2f Mbps\n", 8 * totalsent / total_sec);

        printf("\nHistogram\n");
        for (int i = 0; i < 4; i++) {
            printf("%d - %d\n", i, histogram[i]);
        }
    }

    free(stop_time);
    free(start_time);
    free(dest_port);
    free(src_port);
    free(dest_addr);
    free(indexes);
    free(sockets);
    free(threads);
    free(fmax_usec);
    free(fmin_usec);
    free(favg_usec);

    /*max_usec = 0;
      min_usec = 999999999;
      avg_usec = 0;
      numSent = 0;

      setsockopt(serverfd, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt));

      int i;
      struct timeval tstart, tstop;
      int sec;
      int usec;
      uint gsize;

      for (i = 0; i < iter; i++) {
      printf("Iteration: %d. Sending file.. ", i + 1);
      char buf[100];
      sprintf(buf, "%s\r\n\0", argv[3]);
      write(serverfd, buf, strlen(buf));

    //fd = open(argv[3], O_RDONLY);
    gettimeofday(&tstart, NULL);

    ret = sendfile(serverfd, fd, 0, stat_buf.st_size);

    if (ret == fsize) {
    ret = read(serverfd, &gsize, sizeof(uint));

    int quickack = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &quickack, sizeof(quickack));

    if (ret != sizeof(uint)) {
    printf("Error! size read: %u\n", ret);
    break;
    }

    gettimeofday(&tstop, NULL);

    sec = tstop.tv_sec - tstart.tv_sec;
    usec = tstop.tv_usec - tstart.tv_usec;

    if (usec < 0) {
    usec += 1000000;
    sec--;
    }
    }
    else {
    printf("Error! size sent: %u\n", ret);
    break;
    }
    */
    /*printf("Completed! Duration, sec: %u, usec: %u\n", sec, usec);
      usec += sec * 1000000;
      if (i > 0) {
      numSent++;
      if (usec < min_usec)
      min_usec = usec;
      if (usec > max_usec)
      max_usec = usec;
      avg_usec += usec;
      }
      }

    //close(fd);
    //}

    if (numSent > 0) {
    printf("Files sent: %d\nAvg: %d sec, %d usec\nMin: %d sec, %d usec\nMax: %d sec, %d usec\n", numSent, (avg_usec / numSent) / 1000000, (avg_usec / numSent) % 1000000, min_usec / 1000000, min_usec % 1000000,  max_usec / 1000000, max_usec % 1000000);
    }
    else {
    printf("Error!\n");
    }

    close(serverfd);
    */
    return 0;
    }

void *listen_connection(void *ptr) {
    int index = *((int *)ptr);
    int sock = sockets[index];
    int total = 0;
    int n;
    char buf[MAX_READ];

    memcpy(buf, &l25, sizeof(int));
    memcpy(buf + sizeof(int), &req_size_n, sizeof(uint));

    gettimeofday(&start_time[index], NULL);
    write(sockets[index], buf, sizeof(int) + sizeof(uint));

    do {
        n = read(sock, buf, MAX_READ);

        int quickack = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &quickack, sizeof(quickack));

        if (n > 0) {
            total += n;
            //printf("n: %d, total: %d \n", n, total);
        }

    } while (total < req_size && n > 0);

    if (total == 0) {
        printf(" Error: server closed the connection. Probably requested file not found on the server\n");
        exit(-1);
    }
    else if (total != req_size) {
        printf(" Error: file transfer incomplete!\n");
        exit(-1);
    }
    gettimeofday(&stop_time[index], NULL);

    //printf("Finished! index: %d\n", index);

    return 0;
}
