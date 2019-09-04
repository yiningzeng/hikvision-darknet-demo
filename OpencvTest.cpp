#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "opencv2/opencv.hpp"
#include "processor.h"

using namespace std;

int main()
{
    int i = 2;
    while (1){
        stringstream ss;
        ss<<i;
        if (i<17){
            string name ="camera/image" + ss.str() + ".bmp";
            cv::Mat img = cv::imread(name);
            cv::Mat imgOut;
            cv::resize(img, imgOut, cv::Size(3840 / 4, 2748 / 4));
            send_input(imgOut);
            printf("send image\n");
        } else if(i == 18){
            cv::Mat img = get_output();
            cv::imwrite("result/" + ss.str() + ".bmp", img);
            i=0;
            break;
        }
        i++;
    }
    return 0;
}
