#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
// #include "file_manager.h"
// #include "pwm_audio.h"
#include "avifile.h"
#include "vidoplayer.h"
#include "mjpeg.h"

static const char *TAG = "avi player";

#define PLAYER_CHECK(a, str, ret)                                              \
    if (!(a))                                                                  \
    {                                                                          \
        ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret);                                                          \
    }

/**
 * TODO: how to recognize each stream id
 */

extern AVI_TypeDef AVI_file;

uint32_t _REV(uint32_t value)
{
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
           (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}

// static void audio_init(void)
// {
//     pwm_audio_config_t pac;
//     pac.duty_resolution = LEDC_TIMER_10_BIT;
//     pac.gpio_num_left = 25;
//     pac.ledc_channel_left = LEDC_CHANNEL_0;
//     pac.gpio_num_right = -1;
//     pac.ledc_channel_right = LEDC_CHANNEL_1;
//     pac.ledc_timer_sel = LEDC_TIMER_0;
//     pac.tg_num = TIMER_GROUP_0;
//     pac.timer_num = TIMER_0;
//     pac.ringbuf_len = 1024 * 8;
//     pwm_audio_init(&pac);

//     pwm_audio_set_volume(0);
// }

uint32_t read_frame(FILE *file, uint8_t *buffer, uint32_t length, uint32_t *fourcc)
{
    AVI_CHUNK_HEAD head;
    fread(&head, sizeof(AVI_CHUNK_HEAD), 1, file);
    if (head.FourCC)
    {
        /* code */
    }
    *fourcc = head.FourCC;
    if (head.size % 2)
    {
        head.size++; //奇数加1
    }
    if (length < head.size)
    {
        ESP_LOGE(TAG, "frame size too large");
        return 0;
    }

    uint32_t ret = fread(buffer, head.size, 1, file);
    return head.size;
}

// #include "esp_camera.h"
// #include "screen_driver.h"
// extern scr_driver_t g_lcd;

static void audio_task(void *args)
{
    while (1)
    {
        /* code */
    }
    vTaskDelete(NULL);
}

static void video_task(void *args)
{
    while (1)
    {
        /* code */
    }
    vTaskDelete(NULL);
}

void avi_play(const char *filename)
{
    FILE *avi_file;
    int ret;
    size_t BytesRD;
    uint32_t Strsize;
    uint32_t Strtype;
    uint8_t *pbuffer;
    uint32_t buffer_size = 22 * 1024;

    avi_file = fopen(filename, "rb");
    if (avi_file == NULL)
    {
        ESP_LOGE(TAG, "Cannot open %s", filename);
        return;
    }

    pbuffer = malloc(buffer_size);
    if (pbuffer == NULL)
    {
        ESP_LOGE(TAG, "Cannot alloc memory for palyer");
        fclose(avi_file);
        return;
    }

    BytesRD = fread(pbuffer, 20480, 1, avi_file);
    ret = AVI_Parser(pbuffer, BytesRD);
    if (0 > ret)
    {
        ESP_LOGE(TAG, "parse failed (%d)", ret);
        return;
    }

    // audio_init();
    // pwm_audio_set_param(AVI_file.auds_sample_rate, AVI_file.auds_bits, AVI_file.auds_channels);
    // pwm_audio_start();

    uint16_t img_width = AVI_file.vids_width;
    uint16_t img_height = AVI_file.vids_height;
    uint8_t *img_rgb888 = heap_caps_malloc(img_width * img_height * 2, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (NULL == img_rgb888)
    {
        ESP_LOGE(TAG, "malloc for rgb888 failed");
        goto EXIT;
    }

    fseek(avi_file, AVI_file.movi_start, SEEK_SET); // 偏移到movi list
    Strsize = read_frame(avi_file, pbuffer, buffer_size, &Strtype);
    BytesRD = Strsize + 8;

    while (1)
    { //播放循环
        if (BytesRD >= AVI_file.movi_size)
        {
            ESP_LOGI(TAG, "paly end");
            break;
        }
        if (Strtype == T_vids)
        { //显示帧
            mjpegdraw(pbuffer, Strsize, img_rgb888);

            // jpg2rgb565((const uint8_t *)pbuffer, Strsize, img_rgb888, JPG_SCALE_NONE);

            // g_lcd.draw_bitmap(0, 0, img_width, img_height, img_rgb888);
            // ESP_LOGI(TAG, "draw %ums", (uint32_t)((esp_timer_get_time() - fr_end) / 1000));fr_end = esp_timer_get_time();

        } //显示帧
        else if (Strtype == T_auds)
        { //音频输出
            size_t cnt;
            // pwm_audio_write((uint8_t *)pbuffer, Strsize, &cnt, 500 / portTICK_PERIOD_MS);

        }
        else
        {
            ESP_LOGE(TAG, "unknow frame");
            break;
        }
        Strsize = read_frame(avi_file, pbuffer, buffer_size, &Strtype); //读入整帧
        ESP_LOGD(TAG, "type=%x, size=%d", Strtype, Strsize);
        BytesRD += Strsize + 8;
    }
EXIT:
    // pwm_audio_deinit();
    free(img_rgb888);
    free(pbuffer);
    fclose(avi_file);
}
