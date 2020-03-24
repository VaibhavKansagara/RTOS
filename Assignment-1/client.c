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
#include "common.h"

int sockfd;
void handle_sigint(int sig) {
    char buff[500];
    memset(buff, '\0', sizeof(buff));
    char temp;
    printf("exit Y/N: ");
    scanf("%s", &temp);
    strcpy(buff, "exit");
    struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
    strcpy(t_m->buff, buff);
    t_m->t = gettime();
    if (temp == 'Y' || temp == 'y') {
        send(sockfd, t_m, sizeof(struct time_message), 0);
        exit(0);
    }
}

void* send_thread_func(void *fd) {
    // create a signal handler which deals when the client presses ctrl+c
    signal(SIGINT, handle_sigint);

    int sckfd = *((int *)fd);
    char buff[500];
    while(1) {
        memset(buff, '\0', sizeof(buff));
        printf("Enter your message: ");
        fgets(buff, 500, stdin);
        struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
        strcpy(t_m->buff, buff);
        t_m->t = gettime();
        send(sckfd, t_m, sizeof(struct time_message), 0);
    }
}

void* rcv_thread_func(void *fd) {
    int sckfd = *((int *)fd);
    while(1) {
        struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
        recv(sckfd, t_m, sizeof(struct time_message), 0);
        printf("%s", t_m->buff);
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
        int read;
        memset(buff, '\0', sizeof(buff));
        recv(sockfd, buff, sizeof(buff), 0);
        printf("%s\n", buff);
        
        memset(buff, '\0', sizeof(buff));
        recv(sockfd, buff, sizeof(buff), 0);
        printf("%s\n", buff);

        memset(buff, '\0', sizeof(buff));
        scanf("%s", buff);
        send(sockfd, buff, sizeof(buff), 0);

        memset(buff, '\0', sizeof(buff));
        recv(sockfd, buff, sizeof(buff), 0);
        printf("%s\n", buff);

        memset(buff, '\0', sizeof(buff));
        scanf("%s", buff);
        send(sockfd, buff, sizeof(buff), 0);
    }

    // Create two threads - one for recieving messages and one for sending message
    // to the server.
    pthread_t send_threadid;
    pthread_t rcv_threadid;
    pthread_create(&send_threadid, NULL, send_thread_func, &sockfd);
    pthread_create(&rcv_threadid, NULL, rcv_thread_func, &sockfd);

    // join the two threads
    pthread_join(send_threadid, NULL);
    pthread_join(rcv_threadid, NULL);
    close(sockfd);
}
