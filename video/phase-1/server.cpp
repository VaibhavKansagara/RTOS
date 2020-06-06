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

#include "opencv2/opencv.hpp"
#include <iostream>
#include <sys/ioctl.h>
#include <net/if.h>
#include "rsa.h"

using namespace cv;
int capDev = 0;
VideoCapture cap(capDev); // open the default camera

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

int sockfd,sockfd1;

// initially voice chat is disabled.
int voice_chat = 0;
char key;

// We need client's public key to encrypt messages
// client's public key
struct public_key_class client_pub[1];

// server's public and private key
struct private_key_class* server_priv;
struct public_key_class* server_pub;

void handle_sigint(int sig) {
    char temp;
    printf("exit Y/N: ");
    scanf("%s", &temp);
    if (temp == 'Y' || temp == 'y') {
        printf("Server exiting\n");
        cap.release();
        close(sockfd);
        exit(0);
    }
}

void* exchange_public_key(void* fd) {
    int sckfd = *((int *)fd);
    // std::cout << server_pub->modulus << " " << server_pub->exponent <<std::endl;
    int bytes = 0;
    // send server's public key
    if ((bytes = send(sckfd, server_pub, sizeof(struct public_key_class), 0)) < 0){
        std::cerr << "Not able to send server's public key" << std::endl;
        exit(1);
    }
    //receive client's public key
    if ((bytes = recv(sckfd, client_pub, sizeof(struct public_key_class), 0)) < 0){
        std::cerr << "Not able to receive client's public key" << std::endl;
        exit(1);
    }

    // std::cout << client_pub->modulus << " " << client_pub->exponent <<std::endl;
}

void* send_video_thread_func(void *fd) {
    int sckfd = *((int *)fd);
    
    Mat img, imgGray;
    img = Mat::zeros(240 , 320, CV_8UC1);   
     //make it continuous
    if (!img.isContinuous()) {
        img = img.clone();
    }

    int imgSize = img.total() * img.elemSize();
    int bytes = 0;
    int key;

    //make img continuos
    if ( ! img.isContinuous() ) { 
          img = img.clone();
          imgGray = img.clone();
    }

    std::cout << "Image Size:" << imgSize << std::endl;
    cap.set(CV_CAP_PROP_FOURCC, CV_FOURCC('D', 'I', 'V', 'X'));
    cap.set(CV_CAP_PROP_FRAME_HEIGHT,240);
    cap.set(CV_CAP_PROP_FRAME_WIDTH,320);
    while(1) {
        char buff[BUFSIZE];
        char buff2[BUFSIZE];
        uchar videobuff[imgSize];

        if (voice_chat) {
            // video code
            // get a frame from camera 
            cap >> img;
        
            //do video processing here 
            cvtColor(img, imgGray, CV_BGR2GRAY);

            // memcpy(videobuff, imgGray.data, imgSize);
            long long *encrypted = rsa_encrypt(imgGray.data, sizeof(videobuff), client_pub);
            // std::cout << 8*imgSize << " " << sizeof(long long) * sizeof(videobuff) << std::endl;
            //send processed image
            if ((bytes = send(sckfd, encrypted, 8*imgSize, 0)) < 0){
                 std::cerr << "bytes = " << bytes << std::endl;
                 break;
            }
            // if ((bytes = send(sckfd, videobuff, imgSize, 0)) < 0){
            //      std::cerr << "bytes = " << bytes << std::endl;
            //      break;
            // }
        } else {
            printf("Enter your message: ");
            fgets(buff, BUFSIZE, stdin);
            write(sckfd, buff, sizeof(buff));
        }
    }
}

void* send_audio_thread_func(void *fd) {
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

void* rcv_video_thread_func(void* connfd) {
    int sckfd = *((int *)connfd);

    Mat img;
    img = Mat::zeros(480 , 640, CV_8UC1);
    int imgSize = img.total() * img.elemSize();
    uchar *iptr = img.data;
    int bytes = 0;
    //make img continuos
    if ( ! img.isContinuous() ) { 
          img = img.clone();
    }
    std::cout << "Image Size:" << imgSize << std::endl;
    namedWindow("CV Video Server",1);

    // Now read the message from the client for infinite time
    // until he quits
    while(key != 'q') {
        if ((bytes = recv(sckfd, iptr, imgSize , MSG_WAITALL)) == -1) {
            std::cerr << "recv failed, received bytes = " << bytes << std::endl;
        }
        cv::imshow("CV Video Server", img);
        key = cv::waitKey(10);
    }
}


// Handler for receiver threads
void* rcv_audio_thread_func(void* connfd) {
    int sckfd = *((int *)connfd);

    /* Create a new playback stream */
    if (!(s2 = pa_simple_new(NULL, "VOIP", PA_STREAM_PLAYBACK, NULL, "playback", &ss2, NULL, NULL, &error2))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error2));
        goto finish;
    }

    // Now read the message from the client for infinite time
    // until he quits
    while(key != 'q') {
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
    int portno1;
} arg;


// Main thread will listen to the clients and form groups and
// will be responsible for creating new client threads and
// assigning them into their respective groups and also will be
// responsible for deleting the clients.
void* main_thread_func(void* argv) {
    int connfd,connfd1;
    struct sockaddr_in serveraddr,cli;
    struct sockaddr_in serveraddr1,cli1;
    struct argument* args = (struct argument*) argv;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1){
        printf("socket creation failed\n");
    }   
    else printf("socket successfully created\n");

    sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd1 == -1){
        printf("socket1 creation failed\n");
    }   
    else printf("socket1 successfully created\n");

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(args->sddr);
    serveraddr.sin_port = htons(args->portno);

    bzero(&serveraddr1, sizeof(serveraddr1));
    serveraddr1.sin_family = AF_INET;
    serveraddr1.sin_addr.s_addr = inet_addr(args->sddr);
    serveraddr1.sin_port = htons(args->portno1);

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


    if ((bind(sockfd1, (struct sockaddr*)&serveraddr1, sizeof(serveraddr1))) != 0) {
        printf("socket bindings1 failed\n");
        exit(0);
    }
    else printf("socket binding1 succesfull\n");

    if ((listen(sockfd1, 10)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server1 listening..\n");

    socklen_t addrsize = sizeof(cli);
    if ((connfd = accept(sockfd, (struct sockaddr*)&cli, &(addrsize))) >= 0) {
        printf("server accepted the client\n");
    } else printf("server accept failed\n");

    socklen_t addrsize1 = sizeof(cli1);
    if ((connfd1 = accept(sockfd1, (struct sockaddr*)&cli1, &(addrsize1))) >= 0) {
        printf("server1 accepted the client\n");
    } else printf("server1 accept failed\n");


    pthread_t exchange_id;
    pthread_create(&exchange_id, NULL, exchange_public_key, &connfd);    
    pthread_join(exchange_id, NULL);

    // pthread_t rcv_video_thread_id;
    // pthread_t rcv_audio_thread_id;
    pthread_t send_video_thread_id;
    pthread_t send_audio_thread_id;

    // if(pthread_create(&rcv_video_thread_id, NULL, rcv_video_thread_func, &connfd) == 0) {
    //     printf("Receive video thread created successfull\n");
    // } else printf("Receive video thread failed to create\n");

    // if(pthread_create(&rcv_audio_thread_id, NULL, rcv_audio_thread_func, &connfd1) == 0) {
    //     printf("Receive audio thread created successfull\n");
    // } else printf("Receive audio thread failed to create\n");

    if(pthread_create(&send_video_thread_id, NULL, send_video_thread_func, &connfd) == 0) {
        printf("Send video thread created successfull\n");
    } else printf("Send video thread failed to create\n");

    if(pthread_create(&send_audio_thread_id, NULL, send_audio_thread_func, &connfd1) == 0) {
        printf("Send audio thread created successfull\n");
    } else printf("Send audio thread failed to create\n");


    // pthread_join(rcv_video_thread_id, NULL);
    // pthread_join(rcv_audio_thread_id, NULL);
    pthread_join(send_video_thread_id, NULL);
    pthread_join(send_audio_thread_id, NULL);

    // close the socket
    close(sockfd);
    close(sockfd1);    
}

int main(int argc, char* argv[]) {
    server_pub = (struct public_key_class*)malloc(sizeof(struct public_key_class));
    server_priv = (struct private_key_class*)malloc(sizeof(struct private_key_class));
    rsa_gen_keys(server_pub, server_priv, PRIME_SOURCE_FILE);
    signal(SIGINT, handle_sigint);
    pthread_t main_thread_id;

    arg.sddr = argv[1];
    arg.portno = atoi(argv[2]);
    arg.portno1 = atoi(argv[3]);
    voice_chat = atoi(argv[4]);

    pthread_create(&main_thread_id, NULL, main_thread_func, (void *) &arg);
    pthread_join(main_thread_id, NULL);
}
