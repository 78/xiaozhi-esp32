#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "avifile.h"


static const char *TAG = "avifile";

AVI_TypeDef AVI_file;

#define MAKE_FOURCC(a, b, c, d) ((uint32_t)(d)<<24 | (uint32_t)(c)<<16 | (uint32_t)(b)<<8 | (uint32_t)(a))
#define READ_WORD(a) ((uint32_t)*(a)<<24 | (uint32_t)*(a)<<16 | (uint32_t)*(a)<<8 | (uint32_t)*(a))

static uint32_t _REV(uint32_t value){
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 | 
        (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24; 
}

static int search_fourcc(uint32_t fourcc, const uint8_t *buffer, uint32_t length)
{
    uint32_t i, j;
    uint32_t *pdata;
    j = length - 4;
    for (i = 0; i < j; i++) {
        pdata = (uint32_t*)(buffer+i);
        if (fourcc == *pdata)
        {
            return i;
        }
    }
    return -1;
}

static int Strl_Parser(const uint8_t *buffer, uint32_t length, uint32_t *list_length)
{
    /**
     * TODO: how to deal with the list is not complete in the buffer 
     */
    const uint8_t *pdata = buffer;
    // strl(stream list), include "strh" and "strf"
    AVI_LIST_HEAD *strl = (AVI_LIST_HEAD*)pdata;
    if (strl->List != LIST_ID || strl->FourCC != strl_ID) {
        return -1;
    }
    pdata += sizeof(AVI_LIST_HEAD);
    *list_length = strl->size + 8; //return the entire size of list

    // strh
    AVI_STRH_CHUNK *strh = (AVI_STRH_CHUNK*)pdata;
    if (strh->FourCC != strh_ID || strh->size+8 != sizeof(AVI_STRH_CHUNK)) {
        return -5;
    }
#ifdef DEBUGINFO
    printf("-----strh info------\r\n");
    printf("fourcc_type:0x%x\r\n", strh->fourcc_type);
    printf("fourcc_codec:0x%x\r\n", strh->fourcc_codec);
    printf("flags:%d\r\n", strh->flags);
    printf("Priority:%d\r\n", strh->priority);
    printf("Language:%d\r\n", strh->language);
    printf("InitFrames:%d\r\n", strh->init_frames);
    printf("Scale:%d\r\n", strh->scale);
    printf("Rate:%d\r\n", strh->rate);
    printf("Start:%d\r\n", strh->start);
    printf("Length:%d\r\n", strh->length);
    printf("RefBufSize:%d\r\n", strh->suggest_buff_size);
    printf("Quality:%d\r\n", strh->quality);
    printf("SampleSize:%d\r\n", strh->sample_size);
    printf("FrameLeft:%d\r\n", strh->rcFrame.left);
    printf("FrameTop:%d\r\n", strh->rcFrame.top);
    printf("FrameRight:%d\r\n", strh->rcFrame.right);
    printf("FrameBottom:%d\r\n\n", strh->rcFrame.bottom);
#endif
    pdata += sizeof(AVI_STRH_CHUNK);

    if(vids_ID == strh->fourcc_type) {
        ESP_LOGI(TAG, "Find a video stream");
        if (mjpg_ID != strh->fourcc_codec) {
            ESP_LOGE(TAG, "only support mjpeg decoder, but needed is 0x%x", strh->fourcc_codec);
            return -1;
        }
        AVI_VIDS_STRF_CHUNK *strf = (AVI_VIDS_STRF_CHUNK*)pdata;
        if (strf->FourCC != strf_ID || strf->size+8 != sizeof(AVI_VIDS_STRF_CHUNK)) {
            return -5;
        }
#ifdef DEBUGINFO
        printf("-----video strf info------\r\n");
        printf("本结构体大小:%d\r\n", strf->size1);
        printf("图像宽:%d\r\n", strf->width);
        printf("图像高:%d\r\n", strf->height);
        printf("平面数:%d\r\n", strf->planes);
        printf("像素位数:%d\r\n", strf->bitcount);
        printf("压缩类型:0x%x\r\n", strf->fourcc_compression);
        printf("图像大小:%d\r\n", strf->image_size);
        printf("水平分辨率:%d\r\n", strf->x_pixels_per_meter);
        printf("垂直分辨率:%d\r\n", strf->y_pixels_per_meter);
        printf("使用调色板颜色数:%d\r\n", strf->num_colors);
        printf("重要颜色:%d\r\n\n", strf->imp_colors);
#endif
        AVI_file.vids_fps = strh->rate / strh->scale;
        AVI_file.vids_width = strf->width;
        AVI_file.vids_height = strf->height;
        pdata += sizeof(AVI_VIDS_STRF_CHUNK);

    } else if(auds_ID == strh->fourcc_type) {
        ESP_LOGI(TAG, "Find a audio stream");
        AVI_AUDS_STRF_CHUNK *strf = (AVI_AUDS_STRF_CHUNK*)pdata;
        if (strf->FourCC != strf_ID || (strf->size+8 != sizeof(AVI_AUDS_STRF_CHUNK) && strf->size+10 != sizeof(AVI_AUDS_STRF_CHUNK))) {
            ESP_LOGE(TAG, "FourCC=0x%x|%x, size=%d|%d", strf->FourCC, strf_ID, strf->size, sizeof(AVI_AUDS_STRF_CHUNK));
            return -5;
        }
#ifdef DEBUGINFO
        printf("-----audio strf info------\r\n");
        printf("strf数据块信息(音频流):");
        printf("格式标志:%d\r\n", strf->format_tag);
        printf("声道数:%d\r\n", strf->channels);
        printf("采样率:%d\r\n", strf->samples_per_sec);
        printf("波特率:%d\r\n", strf->avg_bytes_per_sec);
        printf("块对齐:%d\r\n", strf->block_align);
        printf("采样位宽:%d\r\n\n", strf->bits_per_sample);
#endif
        AVI_file.auds_channels = strf->channels;
        AVI_file.auds_sample_rate = strf->samples_per_sec;
        AVI_file.auds_bits = strf->bits_per_sample;
        pdata += sizeof(AVI_AUDS_STRF_CHUNK);

    } else {
        ESP_LOGW(TAG, "Unsupport stream 0x%x", strh->fourcc_type);
    }
    return 0;
}

int AVI_Parser(const uint8_t *buffer, uint32_t length)
{
    const uint8_t *pdata = buffer;
    AVI_LIST_HEAD *riff = (AVI_LIST_HEAD*)pdata;
    if (riff->List != RIFF_ID || riff->FourCC != AVI_ID) {
        return -1;
    }
    AVI_file.RIFFchunksize = riff->size; //RIFF数据块长度
    pdata += sizeof(AVI_LIST_HEAD);

    AVI_LIST_HEAD *list = (AVI_LIST_HEAD*)pdata;
    if (list->List != LIST_ID || list->FourCC != hdrl_ID) {
        return -3;
    }
    AVI_file.LISTchunksize = list->size; //LIST数据块长度
    pdata += sizeof(AVI_LIST_HEAD);

    // avih chunk
    AVI_AVIH_CHUNK *avih = (AVI_AVIH_CHUNK*)pdata;
    if (avih->FourCC != avih_ID || avih->size+8 != sizeof(AVI_AVIH_CHUNK)) {
        return -5;
    }
    AVI_file.avihsize = avih->size; //avih数据块长度
#ifdef DEBUGINFO
    printf("-----avih info------\r\n");
    printf("us_per_frame:%d\r\n", avih->us_per_frame);
    printf("max_bytes_per_sec:%d\r\n", avih->max_bytes_per_sec);
    printf("padding:%d\r\n", avih->padding);
    printf("flags:%d\r\n", avih->flags);
    printf("total_frames:%d\r\n", avih->total_frames);
    printf("init_frames:%d\r\n", avih->init_frames);
    printf("streams:%d\r\n", avih->streams);
    printf("suggest_buff_size:%d\r\n", avih->suggest_buff_size);
    printf("Width:%d\r\n", avih->width);
    printf("Height:%d\r\n\n", avih->height);
#endif
    if ((avih->width > 800) || (avih->height > 480)) {
        ESP_LOGE(TAG, "The size of video is too large");
        return -6;    //视频尺寸不支持
    }
    pdata += sizeof(AVI_AVIH_CHUNK);

    // process all streams in turn
    for (size_t i = 0; i < avih->streams; i++) {
        uint32_t strl_size=0;
        int ret = Strl_Parser(pdata, length-(pdata-buffer), &strl_size);
        if (0 > ret) {
            ESP_LOGE(TAG, "strl of stream%d prase failed", i);
            break;
            /**
             * TODO: how to deal this error? maybe we should search for the next strl.
             */
        }
        pdata += strl_size;
    }
    int movi_offset = search_fourcc(MAKE_FOURCC('m', 'o', 'v', 'i'), pdata, length-(pdata-buffer));
    if (0 > movi_offset) {
        ESP_LOGE(TAG, "can't find \"movi\" list");
        return -7;
    }
    AVI_file.movi_start = movi_offset + 4  + pdata - buffer;
    pdata += movi_offset - 8; // back to the list head
    AVI_LIST_HEAD *movi = (AVI_LIST_HEAD*)pdata;
    if (movi->List != LIST_ID || movi->FourCC != movi_ID) {
        return -8;
    }
    AVI_file.movi_size = movi->size; //LIST数据块长度
    pdata += sizeof(AVI_LIST_HEAD);
    ESP_LOGI(TAG, "movi pos:%d, size:%d", AVI_file.movi_start, AVI_file.movi_size);
    
    return 0;
}


