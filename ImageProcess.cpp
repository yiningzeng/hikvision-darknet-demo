#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "opencv2/opencv.hpp"
#include "MvCameraControl.h"
#include "processor.h"
#include "include/yolo_v2_class.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable>
#include <time.h>

using namespace std;

#define MAX_IMAGE_DATA_SIZE   (40*1024*1024)

static cv::Scalar obj_id_to_color(int obj_id) {
    int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };
    int const offset = obj_id * 123457 % 6;
    int const color_scale = 150 + (obj_id * 123457) % 100;
    cv::Scalar color(colors[offset][0], colors[offset][1], colors[offset][2]);
    color *= color_scale;
    return color;
}

void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names, int current_det_fps = -1, int current_cap_fps = -1)
{
    int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };
    for (auto& i : result_vec) {
        cv::Scalar color = obj_id_to_color(i.obj_id);
        cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 2);
        if (obj_names.size() > i.obj_id) {
            std::string obj_name = obj_names[i.obj_id];
            if (i.track_id > 0) obj_name += " - " + std::to_string(i.track_id);
            cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, 2, 0);
            int const max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
            cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 1, 0), std::max((int)i.y - 30, 0)),
                          cv::Point2f(std::min((int)i.x + max_width, mat_img.cols - 1), std::min((int)i.y, mat_img.rows - 1)),
                          color, CV_FILLED, 8, 0);
            putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 10), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(0, 0, 0), 2);
        }
    }
}

std::vector<std::string> objects_names_from_file(std::string const filename) {
    std::ifstream file(filename);
    std::vector<std::string> file_lines;
    if (!file.is_open()) return file_lines;
    for (std::string line; getline(file, line);) file_lines.push_back(line);
    std::cout << "object names loaded \n";
    return file_lines;
}

// wait for user to input enter to stop grabbing or end the sample program
void PressEnterToExit(void)
{
    int c;
    while ( (c = getchar()) != '\n' && c != EOF );
    fprintf( stderr, "\nPress enter to exit.\n");
    while( getchar() != '\n');
}

bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("%s\n" , "The Pointer of pstMVDevInfoList is NULL!");
        return false;
    }
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        // print current ip and user defined name
        printf("%s %x\n" , "nCurrentIp:" , pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp);                   //��ǰIP
        printf("%s %s\n\n" , "chUserDefinedName:" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);     //�û�������
    }
    else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
        printf("UserDefinedName:%s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    }
    else
    {
        printf("Not support.\n");
    }
    return true;
}


int main()
{
    Detector detector("/home/baymin/daily-work/new-work/ab-darknet-localhost/assets/yolov3-voc-test.cfg", "/home/baymin/daily-work/new-work/ab-darknet-localhost/assets/backup/yolov3-voc_last.weights");
    auto obj_names = objects_names_from_file("/home/baymin/daily-work/new-work/ab-darknet-localhost/assets/voc.names");
    float thresh = 0.1;
    int nRet = MV_OK;

    void* handle = NULL;

    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    // enum device
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet)
    {
        printf("MV_CC_EnumDevices fail! nRet [%x]\n", nRet);
        return -1;
    }
    unsigned int nIndex = 0;
    if (stDeviceList.nDeviceNum > 0)
    {
        for (int i = 0; i < stDeviceList.nDeviceNum; i++)
        {
            printf("[device %d]:\n", i);
            MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
            if (NULL == pDeviceInfo)
            {
                break;
            }
            PrintDeviceInfo(pDeviceInfo);
        }
    }
    else
    {
        printf("Find No Devices!\n");
        return -1;
    }



    // select device and create handle
    nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
    if (MV_OK != nRet)
    {
        printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
        return -1;
    }

    // open device
    nRet = MV_CC_OpenDevice(handle);
    if (MV_OK != nRet)
    {
        printf("MV_CC_OpenDevice fail! nRet [%x]\n", nRet);
        return -1;
    }

    nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 0);
    if (MV_OK != nRet)
    {
        printf("MV_CC_SetTriggerMode fail! nRet [%x]\n", nRet);
        return -1;
    }

    // start grab image
    nRet = MV_CC_StartGrabbing(handle);
    if (MV_OK != nRet)
    {
        printf("MV_CC_StartGrabbing fail! nRet [%x]\n", nRet);
        return -1;
    }

    MV_FRAME_OUT_INFO_EX stImageInfo = {0};
    memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
    unsigned char * pData = (unsigned char *)malloc(sizeof(unsigned char) * MAX_IMAGE_DATA_SIZE);
    unsigned int nDataSize = MAX_IMAGE_DATA_SIZE;
    unsigned char *pDataForRGB = NULL;
    unsigned char *pDataForSaveImage = NULL;

    int a = 0;
    while(a<5){
        nRet = MV_CC_GetOneFrameTimeout(handle, pData, nDataSize, &stImageInfo, 1000);
        if (nRet == MV_OK) {
            printf("Discard Frame \n\n");
        }
        a++;
    }
//    cv::Mat imgShow=cv::imread("screen/1.png");
//    if (imgShow.empty()) {
//        cout << "Error" << endl;
//        return -1;
//    }
//    cv::imshow("camera", imgShow);
//    cv::resizeWindow("camera", 1600, 1200);
//    cv::moveWindow("camera", 1367, -25);
//    printf("show screen one\n");
//    cv::waitKey(500);
//    sleep(1);

    int i = 1;
    while (1) {
        stringstream ss;
        ss << i;
//        cv::Mat imgShow = cv::imread("screen/" + ss.str() + ".png");
//        if (imgShow.empty()) {
//            cout << "Error" << endl;
//            return -1;
//        }
//        cv::imshow("camera", imgShow);
//        cv::resizeWindow("camera", 1600, 1200);
////    setWindowProperty("camera", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
//        cv::moveWindow("camera", 1367, -25);
//        printf("show screen\n");
        string name = "camera/test.bmp";
//        cv::waitKey(50);
//        //usleep(100000);

        nRet = MV_CC_GetOneFrameTimeout(handle, pData, nDataSize, &stImageInfo, 1000);
        if (nRet == MV_OK) {
            printf("Now you GetOneFrame, Width[%d], Height[%d], nFrameNum[%d]\n\n",
                   stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum);

            // image processing
            printf("input 0 to do nothing, 1 to convert RGB, 2 to save as BMP\n");

            pDataForSaveImage = (unsigned char *) malloc(stImageInfo.nWidth * stImageInfo.nHeight * 4 + 2048);
            if (NULL == pDataForSaveImage) {
                break;
            }
            // fill in the parameters of save image
            MV_SAVE_IMAGE_PARAM_EX stSaveParam;
            memset(&stSaveParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));
            // Top to bottom are��
            stSaveParam.enImageType = MV_Image_Bmp;
            stSaveParam.enPixelType = stImageInfo.enPixelType;
            stSaveParam.nBufferSize = stImageInfo.nWidth * stImageInfo.nHeight * 4 + 2048;
            stSaveParam.nWidth = stImageInfo.nWidth;
            stSaveParam.nHeight = stImageInfo.nHeight;
            stSaveParam.pData = pData;
            stSaveParam.nDataLen = stImageInfo.nFrameLen;
            stSaveParam.pImageBuffer = pDataForSaveImage;
            stSaveParam.nJpgQuality = 50;

            nRet = MV_CC_SaveImageEx(&stSaveParam);
            if (MV_OK != nRet) {
                printf("failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
                break;
            }
            FILE *fp = fopen(name.c_str(), "wb");
            if (NULL == fp) {
                printf("fopen failed\n");
                break;
            }
            fwrite(pDataForSaveImage, 1, stSaveParam.nImageLen, fp);
            fclose(fp);
            printf("save image succeed\n");
            cv::Mat img = cv::imread(name);
            cv::Mat imgOut;
            cv::resize(img, imgOut, cv::Size(stImageInfo.nWidth / 4, stImageInfo.nHeight / 4));


            cout << ">>>>>>>>>>>>>>>>>>>>>>>>>> \n" << detector.get_net_width() << endl;
            image_t aa = detector.load_image(name);
            std::vector<bbox_t> result_vec = detector.detect(aa);
//            auto start = std::chrono::steady_clock::now(); // 开始时间
//            std::vector<bbox_t> result_vec = detector.detect(img,0.1); // 检测函数
//            auto end = std::chrono::steady_clock::now(); // 结束时间
//            std::chrono::duration<double> spent = end - start; // 检测时间
//            std::cout << " Time: " << spent.count() << " sec \n<<<<<<<<<<<<<<<<<<<<<<<<\n";

            draw_boxes(img, result_vec, obj_names); // 最后把缺陷框描绘在图像上
//            cv::imwrite("C:/Users/Administrator/source/repos/test/x64/Release/yunsheng/result/" + filesname[i], mat_img); // 保存检测的结果图片
            if (result_vec.size()>0){
                cout << "缺陷大于0\n" << endl;
                cv::imwrite("camera/result.jpg", img); // 保存检测的结果图片
            }
            cv::imshow("test", img);
            cv::waitKey(10);
//            if (i > 1) {
//                send_input(imgOut);
//            }
            printf("send image success\n");
//                cv::waitKey(100);
        }

        i++;
        printf("i: %x\n", i);
//            cv::Mat img_decode;
//            img_decode = cv::imdecode(data, 1);
//            cv::imshow("camera",img_decode);
//            cv::waitKey(3000);
    }


    // end grab image
    nRet = MV_CC_StopGrabbing(handle);
    if (MV_OK != nRet)
    {
        printf("MV_CC_StopGrabbing fail! nRet [%x]\n", nRet);
        return -1;
    }

    // destroy handle
    nRet = MV_CC_DestroyHandle(handle);
    if (MV_OK != nRet)
    {
        printf("MV_CC_DestroyHandle fail! nRet [%x]\n", nRet);
        return -1;
    }

    if (pData)
    {
        free(pData);
        pData = NULL;
    }
    if (pDataForRGB)
    {
        free(pDataForRGB);
        pDataForRGB = NULL;
    }
    if (pDataForSaveImage)
    {
        free(pDataForSaveImage);
        pDataForSaveImage = NULL;
    }

    PressEnterToExit();
    printf("exit\n");
    return 0;
}
