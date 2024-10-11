/*
 * @Author: Kevincoooool
 * @Date: 2022-04-19 15:07:32
 * @Description:
 * @version:
 * @Filename: Do not Edit
 * @LastEditTime: 2024-03-25 15:51:49
 * @FilePath: \Korvo_Demo\14.avi_player\main\avi_player.c
 */

#include "avi_player.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>

#include <esp_system.h>
#include "esp_log.h"
#include "file_manager.h"
#include "lvgl.h"

#include "avifile.h"
#include "vidoplayer.h"
#include "mjpeg.h"
#include "driver/i2s.h"

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#include "esp_jpeg_enc.h"

#define TAG "avi_player"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include "esp_timer.h"
#include "esp_random.h"
#endif
extern AVI_TypeDef AVI_file;
lv_obj_t *img_cam; // 要显示图像
lv_img_dsc_t img_dsc = {
    .header.always_zero = 0,
    .header.w = 280,
    .header.h = 240,
    .data_size = 280 * 240 * 2,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};
#define filename "/sdcard/badapple.avi"

// #define IIS_SCLK 16
// #define IIS_LCLK 45
// #define IIS_DSIN 8
// #define IIS_DOUT 10

// #define I2S_NUM 0

// void play_i2s_init(void)
// {
// 	i2s_config_t i2s_config = {
// 		.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
// 		.sample_rate = 44100,
// 		.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
// 		.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
// 		.communication_format = I2S_COMM_FORMAT_STAND_I2S,
// 		.intr_alloc_flags = ESP_INTR_FLAG_LOWMED,
// 		.dma_buf_count = 6,
// 		.dma_buf_len = 1024,
// 		.bits_per_chan = I2S_BITS_PER_SAMPLE_16BIT};
// 	i2s_pin_config_t pin_config = {
// 		.mck_io_num = -1,
// 		.bck_io_num = IIS_SCLK,	  // IIS_SCLK
// 		.ws_io_num = IIS_LCLK,	  // IIS_LCLK
// 		.data_out_num = IIS_DSIN, // IIS_DSIN
// 		.data_in_num = -1		  // IIS_DOUT
// 	};
// 	i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
// 	i2s_set_pin(I2S_NUM, &pin_config);
// 	i2s_zero_dma_buffer(I2S_NUM);
// }

extern const lv_img_dsc_t img_test;

static jpeg_error_t esp_jpeg_decoder_one_image(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    jpeg_error_t ret = JPEG_ERR_OK;
    int inbuf_consumed = 0;

    // Generate default configuration
    jpeg_dec_config_t config = {
        .output_type = JPEG_RAW_TYPE_RGB565_LE,
        .rotate = JPEG_ROTATE_0D,
    };

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t *jpeg_dec = NULL;

    // Create jpeg_dec
    jpeg_dec = jpeg_dec_open(&config);

    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = (jpeg_dec_io_t *)calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL)
    {
        return JPEG_ERR_MEM;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info = (jpeg_dec_header_info_t *)calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL)
    {
        return JPEG_ERR_MEM;
    }
    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0)
    {
        // Serial.println("JPEG decode parse failed");
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;
    inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = input_buf + inbuf_consumed;
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0)
    {
        // Serial.println("JPEG decode process failed");
        goto _exit;
    }

_exit:
    // Decoder deinitialize
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
    return ret;
}
FILE *avi_file;
int ret = 0;
size_t BytesRD = 0;
uint32_t Strsize = 0;
uint32_t Strtype = 0;
uint8_t *pbuffer = NULL;
uint32_t buffer_size = 100 * 1024;
uint8_t *img_rgb565 = NULL;

void play_file(char *Filename)
{
    avi_file = fopen(Filename, "rb");
    ESP_LOGW(TAG, "open %s", Filename);

    if (avi_file == NULL)
    {
        ESP_LOGE(TAG, "Cannot open %s", Filename);
        return;
    }

    BytesRD = fread(pbuffer, 20 * 1024, 1, avi_file);
    ret = AVI_Parser(pbuffer, BytesRD);
    if (0 > ret)
    {
        ESP_LOGE(TAG, "parse failed (%d)", ret);
        return;
    }
    ESP_LOGI(TAG, "frame_rate=%d, ch=%d, width=%d", AVI_file.auds_sample_rate, AVI_file.auds_channels, AVI_file.auds_bits);
    uint16_t img_width = AVI_file.vids_width;
    uint16_t img_height = AVI_file.vids_height;

    fseek(avi_file, AVI_file.movi_start, SEEK_SET); // 偏移到movi list
    Strsize = read_frame(avi_file, pbuffer, buffer_size, &Strtype);
    BytesRD = Strsize + 8;
    uint8_t temp = 0;
    while (1)
    { // 播放循环
        if (BytesRD >= AVI_file.movi_size)
        {
            ESP_LOGI(TAG, "paly end");
            fclose(avi_file);
            break;
        }
        if (Strtype == T_vids)
        { // 显示帧
            int64_t fr_end = esp_timer_get_time();
            esp_jpeg_decoder_one_image(pbuffer, Strsize, img_rgb565); // 使用乐鑫adf的jpg解码 速度快三倍
            img_dsc.data = (uint8_t *)img_rgb565;
            lv_img_set_src(img_cam, &img_dsc);
            // ESP_LOGI(TAG, "draw %ums", (uint32_t)((esp_timer_get_time() - fr_end) / 1000));

        } // 显示帧
        else if (Strtype == T_auds)
        { // 音频输出
            size_t cnt;
        }
        else
        {
            ESP_LOGE(TAG, "unknow frame");
            break;
        }
        Strsize = read_frame(avi_file, pbuffer, buffer_size, &Strtype); // 读入整帧
        ESP_LOGD(TAG, "type=%x, size=%d", Strtype, Strsize);
        BytesRD += Strsize + 8;
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
uint8_t new_out_play_state = FACE_STATIC;
uint8_t out_play_state = FACE_STATIC;
uint8_t in_play_state = CIRCLE_IN;
uint8_t static_play_state = 1;

uint8_t need_change = 1;
char file_in_name[128] = {0};
char file_run_name[128] = {0};
char file_out_name[128] = {0};
static uint8_t delay_time = 0;
#define LIMIT(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

uint16_t Get_Set_Random(uint8_t set)
{
    return LIMIT(esp_random() % set, 1, set);
}
void play_change(uint8_t state)
{
    need_change = 1;
    new_out_play_state = state;
    in_play_state = CIRCLE_OUT;
}
void IN_play(char *file, uint8_t state)
{
    // if (need_change == 1)
    // {
    switch (state)
    {
    case FACE_STATIC:

        switch (static_play_state)
        {
        case 0:

            memset(file_in_name, 0, sizeof(file_in_name));
            memset(file_run_name, 0, sizeof(file_run_name));
            memset(file_out_name, 0, sizeof(file_out_name));
            memcpy(file_in_name, "/sdcard/static/static_1.avi", strlen("/sdcard/static/static_1.avi"));
            memcpy(file_run_name, "/sdcard/static/static_1.avi", strlen("/sdcard/static/static_1.avi"));
            memcpy(file_out_name, "/sdcard/static/static_2.avi", strlen("/sdcard/static/static_2.avi"));
            break;
        case 1:

            memset(file_in_name, 0, sizeof(file_in_name));
            memset(file_run_name, 0, sizeof(file_run_name));
            memset(file_out_name, 0, sizeof(file_out_name));
            memcpy(file_in_name, "/sdcard/right/right_1.avi", strlen("/sdcard/right/right_1.avi"));
            memcpy(file_run_name, "/sdcard/right/right_2.avi", strlen("/sdcard/right/right_2.avi"));
            memcpy(file_out_name, "/sdcard/right/right_3.avi", strlen("/sdcard/right/right_3.avi"));
            break;
        case 2:

            memset(file_in_name, 0, sizeof(file_in_name));
            memset(file_run_name, 0, sizeof(file_run_name));
            memset(file_out_name, 0, sizeof(file_out_name));
            memcpy(file_in_name, "/sdcard/left/left_1.avi", strlen("/sdcard/left/left_1.avi"));
            memcpy(file_run_name, "/sdcard/left/left_2.avi", strlen("/sdcard/left/left_2.avi"));
            memcpy(file_out_name, "/sdcard/left/left_3.avi", strlen("/sdcard/left/left_3.avi"));
            break;

        default:
            break;
        }

        break;

    case FACE_HAPPY:
        memset(file_in_name, 0, sizeof(file_in_name));
        memset(file_run_name, 0, sizeof(file_run_name));
        memset(file_out_name, 0, sizeof(file_out_name));
        memcpy(file_in_name, "/sdcard/happy/happy_1.avi", strlen("/sdcard/happy/happy_1.avi"));
        memcpy(file_run_name, "/sdcard/happy/happy_2.avi", strlen("/sdcard/happy/happy_2.avi"));
        memcpy(file_out_name, "/sdcard/happy/happy_3.avi", strlen("/sdcard/happy/happy_3.avi"));
        break;

    case FACE_ANGRY:
        memset(file_in_name, 0, sizeof(file_in_name));
        memset(file_run_name, 0, sizeof(file_run_name));
        memset(file_out_name, 0, sizeof(file_out_name));
        memcpy(file_in_name, "/sdcard/angry/angry_1.avi", strlen("/sdcard/angry/angry_1.avi"));
        memcpy(file_run_name, "/sdcard/angry/angry_2.avi", strlen("/sdcard/angry/angry_2.avi"));
        memcpy(file_out_name, "/sdcard/angry/angry_3.avi", strlen("/sdcard/angry/angry_3.avi"));
        break;

    case FACE_BAD:
        memset(file_in_name, 0, sizeof(file_in_name));
        memset(file_run_name, 0, sizeof(file_run_name));
        memset(file_out_name, 0, sizeof(file_out_name));
        memcpy(file_in_name, "/sdcard/bad/bad_1.avi", strlen("/sdcard/bad/bad_1.avi"));
        memcpy(file_run_name, "/sdcard/bad/bad_2.avi", strlen("/sdcard/bad/bad_2.avi"));
        memcpy(file_out_name, "/sdcard/bad/bad_3.avi", strlen("/sdcard/bad/bad_3.avi"));
        break;

    case FACE_FEAR:
        memset(file_in_name, 0, sizeof(file_in_name));
        memset(file_run_name, 0, sizeof(file_run_name));
        memset(file_out_name, 0, sizeof(file_out_name));
        memcpy(file_in_name, "/sdcard/fear/fear_1.avi", strlen("/sdcard/fear/fear_1.avi"));
        memcpy(file_run_name, "/sdcard/fear/fear_2.avi", strlen("/sdcard/fear/fear_2.avi"));
        memcpy(file_out_name, "/sdcard/fear/fear_3.avi", strlen("/sdcard/fear/fear_3.avi"));
        break;

    case FACE_NOGOOD:
        memset(file_in_name, 0, sizeof(file_in_name));
        memset(file_run_name, 0, sizeof(file_run_name));
        memset(file_out_name, 0, sizeof(file_out_name));
        memcpy(file_in_name, "/sdcard/nogood/nogood_1.avi", strlen("/sdcard/nogood/nogood_1.avi"));
        memcpy(file_run_name, "/sdcard/nogood/nogood_2.avi", strlen("/sdcard/nogood/nogood_2.avi"));
        memcpy(file_out_name, "/sdcard/nogood/nogood_3.avi", strlen("/sdcard/nogood/nogood_3.avi"));
        break;

    default:
        break;
    }
    //     need_change = 0;
    // }

    switch (in_play_state)
    {
    case CIRCLE_IN:
        play_file(file_in_name);
        in_play_state = CIRCLE_RUN;
        need_change = 0;
        break;
    case CIRCLE_RUN:
        play_file(file_run_name);
        // in_play_state = CIRCLE_OUT;
        if (need_change == 1)
        {
            in_play_state = CIRCLE_OUT;
            need_change = 0;
        }
        else
        {
            in_play_state = CIRCLE_RUN;
        }
        if (state == FACE_STATIC)
        {
            delay_time = Get_Set_Random(3);
            printf("random delay_time :%d\n", delay_time);
            vTaskDelay(delay_time * 1000);
            static_play_state = Get_Set_Random(4);
            printf("static_play_state face :%d\n", static_play_state);
            if (static_play_state != 1)
            {
                in_play_state = CIRCLE_IN;
            }
        }

        break;
    case CIRCLE_OUT:
        play_file(file_out_name);
        in_play_state = CIRCLE_END;

        break;
    case CIRCLE_END:
        // in_play_state = CIRCLE_IN;

        vTaskDelay(2 / portTICK_PERIOD_MS);

        break;
    default:
        break;
    }
}
void Avi_Player_Task(void *arg)
{

    pbuffer = heap_caps_malloc(buffer_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM); // MALLOC_CAP_SPIRAM  MALLOC_CAP_DMA

    if (pbuffer == NULL)
    {
        ESP_LOGE(TAG, "Cannot alloc memory for palyer");
        return;
    }
    img_rgb565 = heap_caps_malloc(280 * 240 * 2, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM); // MALLOC_CAP_SPIRAM
    if (NULL == img_rgb565)
    {
        ESP_LOGE(TAG, "malloc for rgb888 failed");
    }
    //             IN_play("/sdcard/badapple.avi", FACE_STATIC);
    while (1)
    {
        ESP_LOGE(TAG, "out_play_state :%d in_play_state:%d", out_play_state, in_play_state);

        switch (out_play_state)
        {
        case FACE_STATIC:
            IN_play("/sdcard/badapple.avi", FACE_STATIC);
            if (in_play_state == CIRCLE_END || need_change == 1)
            {
                out_play_state = new_out_play_state;
                in_play_state = CIRCLE_IN;
            }

            else
                out_play_state = FACE_STATIC;

            break;
        case FACE_HAPPY:
            IN_play("/sdcard/badapple.avi", FACE_HAPPY);
            if (in_play_state == CIRCLE_END || need_change == 1)
            {
                out_play_state = new_out_play_state;
                in_play_state = CIRCLE_IN;
            }
            else
                out_play_state = FACE_HAPPY;
            break;
        case FACE_ANGRY:
            IN_play("/sdcard/badapple.avi", FACE_ANGRY);
            if (in_play_state == CIRCLE_END || need_change == 1)
            {
                out_play_state = new_out_play_state;
                in_play_state = CIRCLE_IN;
            }
            else
                out_play_state = FACE_ANGRY;
            break;
        case FACE_BAD:
            IN_play("/sdcard/badapple.avi", FACE_BAD);
            if (in_play_state == CIRCLE_END || need_change == 1)
            {
                out_play_state = new_out_play_state;
                in_play_state = CIRCLE_IN;
            }
            else
                out_play_state = FACE_BAD;
            break;
        case FACE_FEAR:
            IN_play("/sdcard/badapple.avi", FACE_FEAR);
            if (in_play_state == CIRCLE_END || need_change == 1)
            {
                out_play_state = new_out_play_state;
                in_play_state = CIRCLE_IN;
            }
            else
                out_play_state = FACE_FEAR;
            break;
        case FACE_NOGOOD:
            IN_play("/sdcard/badapple.avi", FACE_NOGOOD);
            if (in_play_state == CIRCLE_END || need_change == 1)
            {
                out_play_state = new_out_play_state;
                in_play_state = CIRCLE_IN;
            }

            else
                out_play_state = FACE_NOGOOD;
            break;
        default:
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
void Cam_Task(void *pvParameters)
{

    fm_sdcard_init();

    avi_file = fopen(filename, "rb");
    if (avi_file == NULL)
    {
        ESP_LOGE(TAG, "Cannot open %s", filename);
        return;
    }

    BytesRD = fread(pbuffer, 20 * 1024, 1, avi_file);
    ret = AVI_Parser(pbuffer, BytesRD);
    if (0 > ret)
    {
        ESP_LOGE(TAG, "parse failed (%d)", ret);
        return;
    }
    ESP_LOGI(TAG, "frame_rate=%d, ch=%d, width=%d", AVI_file.auds_sample_rate, AVI_file.auds_channels, AVI_file.auds_bits);
    uint16_t img_width = AVI_file.vids_width;
    uint16_t img_height = AVI_file.vids_height;
    uint8_t *img_rgb565 = heap_caps_malloc(img_width * img_height * 2, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM); // MALLOC_CAP_SPIRAM
    // uint8_t *img_rgb565 = heap_caps_malloc(img_width * img_height * 2, MALLOC_CAP_8BIT | MALLOC_CAP_DMA); // MALLOC_CAP_SPIRAM

    if (NULL == img_rgb565)
    {
        ESP_LOGE(TAG, "malloc for rgb888 failed");
        // goto EXIT;
    }

    fseek(avi_file, AVI_file.movi_start, SEEK_SET); // 偏移到movi list
    // start_play_audio();
    Strsize = read_frame(avi_file, pbuffer, buffer_size, &Strtype);
    BytesRD = Strsize + 8;
    uint8_t temp = 0;

    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    while (1)
    { // 播放循环
        if (BytesRD >= AVI_file.movi_size)
        {
            ESP_LOGI(TAG, "paly end");
            break;
        }
        if (Strtype == T_vids)
        { // 显示帧
            int64_t fr_end = esp_timer_get_time();
            // mjpegdraw(pbuffer, Strsize, img_rgb565);//软件jpeg解码 比较慢
            // for (int i = 0; i < 800 * 480 * 2; i += 2)//根据需求交换RGB
            // {
            //     temp = img_rgb565[i];
            //     img_rgb565[i] = img_rgb565[i + 1];
            //     img_rgb565[i + 1] = temp;
            // }
            // / jpg2rgb565((const uint8_t *)pbuffer, Strsize, img_rgb565, JPG_SCALE_NONE);
            // ESP_LOGI(TAG, "FPS %u", 1000 / ((uint32_t)((esp_timer_get_time() - fr_end) / 1000)));

            esp_jpeg_decoder_one_image(pbuffer, Strsize, img_rgb565); // 使用乐鑫adf的jpg解码 速度快三倍

            img_dsc.data = (uint8_t *)img_rgb565;
            lv_img_set_src(img_cam, &img_dsc);
            // fr_end = esp_timer_get_time();
            // g_lcd.draw_bitmap(0, 0, img_width, img_height, img_rgb565);
            ESP_LOGI(TAG, "draw %ums", (uint32_t)((esp_timer_get_time() - fr_end) / 1000));

        } // 显示帧
        else if (Strtype == T_auds)
        { // 音频输出
            size_t cnt;
            // i2s_write(0, (uint8_t *)pbuffer, Strsize, &cnt, 50 / portTICK_PERIOD_MS);

            // pwm_audio_write((uint8_t *)pbuffer, Strsize, &cnt, 500 / portTICK_PERIOD_MS);
        }
        else
        {
            ESP_LOGE(TAG, "unknow frame");
            break;
        }
        Strsize = read_frame(avi_file, pbuffer, buffer_size, &Strtype); // 读入整帧
        ESP_LOGD(TAG, "type=%x, size=%d", Strtype, Strsize);
        BytesRD += Strsize + 8;
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    // never reach
    while (1)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void imgcam_init(void)
{

    img_cam = lv_img_create(lv_scr_act());
    lv_obj_align(img_cam, LV_ALIGN_CENTER, 0, 0);
}
void avi_player_load()
{
    imgcam_init();

    xTaskCreatePinnedToCore(&Avi_Player_Task, "Avi_Player_Task", 1024 * 8, NULL, 10, NULL, 1);
}
