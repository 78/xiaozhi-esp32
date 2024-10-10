#ifndef __AVIFILE_H
#define __AVIFILE_H

#include "avi_def.h"

// #define DEBUGINFO	//信息打印开关

/** big_endian */
// #define RIFF_ID		0x52494646
// #define AVI_ID		0x41564920
// #define LIST_ID		0x4c495354
// #define hdrl_ID		0x6864726c
// #define avih_ID		0x61766968
// #define strl_ID		0x7374726c
// #define strh_ID		0x73747268
// #define strf_ID		0x73747266
// #define movi_ID		0x6d6f7669
// #define mjpg_ID     0x4D4A5047
// #define vids_ID     0x76696473
// #define auds_ID     0x61756473

/** little_endian */
#define RIFF_ID		_REV(0x52494646)
#define AVI_ID		_REV(0x41564920)
#define LIST_ID		_REV(0x4c495354)
#define hdrl_ID		_REV(0x6864726c)
#define avih_ID		_REV(0x61766968)
#define strl_ID		_REV(0x7374726c)
#define strh_ID		_REV(0x73747268)
#define strf_ID		_REV(0x73747266)
#define movi_ID		_REV(0x6d6f7669)
#define mjpg_ID     _REV(0x4D4A5047)
#define vids_ID     _REV(0x76696473)
#define auds_ID     _REV(0x61756473)


typedef struct
{	
      uint32_t  RIFFchunksize;
      uint32_t  LISTchunksize;
    uint32_t  avihsize;
    uint32_t  strlsize;
    uint32_t  strhsize;

    uint32_t movi_start; //数据块的起始位置
    uint32_t movi_size;

    uint16_t vids_fps;
    uint16_t vids_width;
    uint16_t vids_height;

    uint16_t auds_channels;
    uint16_t auds_sample_rate;
    uint16_t auds_bits;

}AVI_TypeDef;

// typedef struct
// {	
// 	uint32_t  SecPerFrame;	//显示每帧的时间
// 	uint32_t  MaxByteSec; 	//最大数据传输率
// 	uint32_t  ChunkBase; 	//块长度为此值的倍数，通常为2048
// 	uint32_t  SpecProp;	  //AVI文件的特殊属性
// 	uint32_t  TotalFrame;	//文件总帧数
// 	uint32_t  InitFrames;  //开始播放前的初始化帧数
// 	uint32_t  Streams;		  //包含的数据流种类
// 	uint32_t  RefBufSize;  //建议使用的缓冲区大小，通常为存储一帧图像与同步音频数据之和
// 	uint32_t  Width;		    //图像宽
// 	uint32_t  Height;		  //图像高
// 	uint32_t  Reserved;	  //保留
// }avih_TypeDef;

// typedef struct
// {	
// 	uint8_t StreamType[4];//数据流种类，vids(0x73646976):视频,auds:音频
// 	uint8_t Handler[4];	  //数据解压缩的驱动程序代号
// 	uint32_t   StreamFlag;   //数据流属性
// 	uint32_t    Priority;	    //此数据流播放的优先级
// 	uint32_t    Language;	    //音频的语言代号
// 	uint32_t   InitFrames;   //开始播放前的初始化帧数
// 	uint32_t   Scale;		    //视频每帧的大小或音频的采样率
// 	uint32_t   Rate; 		    //Scale/Rate=每秒采样数
// 	uint32_t   Start;		    //数据流开始播放的位置，单位为Scale
// 	uint32_t   Length;		    //数据流的数据量，单位为Scale
//  	uint32_t   RefBufSize;   //建议使用的缓冲区大小
//   uint32_t   Quality;		  //解压缩质量参数，值越大，质量越好
// 	uint32_t   SampleSize;	  //音频的样本大小
// 	struct{				
// 	  short int  Left;
// 		short int  Top;
// 		short int  Right;
// 		short int  Bottom;
// 	}Frame;				        //视频帧所占的矩形 
// }strh_TypeDef;

// typedef struct tagBMPHEADER
// {
//   uint32_t  	Size;		      //本结构所占用字节数
// 	long   	Width;		    //图像宽
// 	long   	Height;		    //图像高
// 	uint32_t   	Planes;		    //平面数，必须为1
// 	uint32_t   	BitCount;	    //像素位数
// 	uint8_t  	Compression[4];	//压缩类型，MJPEG
// 	uint32_t  	SizeImage;	  //图像大小
// 	long   	XpixPerMeter; //水平分辨率
// 	long   	YpixPerMeter; //垂直分辨率
// 	uint32_t  	ClrUsed;		  //实际使用了调色板中的颜色数,压缩格式中不使用
// 	uint32_t  	ClrImportant; //重要的颜色
// }BMPHEADER;             //该结构占用40字节

// typedef struct tagRGBQUAD 
// {
// 	uint8_t  rgbBlue;	      //蓝色的亮度(值范围为0-255)
// 	uint8_t  rgbGreen;       //绿色的亮度(值范围为0-255)
// 	uint8_t  rgbRed; 	      //红色的亮度(值范围为0-255)
// 	uint8_t  rgbReserved;    //保留，必须为0
// }RGBQUAD;               //颜色表

// typedef struct tagBITMAPINFO 
// {
// 	BMPHEADER  bmiHeader;  //位图信息头
// 	RGBQUAD    bmColors[1];//颜色表
// }BITMAPINFO;  

// typedef struct 
// {
//   uint32_t  FormatTag;
// 	uint32_t  Channels;	  //声道数
// 	uint32_t SampleRate; //采样率
// 	uint32_t BaudRate;   //波特率
// 	uint32_t  BlockAlign; //数据块对齐标志
// 	uint32_t  Size;       //该结构大小
// }WAVEFORMAT;

#define	 MAKEWORD(ptr)	(uint32_t)(((uint32_t)*((uint8_t*)(ptr))<<8)|(uint32_t)*(uint8_t*)((ptr)+1))
#define  MAKEuint32_t(ptr)	(uint32_t)(((uint32_t)*(uint8_t*)(ptr)|(((uint32_t)*(uint8_t*)(ptr+1))<<8)|\
                        (((uint32_t)*(uint8_t*)(ptr+2))<<16)|(((uint32_t)*(uint8_t*)(ptr+3))<<24))) 


int AVI_Parser(const uint8_t *buffer, uint32_t length);

#endif

