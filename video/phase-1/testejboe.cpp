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
using namespace std;
int capDev = 0;
VideoCapture cap(capDev); // open the default camera

int main() {
    vector<uchar> vt(5);
    cout << sizeof(vt) << endl << endl;
	cout << CV_MAJOR_VERSION << endl;
	cout << CV_MINOR_VERSION << endl;
	Mat img, imgGray;
    img = Mat::zeros(480 , 640, CV_8UC1);

	//make it continuous
    if (!img.isContinuous()) {
        img = img.clone();
        imgGray = img.clone();
    }
    int imgSize = img.total() * img.elemSize();
    std::cout << "Image Size:" << imgSize << std::endl;
    cap >> img;
	
	// cvtColor(img, imgGray, CV_BGR2GRAY);
	std::vector<uchar> buff;//buffer for coding
    std::vector<int> param(2);
    param[0] = cv::IMWRITE_JPEG_QUALITY;
    param[1] = 80;//default(95) 0-100
    cv::imencode(".jpg", img, buff, param);
    cout << buff.size() << endl;

    Mat resultimage;
    resultimage = cv::imdecode(buff, IMREAD_GRAYSCALE);
    namedWindow("Image",1);
    char key;
    cv::imshow("Image", resultimage);
    key = cv::waitKey(1000);
}