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

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/pulseaudio.h>

#define BUFSIZE 1024

int voice_chat = 0;

// Create two threads - one for recieving messages and one for sending message
// to the server.
pthread_t send_threadid;
pthread_t rcv_threadid;


/* The sample type to use */
static const pa_sample_spec ss1 = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};

static const pa_sample_spec ss2 = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};
pa_simple *s1 = NULL;
pa_simple *s2 = NULL;
int ret = 1;
int error1;
int error2;

int sockfd;

void handle_sigint(int sig) {
    char buff[BUFSIZE];
    memset(buff, '\0', sizeof(buff));
    char temp[5];
    printf("exit Y/N: ");
    // scanf("%s", temp);
    fgets(temp, 5, stdin);
    strcpy(buff, "exit");
    struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
    strcpy(t_m->buff, buff);
    t_m->t = gettime();
    if (temp[0] == 'Y' || temp[0] == 'y') {
        send(sockfd, t_m, sizeof(struct time_message), 0);
        exit(0);
    }
}

void* send_thread_func(void *fd) {
    if (voice_chat) {
        /* Create the recording stream */
        if (!(s1 = pa_simple_new(NULL, "VOICE_CHAT", PA_STREAM_RECORD, NULL, "record", &ss1, NULL, NULL, &error1))) {
            fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error1));
            goto finish;
        }
    }

    int sckfd = *((int *)fd);
    while(1) {
        char buff[BUFSIZE];
        uint8_t buff2[BUFSIZE];
        if (voice_chat) {
            /* Record some data ... */
            if (pa_simple_read(s1, buff2, sizeof(buff2), &error1) < 0) {
                fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error1));
                goto finish;
            }
        } else {
            // printf("Enter your message: ");
            // scanf("%s", buff);
            fgets(buff, BUFSIZE, stdin);
            // scanf("%[^\n]s", buff);
        }

        struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
        strcpy(t_m->buff, buff);
        if (voice_chat)
            memcpy(t_m->voice_buff, buff2, sizeof(t_m->voice_buff));
        t_m->t = gettime();
        send(sckfd, t_m, sizeof(struct time_message), 0);
    }

finish:
    if (s1) pa_simple_free(s1);
}

void* rcv_thread_func(void *fd) {
    int sckfd = *((int *)fd);

    /* Create a new playback stream */
    if (!(s2 = pa_simple_new(NULL, "VOICE_CHAT", PA_STREAM_PLAYBACK, NULL, "playback", &ss2, NULL, NULL, &error2))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error2));
        goto finish;
    }

    while(1) {
        struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
        recv(sckfd, t_m, sizeof(struct time_message), 0);
        if (strcmp(t_m->buff, "exit: exit") == 0) {
            exit(0);
        }
        if (voice_chat) {
            // printf("levevlej\n");
            /* ... and play it */
            if (pa_simple_write(s2, t_m->voice_buff, (size_t) sizeof(t_m->voice_buff), &error2) < 0) {
                fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error2));
                goto finish;
            }
        } else {
            printf("%s", t_m->buff);
        }
    }

    /* Make sure that every single sample was played */
    if (pa_simple_drain(s2, &error2) < 0) {
        fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error2));
        goto finish;
    }

finish:
    if (s2) pa_simple_free(s2);
}

int main(int argc, char* argv[]) {
    // create a signal handler which deals when the client presses ctrl+c
    signal(SIGINT, handle_sigint);

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

    voice_chat = atoi(argv[3]);

    if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0) {
        printf("connection with the server failed\n");
        exit(0);
    } else {
        printf("connection established successfully\n");

        char buff[BUFSIZE];
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

    pthread_create(&send_threadid, NULL, send_thread_func, &sockfd);
    pthread_create(&rcv_threadid, NULL, rcv_thread_func, &sockfd);

    // join the two threads
    pthread_join(send_threadid, NULL);
    pthread_join(rcv_threadid, NULL);
    close(sockfd);
}
