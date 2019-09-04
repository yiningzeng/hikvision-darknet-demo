#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "opencv2/opencv.hpp"
#include "MvCameraControl.h"
#include "processor.h"

using namespace std;

#define MAX_IMAGE_DATA_SIZE   (40*1024*1024)

// 等待用户输入enter键来结束取流或结束程序
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
		// 打印当前相机ip和用户自定义名字
		// print current ip and user defined name
        printf("%s %x\n" , "nCurrentIp:" , pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp);                   //当前IP
        printf("%s %s\n\n" , "chUserDefinedName:" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);     //用户定义名
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
    int nRet = MV_OK;

    void* handle = NULL;

    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    // 枚举设备
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



    // 选择设备并创建句柄
	// select device and create handle
    nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
    if (MV_OK != nRet)
    {
        printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
        return -1;
    }
	
    // 打开设备
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
	
    // 开始取流
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
	cv::Mat imgShow=cv::imread("screen/1.png");
	if (imgShow.empty()) {
	    cout << "Error" << endl;
	        return -1;
	}
	cv::imshow("camera", imgShow);
	cv::resizeWindow("camera", 1600, 1200);
	cv::moveWindow("camera", 1367, -25);
	printf("show screen one\n");
	cv::waitKey(500);
	sleep(1);

	int i = 1;
	while (1) {
        stringstream ss;
        ss << i;
        if (i < 9) {
            cv::Mat imgShow = cv::imread("screen/" + ss.str() + ".png");
            if (imgShow.empty()) {
                cout << "Error" << endl;
                return -1;
            }
            cv::imshow("camera", imgShow);
            cv::resizeWindow("camera", 1600, 1200);
//    setWindowProperty("camera", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
            cv::moveWindow("camera", 1367, -25);
            printf("show screen\n");
            string name = "camera/image" + ss.str() + ".bmp";
            cv::waitKey(50);
            //usleep(100000);

            nRet = MV_CC_GetOneFrameTimeout(handle, pData, nDataSize, &stImageInfo, 1000);
            if (nRet == MV_OK) {
                printf("Now you GetOneFrame, Width[%d], Height[%d], nFrameNum[%d]\n\n",
                       stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum);

                // 处理图像
                // image processing
                printf("input 0 to do nothing, 1 to convert RGB, 2 to save as BMP\n");

                pDataForSaveImage = (unsigned char *) malloc(stImageInfo.nWidth * stImageInfo.nHeight * 4 + 2048);
                if (NULL == pDataForSaveImage) {
                    break;
                }
                // 填充存图参数
                // fill in the parameters of save image
                MV_SAVE_IMAGE_PARAM_EX stSaveParam;
                memset(&stSaveParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));
                // 从上到下依次是：输出图片格式，输入数据的像素格式，提供的输出缓冲区大小，图像宽，
                // 图像高，输入数据缓存，输出图片缓存，JPG编码质量
                // Top to bottom are：
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
                if (i > 1) {
                    send_input(imgOut);
                }
                printf("send image success\n");
//                cv::waitKey(100);
            }
        } else if (i == 9) {
            cv::Mat img = get_output();
//                cv::imshow("camera",img);
//                cv::waitKey(0);
            stringstream ss;
            ss << stImageInfo.nFrameNum;
            cv::imwrite("result/" + ss.str() + ".bmp", img);
            i = 0;
            break;
        }
        i++;
        printf("i: %x\n", i);
//            cv::Mat img_decode;
//            img_decode = cv::imdecode(data, 1);
//            cv::imshow("camera",img_decode);
//            cv::waitKey(3000);
    }

	
	// 停止取流
	// end grab image
    nRet = MV_CC_StopGrabbing(handle);
    if (MV_OK != nRet)
    {
        printf("MV_CC_StopGrabbing fail! nRet [%x]\n", nRet);
        return -1;
    }

    // 销毁句柄
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
