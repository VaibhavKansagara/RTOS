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

// initially voice chat is disabled.
int voice_chat = 0;


void handle_sigint(int sig) {
    char buff[BUFSIZE];
    memset(buff, '\0', sizeof(buff));
    char temp;
    printf("exit Y/N: ");
    scanf("%s", &temp);
    if (temp == 'Y' || temp == 'y') {
        printf("Server exiting\n");
        close(sockfd);
        exit(0);
    }
}

void* send_thread_func(void *fd) {
    int sckfd = *((int *)fd);
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
            write(sckfd, buff2, sizeof(buff2));
        } else {
            printf("Enter your message: ");
            fgets(buff, BUFSIZE, stdin);
            write(sckfd, buff, sizeof(buff));
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

struct argument {
    char* sddr;
    int portno;
} arg;
    
// Main thread will listen to the clients and form groups and
// will be responsible for creating new client threads and
// assigning them into their respective groups and also will be
// responsible for deleting the clients.
void* main_thread_func(void* argv) {
    int connfd, len;
    struct sockaddr_in serveraddr,cli;
    struct argument* args = (struct argument*) argv;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1){
        printf("socket creation failed\n");
    }   
    else printf("socket successfully created\n");

    // memset(&serveraddr,0,sizeof(serveraddr));
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(args->sddr);
    serveraddr.sin_port = htons(args->portno);

    if ((bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) != 0) {
        printf("socket bindings failed\n");
        exit(0);
    }
    else printf("socket binding succesfull\n");

    if ((listen(sockfd, 10)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n");

    pthread_t rcv_thread_id;
    pthread_t send_thread_id;
    int i = 0;

    socklen_t addrsize = sizeof(cli);
    if ((connfd = accept(sockfd, (struct sockaddr*)&cli, &(addrsize))) >= 0) {
        printf("server accepted the client\n");
    } else printf("server accept failed\n");

    
    if(pthread_create(&rcv_thread_id, NULL, rcv_thread_func, &connfd) == 0) {
        printf("Receive thread created successfull\n");
    } else printf("Receive thread failed to create\n");

    if(pthread_create(&rcv_thread_id, NULL, send_thread_func, &connfd) == 0) {
        printf("Send thread created successfull\n");
    } else printf("Send thread failed to create\n");

    pthread_join(rcv_thread_id, NULL);
    pthread_join(send_thread_id, NULL);

    // close the socket
    close(sockfd);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);
    pthread_t main_thread_id;

    arg.sddr = argv[1];
    arg.portno = atoi(argv[2]);
    voice_chat = atoi(argv[3]);

    pthread_create(&main_thread_id, NULL, main_thread_func, (void *) &arg);
    pthread_join(main_thread_id, NULL);
}
