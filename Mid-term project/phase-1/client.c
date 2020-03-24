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

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/pulseaudio.h>

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define BUFSIZE 512

int voice_chat = 0;


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
    char temp;
    printf("exit Y/N: ");
    scanf("%s", &temp);
    if (temp == 'Y' || temp == 'y') {
        printf("Client exiting\n");
        close(sockfd);
        exit(0);
    }
}

void* send_thread_func(void *fd) {
    if (voice_chat) {
        /* Create the recording stream */
        if (!(s1 = pa_simple_new(NULL, "VOIP", PA_STREAM_RECORD, NULL, "record", &ss1, NULL, NULL, &error1))) {
            fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error1));
            goto finish;
        }
    }

    while(1) {
        char buff[BUFSIZE];
        char buff2[BUFSIZE];

        if (voice_chat) {
            /* Record some data ... */
            if (pa_simple_read(s1, buff2, sizeof(buff2), &error1) < 0) {
                fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error1));
                goto finish;
            }
            write(sockfd, buff2, sizeof(buff2));
        } else {
            printf("Enter your message: ");
            fgets(buff, BUFSIZE, stdin);
            write(sockfd, buff, sizeof(buff));
        }
    }

finish:
    if (s1) pa_simple_free(s1);
}

// Handler for receiver threads
void* rcv_thread_func(void* connfd) {
    int sckfd = *((int *)connfd);

    /* Create a new playback stream */
    if (!(s2 = pa_simple_new(NULL, "VOIP", PA_STREAM_PLAYBACK, NULL, "playback", &ss2, NULL, NULL, &error2))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error2));
        goto finish;
    }

    // Now read the message from the client for infinite time
    // until he quits
    while(1) {
        char buff[BUFSIZE];
        int temp = read(sckfd, buff, sizeof(buff));
        if (temp < 0) {
            handle_error("read");
            break;
        }
        /* ... and play it */
        if (pa_simple_write(s2, buff, sizeof(buff) , &error2) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error2));
            goto finish;
        }
    }

finish:
    if (s2) pa_simple_free(s2);
}

int main(int argc, char* argv[]) {
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
