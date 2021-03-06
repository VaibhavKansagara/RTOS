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
#include "rsa.h"

using namespace cv;

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define BUFSIZE 512

int voice_chat = 0;
char key;

// int capDev = 0;
// VideoCapture cap(capDev);

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

// We need server's public key to encrypt messages
// client's public key
struct public_key_class server_pub[1];

// client's public and private key
struct private_key_class client_priv[1];
struct public_key_class client_pub[1];

void handle_sigint(int sig) {
    char buff[BUFSIZE];
    memset(buff, '\0', sizeof(buff));
    char temp;
    printf("exit Y/N: ");
    scanf("%s", &temp);
    if (temp == 'Y' || temp == 'y') {
        printf("Client exiting\n");
        close(sockfd);
        close(sockfd1);
        exit(0);
    }
}

void* exchange_public_key(void* connfd) {
    int sckfd = *((int *)connfd);
    // std::cout << client_pub->modulus << " " << client_pub->exponent <<std::endl;

    int bytes = 0;
    // receive server's public key
    if ((bytes = recv(sckfd, server_pub, sizeof(struct public_key_class), 0)) < 0){
        std::cerr << "Not able to receive server's public key" << std::endl;
        exit(1);
    }
    // send client's public key
    if ((bytes = send(sckfd, client_pub, sizeof(struct public_key_class), 0)) < 0){
        std::cerr << "Not able to send client's public key" << std::endl;
        exit(1);
    }

    // std::cout << server_pub->modulus << " " << server_pub->exponent <<std::endl;

}

void* send_video_thread_func(void *fd) {
    int sckfd = *((int *)fd);
    
    Mat img, imgGray;
    img = Mat::zeros(480 , 640, CV_8UC1);   
     //make it continuous
    if (!img.isContinuous()) {
        img = img.clone();
        imgGray = img.clone();
    }

    int imgSize = img.total() * img.elemSize();
    int bytes = 0;
    int key;

    std::cout << "Image Size:" << imgSize << std::endl;
    // cap.set(CV_CAP_PROP_FOURCC, CV_FOURCC('D', 'I', 'V', 'X'));
    while(1) {
        char buff[BUFSIZE];
        char buff2[BUFSIZE];

        if (voice_chat) {
            // video code
            // get a frame from camera 
            // cap >> img;
        
            //do video processing here 
            cvtColor(img, imgGray, CV_BGR2GRAY);

            std::vector<uchar> buff_v;//buffer for encoding
            std::vector<int> param(2);
            param[0] = cv::IMWRITE_JPEG_QUALITY;
            param[1] = 80;//default(95) 0-100
            cv::imencode(".jpg", imgGray, buff_v, param);

            int size_buff = buff_v.size();

            if ((bytes = send(sckfd, &size_buff, sizeof(int), 0)) < 0){
                 std::cerr << "Size of the buffer not sent correctly"<< std::endl;
                 break;
            }
            
            long long *encrypted = rsa_encrypt(buff_v, size_buff, server_pub);
            //send processed image
            if ((bytes = send(sckfd, encrypted, 8 * size_buff, 0)) < 0){
                 std::cerr << "bytes = " << bytes << std::endl;
                 break;
            }
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

// Handler for receiver threads
void* rcv_video_thread_func(void* connfd) {
    int sckfd = *((int *)connfd);

    int bytes = 0;
    namedWindow("CV Video Server",1);

    // Now read the message from the client for infinite time
    // until he quits
    while(key != 'q') {
        int buff_size;
        if ((bytes = recv(sckfd, &buff_size, sizeof(int) , 0)) == -1) {
            std::cerr << "recv failed, received bytes = " << bytes << std::endl;
        }

        long long encrypted[buff_size];
        if ((bytes = recv(sckfd, encrypted, 8 * buff_size, MSG_WAITALL)) == -1) {
            std::cerr << "recv failed, received bytes = " << bytes << std::endl;
        }
        uchar *decrypted = rsa_decrypt(encrypted, 8 * buff_size, client_priv);

        std::vector<uchar> decrypted_vec(buff_size);
        for (int i=0;i<buff_size;i++) {
            decrypted_vec[i] = decrypted[i];
        }

        Mat resultimage;
        resultimage = cv::imdecode(decrypted_vec, IMREAD_GRAYSCALE);
        cv::imshow("CV Video Server", resultimage);
        key = cv::waitKey(10);
    }
}

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
        if (temp < sizeof(buff)) {
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
    rsa_gen_keys(client_pub, client_priv, PRIME_SOURCE_FILE);
    struct sockaddr_in serveraddr;
    struct sockaddr_in serveraddr1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Socket creation failed\n");
        exit(0);
    } else {
        printf("Socket successfully created\n");
    }

    sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd1 == -1) {
        printf("Socket1 creation failed\n");
        exit(0);
    } else {
        printf("Socket1 successfully created\n");
    }

    memset(&serveraddr,'\0',sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port = htons(atoi(argv[2]));

    memset(&serveraddr1,'\0',sizeof(serveraddr1));
    serveraddr1.sin_family = AF_INET;
    serveraddr1.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr1.sin_port = htons(atoi(argv[3]));

    voice_chat = atoi(argv[4]);

    if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0) {
        printf("connection with the server failed\n");
        exit(0);
    } else {
        printf("connection established successfully\n");
    }

    if (connect(sockfd1, (struct sockaddr*)&serveraddr1, sizeof(serveraddr1)) != 0) {
        printf("connection1 with the server failed\n");
        exit(0);
    } else {
        printf("connection1 established successfully\n");
    }

    pthread_t exchange_id;
    pthread_create(&exchange_id, NULL, exchange_public_key, &sockfd);    
    pthread_join(exchange_id, NULL);

    // Create two threads - one for recieving messages and one for sending message
    // to the server.
    // pthread_t send_video_threadid;
    // pthread_t send_audio_threadid;
    pthread_t rcv_video_threadid;
    pthread_t rcv_audio_threadid;

    // pthread_create(&send_video_threadid, NULL, send_video_thread_func, &sockfd);
    // pthread_create(&send_audio_threadid, NULL, send_audio_thread_func, &sockfd1);
    pthread_create(&rcv_video_threadid, NULL, rcv_video_thread_func, &sockfd);
    pthread_create(&rcv_audio_threadid, NULL, rcv_audio_thread_func, &sockfd1);

    // join the two threads
    // pthread_join(send_video_threadid, NULL);
    // pthread_join(send_audio_threadid, NULL);
    pthread_join(rcv_video_threadid, NULL);
    pthread_join(rcv_audio_threadid, NULL);
    close(sockfd);
    close(sockfd1);
}
