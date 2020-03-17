#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>
#include<fcntl.h> // for open
#include<unistd.h> // for close
#include<pthread.h>
#include<signal.h>
#include"common.h"

int sockfd;
long long total_delay = 0, n = 0;

void handle_sigint(int sig) {
    FILE *filepointer = fopen("test_results.txt", "a");
    fprintf(filepointer, "%lf\n", ((double)total_delay)/(double)n);
    fclose(filepointer);
    char buff[10];
    memset(buff, '\0', sizeof(buff));
    strcpy(buff, "exit");
    struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
    t_m->t = gettime();
    strcpy(t_m->buff, buff);
    send(sockfd, t_m, sizeof(struct time_message), 0);
    exit(0);
}


void* rcv_thread_func(void *fd) {
    int sckfd = *((int *)fd);
    while(1) {
        struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
        recv(sckfd, t_m, sizeof(struct time_message), 0);
        struct t_format curr_time = gettime();
        long long delay = abs(timediff(t_m->t, curr_time));
        if (n < 100) {
            total_delay += delay;
            n++;
        }
        usleep(1000);
    }
}


int main(int argc, char* argv[]) {
    struct sockaddr_in serveraddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Socket creation failed\n");
        exit(0);
    } else {
        printf("Socket successfully created\n");
    }

    memset(&serveraddr,'\0',sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port = htons(atoi(argv[2]));

    if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0) {
        printf("connection with the server failed\n");
        exit(0);
    } else {
        printf("connection established successfully\n");

        char buff[500];
        memset(buff, '\0', sizeof(buff));
        recv(sockfd, buff, sizeof(buff), 0);
        printf("%s\n", buff);
        
        memset(buff, '\0', sizeof(buff));
        recv(sockfd, buff, sizeof(buff), 0);
        printf("%s\n", buff);

        send(sockfd, argv[3], sizeof(argv[3]), 0);

        memset(buff, '\0', sizeof(buff));
        recv(sockfd, buff, sizeof(buff), 0);
        printf("%s\n", buff);

        send(sockfd, argv[4], sizeof(argv[4]), 0);
    }

    pthread_t rcv_threadid;
    pthread_create(&rcv_threadid, NULL, rcv_thread_func, &sockfd);
    signal(SIGINT, handle_sigint);
    pthread_join(rcv_threadid, NULL);
    close(sockfd);
}
