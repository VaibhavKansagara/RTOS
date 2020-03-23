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

/* The Sample format to use */
static const pa_sample_spec ss = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};

pa_simple *s = NULL;
int ret = 1;
int error;

// initially voice chat is disabled.
int voice_chat = 0;

// Handler for client threads
void* client_thread(void* connfd) {
    int sckfd = *((int *)connfd);

    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, "VOIP", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
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
        if (pa_simple_write(s, buff, sizeof(buff) , &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
    }

finish:
    if (s) pa_simple_free(s);
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
    int sockfd, connfd, len;
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

    if ((listen(sockfd, 100)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n");

    pthread_t client_thread_ids[100];
    int i = 0;
    while(1) {
        socklen_t addrsize = sizeof(cli);
        if ((connfd = accept(sockfd, (struct sockaddr*)&cli, &(addrsize))) >= 0) {
            printf("server accepted the client\n");
        } else printf("server accept failed\n");
        // Now create the thread for each client request accepted
        if(pthread_create(&client_thread_ids[i++], NULL, client_thread, &connfd) == 0) {
            printf("Thread created successfull\n");
        } else printf("Thread failed to create\n");
    }

    // join all the threads before terminating the main thread.
    for(int j = 0; j < i; j++) {
        pthread_join(client_thread_ids[j], NULL);
    }

    // close the socket
    close(sockfd);
}

int main(int argc, char* argv[]) {
    pthread_t main_thread_id;

    arg.sddr = argv[1];
    arg.portno = atoi(argv[2]);
    voice_chat = atoi(argv[3]);

    pthread_create(&main_thread_id, NULL, main_thread_func, (void *) &arg);
    pthread_join(main_thread_id, NULL);
}
