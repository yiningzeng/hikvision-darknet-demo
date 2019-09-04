
#ifndef __MV_DEVICE_BASE_H__
#define __MV_DEVICE_BASE_H__

#include "MvInclude.h"

namespace MvCamCtrl
{

    interface   IMvDevice
    {

        // 打开设备
        virtual int     Open(unsigned int nAccessMode = MV_ACCESS_Exclusive, unsigned short nSwitchoverKey = 0)    = 0;


        // 关闭设备
        virtual int     Close()                                 = 0;


        // 判断设备的状态，仅当返回false时，可打开设备
        virtual bool    IsOpen()                                = 0;


        // 开启抓图
        virtual int     StartGrabbing()                         = 0;


        // 停止抓图
        virtual int     StopGrabbing()                          = 0;


        // 获取设备信息
        virtual int     GetDeviceInfo(MV_CC_DEVICE_INFO&)       = 0;


        /** @fn     GetGenICamXML(unsigned char* pData, unsigned int nDataSize, unsigned int* pnDataLen)
         *  @brief  获取设备的XML文件
         *  @param  pData           [IN][OUT]   - 待拷入的缓存地址
                    nDataSize       [IN]        - 缓存大小
                    pnDataLen       [OUT]       - xml 文件数据长度
         *  
         *  @return 成功，返回MV_OK；失败，返回错误码
         *  @note   当pData为NULL或nDataSize比实际的xml文件小时，不拷贝数据，由pnDataLen返回xml文件大小；
         *          当pData为有效缓存地址，且缓存足够大时，拷贝完整数据，并由pnDataLen返回xml文件大小。
         */
        virtual int     GetGenICamXML(unsigned char* pData, unsigned int nDataSize, unsigned int* pnDataLen)        = 0;


        /** @fn     GetOneFrame(unsigned char * pData , unsigned int nDataSize, MV_FRAME_OUT_INFO* pFrameInfo)
         *  @brief  获取一帧图像数据
         *  @param  pData           [IN][OUT]   - 数据指针
                    nDataLen        [IN]        - 数据长度
                    pFrameInfo      [OUT]       - 输出的帧信息
         *  
         *  @return 成功，返回MV_OK；失败，返回错误码
         */
        virtual int     GetOneFrame(unsigned char * pData , unsigned int nDataSize, MV_FRAME_OUT_INFO* pFrameInfo)  = 0;


        // 获取GenICam使用的传输层代理类
        virtual TlProxy     GetTlProxy()                                                                            = 0;

        virtual ~IMvDevice( void ){};
    };


    interface   IDeviceFactory
    {

        // 枚举子网内，指定的传输协议对应的所有设备
        virtual int EnumDevices( unsigned int nTLayerType , MV_CC_DEVICE_INFO_LIST& stDevList )     = 0;


        // 创建设备代理类
        virtual IMvDevice* CreateDevice( const MV_CC_DEVICE_INFO& device )                          = 0;


        // 销毁指定设备的内部资源
        virtual int DestroyDevice( IMvDevice* )                                                     = 0;


        // 判断指定的设备是否可以访问
        virtual bool IsDeviceAccessible( const MV_CC_DEVICE_INFO& deviceInfo)                       = 0;
    };

    

}

#endif /* __MV_DEVICE_BASE_H__ */
