#ifndef _AVI_DEFINE_H_
#define _AVI_DEFINE_H_

#include "stdint.h"


/**
 * reference links: https://www.cnblogs.com/songhe364826110/p/7619949.html 
 * AVIFileFormat: https://web.archive.org/web/20170411001412/http://www.alexander-noe.com/video/documentation/avi.pdf
 * OpenDML AVI File Format Extensions: https://web.archive.org/web/20070112225112/http://www.the-labs.com/Video/odmlff2-avidef.pdf
 */

typedef struct {
    uint32_t FourCC; 
    uint32_t size;   //块大小，等于之后数据的大小
    /* 数据 uint8_t _data[size]; */
}AVI_CHUNK_HEAD;

typedef struct {
    uint32_t List;   //固定为"LIST"，如果是 RIFF List 则为 "RIFF"
    uint32_t size;   //块大小，等于之后数据的大小
    uint32_t FourCC;   
    /* 数据 uint8_t _data[size-4]; */
}AVI_LIST_HEAD;

typedef struct {
    uint32_t FourCC;            //块ID，固定为avih
    uint32_t size;              //块大小，等于struct avi_avih_chunk去掉id和size的大小
    uint32_t us_per_frame;      //视频帧间隔时间(以微秒为单位)
    uint32_t max_bytes_per_sec; //AVI文件的最大数据率
    uint32_t padding;           //设为0即可
    uint32_t flags;             //AVI文件全局属性，如是否含有索引块、音视频数据是否交叉存储等
    uint32_t total_frames;      //总帧数
    uint32_t init_frames;       //为交互格式指定初始帧数(非交互格式应该指定为0)
    uint32_t streams;           //文件包含的流的个数，仅有视频流时为1
    uint32_t suggest_buff_size; //指定读取本文件建议使用的缓冲区大小，通常为存储一桢图像以及同步声音所需的数据之和，不指定时设为0
    uint32_t width;             //视频主窗口宽度（单位：像素）
    uint32_t height;            //视频主窗口高度（单位：像素）
    uint32_t reserved[4];       //保留值dwScale,dwRate,dwStart,dwLength
}AVI_AVIH_CHUNK;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;    
}AVI_RECT_FRAME;

typedef struct
{    
    uint32_t FourCC;            //块ID，固定为strh
    uint32_t size;              //块大小，等于struct avi_strh_chunk去掉id和size的大小
    uint32_t fourcc_type;   //流的类型，vids表示视频流，auds表示音频流
    uint32_t fourcc_codec;         //指定处理这个流需要的解码器，如JPEG
    uint32_t flags;             //标记，如是否允许这个流输出、调色板是否变化等，一般设为0即可
    uint16_t priority;        //流的优先级，视频流设为0即可
    uint16_t language;        //音频语言代号，视频流设为0即可
    uint32_t init_frames;       //为交互格式指定初始帧数(非交互格式应该指定为0)
    uint32_t scale;             //
    uint32_t rate;              //对于视频流，rate / scale = 帧率fps
    uint32_t start;             //对于视频流，设为0即可
    uint32_t length;            //对于视频流，length即总帧数
    uint32_t suggest_buff_size; //读取这个流数据建议使用的缓冲区大小
    uint32_t quality;           //流数据的质量指标
    uint32_t sample_size;       //音频采样大小，视频流设为0即可
    AVI_RECT_FRAME rcFrame;         //这个流在视频主窗口中的显示位置，设为{0,0，width,height}即可
}AVI_STRH_CHUNK;

/*对于视频流，strf块结构如下*/
typedef struct {
    uint32_t FourCC;             //块ID，固定为strf
    uint32_t size;               //块大小，等于struct avi_strf_chunk去掉id和size的大小
    uint32_t size1;              //size1含义和值同size一样
    uint32_t width;              //视频主窗口宽度（单位：像素）
    uint32_t height;             //视频主窗口高度（单位：像素）
    uint16_t planes;             //始终为1  
    uint16_t bitcount;           //每个像素占的位数，只能是1、4、8、16、24和32中的一个
    uint32_t fourcc_compression; //视频流编码格式，如"JPEG"、"MJPG"等
    uint32_t image_size;         //视频图像大小，等于width * height * bitcount / 8
    uint32_t x_pixels_per_meter; //显示设备的水平分辨率，设为0即可
    uint32_t y_pixels_per_meter; //显示设备的垂直分辨率，设为0即可
    uint32_t num_colors;         //含义不清楚，设为0即可   
    uint32_t imp_colors;         //含义不清楚，设为0即可
}AVI_VIDS_STRF_CHUNK;

/*对于音频流，strf块结构如下*/
typedef struct __attribute__((packed)) {
    uint32_t FourCC;             //块ID，固定为strf
    uint32_t size;               //块大小，等于struct avi_strf_chunk去掉id和size的大小
    uint16_t format_tag;
    uint16_t channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint32_t bits_per_sample;
}AVI_AUDS_STRF_CHUNK;

typedef struct 
{
    AVI_LIST_HEAD strl;
    AVI_STRH_CHUNK strh;
    AVI_VIDS_STRF_CHUNK strf;
}AVI_STRL_LIST;

typedef struct 
{
    AVI_LIST_HEAD hdrl;
    AVI_AVIH_CHUNK avih;
    AVI_STRL_LIST  strl;
}AVI_HDRL_LIST;

typedef struct {
    uint32_t FourCC; //块ID，固定为 "idx1"
    uint32_t flags;
    uint32_t chunkoffset;
    uint32_t chunklength;
}AVI_IDX1;

/**
"db"：未压缩的视频帧（RGB数据流）；
"dc"：压缩的视频帧；
"wb"：音频未压缩数据（Wave数据流）；
"wc"：音频压缩数据（压缩的Wave数据流）；
"pc"：改用新的调色板。（新的调色板使用一个数据结构AVIPALCHANGE来定义。如果一个流的调色板中途可能改变，则应在这个流格式的描述中，也就是AVISTREAMHEADER结构的dwFlags中包含一个AVISF_VIDEO_PALCHANGES标记。）
*/

#endif