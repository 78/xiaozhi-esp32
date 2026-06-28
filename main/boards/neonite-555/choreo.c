// choreo.c - 舞蹈编排模块
//
// 功能：
//   1. 从 mmap assets 分区加载 *.json 舞蹈编排（cJSON）
//   2. 按 BPM 对齐音乐与动作节拍
//   3. speed 全局倍率 + offset_ms 整体偏移
//   4. 播放配乐 OGG Opus / WAV（mmap 只读，I2S 输出；OGG 为主）
//   5. 通过 MCP 工具 "舞蹈编排" 调用
//
// 依赖：cJSON（ESP-IDF 内置）、servo.h、dog_control.h、driver/i2s.h、esp_opus_dec
// 音乐格式要求：OGG Opus 24kHz/mono/60ms 或 16-bit PCM monaural 24kHz WAV
//
// 数据存储：舞蹈文件（JSON + OGG/WAV）在构建时通过 build_default_assets.py
//           --extra_files 打包进官方 mmap 格式的 assets.bin。
//           运行时通过 Assets::GetAssetData() 只读访问（封装为 choreo_assets_read）。
//           舞蹈列表通过 choreo_index.json 索引文件维护。
//
/* 舞蹈音乐音量系数（0.0 ~ 1.0）
 * 走 codec::OutputData 路径时 codec 内部已按 output_volume 缩放，设为 1.0 避免双重衰减。
 * 若使用直驱 I2S 路径，可改小此值（如 0.6f）防止失真。 */
#define CHOREO_VOLUME_SCALE  1.0f

#include "choreo.h"
#include "choreo_assets.h"  /* mmap assets 读取桥接（C++ → C） */
#include "dog_control.h"
#include "servo.h"
#include "config.h"       // AUDIO_I2S_SPK_* 引脚定义
#include <esp_log.h>
#include "esp_rom_gpio.h" // esp_rom_gpio_connect_out_signal
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* cJSON 头文件（ESP-IDF 组件）*/
#include <cJSON.h>

/* I2S 驱动（ESP-IDF v5 STD 模式）*/
#include <driver/i2s_std.h>
#include <driver/gpio.h>

/* Opus 解码器（esp_audio_codec 组件）*/
#include "esp_opus_dec.h"

static const char* TAG = "choreo";

/*============================================================================
  内部状态
============================================================================*/

static volatile bool s_choreo_playing  = false;
static volatile bool s_choreo_stop_req = false;
static TaskHandle_t  s_choreo_task_handle = NULL;
static TaskHandle_t  s_wav_task_handle   = NULL;
static TaskHandle_t  s_opus_task_handle  = NULL;  /* opus 独立句柄，不复用 wav */

/* WAV 播放状态（由 s_wav_task 更新）*/
static volatile bool s_audio_playing = false;

/* I2S TX 通道句柄（回退路径，仅当音频回调未注册时使用）*/
static i2s_chan_handle_t s_i2s_tx_chan = NULL;

/* 音频输出回调（由 compact_wifi_board.cc 注册，直接走 codec::Write，绕过 I2S 冲突）*/
static choreo_audio_ctrl_cb_t  s_audio_ctrl  = NULL;
static choreo_audio_write_cb_t s_audio_write = NULL;

/* opus_task 静态栈/TCB（SPIRAM 分配，play_opus_task 退出时手动释放）*/
static StackType_t*  s_opus_stack = NULL;
static StaticTask_t* s_opus_tcb   = NULL;

void choreo_set_audio_callbacks(choreo_audio_ctrl_cb_t ctrl, choreo_audio_write_cb_t write) {
    s_audio_ctrl  = ctrl;
    s_audio_write = write;
}

/*============================================================================
  mmap assets 读取（通过 choreo_assets_read 桥接官方 Assets C++ API）
============================================================================*/

/* 读取整个文件到 malloc 缓冲区（调用者 free），返回数据长度。
 * 成功返回 true，失败返回 false 且 *out_data = NULL, *out_size = 0。
 * name 可以是 "/assets/xxx" 或 "xxx"，自动去前缀。 */
static bool choreo_read_file(const char* name, uint8_t** out_data, size_t* out_size) {
    return choreo_assets_read(name, out_data, out_size);
}

/* 同 choreo_read_file（mmap 读取很快，不需要分块 yield）。
 * 保留接口兼容性，供 choreo_opus_play_async / choreo_play_async 调用。 */
static bool choreo_read_file_chunked(const char* name, uint8_t** out_data, size_t* out_size) {
    return choreo_assets_read(name, out_data, out_size);
}

/*============================================================================
  I2S 初始化 / 释放
============================================================================*/

static esp_err_t i2s_speaker_init(void) {
    /* 已初始化：只需 enable */
    if (s_i2s_tx_chan != NULL && i2s_channel_enable(s_i2s_tx_chan) == ESP_OK) {
        return ESP_OK;
    }

    /* 到达这里：s_i2s_tx_chan == NULL，或 enable 失败 → 重建 */
    ESP_LOGW(TAG, "I2S TX 通道不存在或 enable 失败，重建中");
    if (s_i2s_tx_chan) {
        i2s_channel_disable(s_i2s_tx_chan);
        i2s_del_channel(s_i2s_tx_chan);
        s_i2s_tx_chan = NULL;
    }

    /* 1. 创建 I2S TX 通道 */
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    tx_chan_cfg.dma_desc_num  = 4;
    tx_chan_cfg.dma_frame_num = 256;
    esp_err_t ret = i2s_new_channel(&tx_chan_cfg, &s_i2s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel TX 失败: %d", ret);
        return ret;
    }

    /* 2. 配置 STD 模式（用宏直接初始化，再覆盖需要定制的字段）*/
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_OUTPUT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk  = I2S_GPIO_UNUSED,
            .bclk  = AUDIO_I2S_SPK_GPIO_BCLK,
            .ws    = AUDIO_I2S_SPK_GPIO_LRCK,
            .dout  = AUDIO_I2S_SPK_GPIO_DOUT,
            .din   = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    /* 覆盖宏默认值：时钟源和 MCLK 倍数 */
    tx_std_cfg.clk_cfg.clk_src       = I2S_CLK_SRC_DEFAULT;
    tx_std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    ret = i2s_channel_init_std_mode(s_i2s_tx_chan, &tx_std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode 失败: %d", ret);
        return ret;
    }

    ret = i2s_channel_enable(s_i2s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable 失败: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "I2S 扬声器初始化完成（BCLK=%d, LRCK=%d, DOUT=%d）",
             AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT);
    return ESP_OK;
}

/* ESP32-S3 GPIO 矩阵 I2S 输出信号索引（soc/gpio_sig_map.h） */
#define GPIO_SIG_I2S0_BCK  22
#define GPIO_SIG_I2S0_WS   24
#define GPIO_SIG_I2S0_SD   25

/* 彻底释放 I2S_NUM_1 并将 GPIO 路由还给 I2S_NUM_0
   - i2s_channel_disable 只停时钟，GPIO 矩阵路由仍在 → 必须 delete 彻底清理
   - delete 后 GPIO 回到默认输入模式 → 重新设为输出并路由到 I2S0 信号
   - 不改小智 codec 逻辑：xiaozhi 的 i2s_channel_enable(I2S_NUM_0) 不重配 GPIO */
static void i2s_speaker_stop(void) {
    if (!s_i2s_tx_chan) return;

    i2s_channel_disable(s_i2s_tx_chan);
    i2s_del_channel(s_i2s_tx_chan);
    s_i2s_tx_chan = NULL;

    /* i2s_del_channel 后 GPIO 复位为默认，重新配置给 I2S_NUM_0 */
    gpio_set_direction(AUDIO_I2S_SPK_GPIO_BCLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(AUDIO_I2S_SPK_GPIO_LRCK, GPIO_MODE_OUTPUT);
    gpio_set_direction(AUDIO_I2S_SPK_GPIO_DOUT, GPIO_MODE_OUTPUT);

    esp_rom_gpio_connect_out_signal(AUDIO_I2S_SPK_GPIO_BCLK, GPIO_SIG_I2S0_BCK, false, false);
    esp_rom_gpio_connect_out_signal(AUDIO_I2S_SPK_GPIO_LRCK, GPIO_SIG_I2S0_WS,  false, false);
    esp_rom_gpio_connect_out_signal(AUDIO_I2S_SPK_GPIO_DOUT, GPIO_SIG_I2S0_SD,  false, false);

    ESP_LOGI(TAG, "I2S_NUM_1 已释放，GPIO 路由已还给 I2S_NUM_0");
}

/*============================================================================
  WAV 播放任务（mmap 读 → I2S 写 PCM）
============================================================================*/

#define WAV_BUF_SAMPLES  256
#define WAV_BUF_BYTES   (WAV_BUF_SAMPLES * sizeof(int16_t))

static void play_wav_task(void* arg) {
    const char* filepath = (const char*)arg;

    if (!filepath || strlen(filepath) == 0) {
        ESP_LOGE(TAG, "WAV 路径为空");
        s_audio_playing = false;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "WAV 播放: %s", filepath);

    /* 从 mmap assets 读取整个文件 */
    uint8_t* data = NULL;
    size_t data_size = 0;
    if (!choreo_read_file(filepath, &data, &data_size)) {
        ESP_LOGE(TAG, "无法读取 WAV 文件: %s", filepath);
        s_audio_playing = false;
        vTaskDelete(NULL);
        return;
    }

    if (data_size < 44) {
        ESP_LOGE(TAG, "WAV 文件太小: %u 字节", (unsigned)data_size);
        s_audio_playing = false;
        vTaskDelete(NULL);
        return;
    }

    /* 解析 WAV 头部 */
    const uint8_t* header = data;

    /* 验证 "RIFF" 和 "WAVE" */
    if (memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "不是有效的 WAV 文件");
        free(data);
        s_audio_playing = false;
        vTaskDelete(NULL);
        return;
    }

    /* 解析 fmt chunk */
    uint16_t audio_fmt   = header[20] | (header[21] << 8);
    uint16_t channels    = header[22] | (header[23] << 8);
    uint32_t sample_rate = header[24]       | (header[25] << 8)  |
                           (header[26] << 16) | (header[27] << 24);
    uint16_t bits        = header[34] | (header[35] << 8);

    if (audio_fmt != 1) {
        ESP_LOGE(TAG, "不支持压缩格式，仅支持 PCM (fmt=%d)", audio_fmt);
        free(data);
        s_audio_playing = false;
        vTaskDelete(NULL);
        return;
    }
    if (bits != 16) {
        ESP_LOGE(TAG, "仅支持 16-bit WAV（当前 %d bit）", bits);
        free(data);
        s_audio_playing = false;
        vTaskDelete(NULL);
        return;
    }

    // 查找 "data" chunk 的偏移（WAV 头部可能不正好 44 字节）
    size_t offset = 12;  // 跳过 RIFF 头
    size_t data_chunk_offset = 0;
    size_t data_chunk_size = 0;

    while (offset + 8 <= data_size) {
        uint32_t chunk_id   = *(const uint32_t*)(data + offset);
        uint32_t chunk_size = *(const uint32_t*)(data + offset + 4);
        if (chunk_id == 0x61746164) {  // "data" (little-endian)
            data_chunk_offset = offset + 8;
            data_chunk_size   = chunk_size;
            break;
        }
        offset += 8 + chunk_size;
    }

    if (data_chunk_offset == 0 || data_chunk_offset + data_chunk_size > data_size) {
        ESP_LOGE(TAG, "未找到有效的 data chunk");
        free(data);
        s_audio_playing = false;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "WAV 信息: %lu Hz, %d ch, %d bit, PCM 大小=%u",
             sample_rate, channels, bits, (unsigned)data_chunk_size);

    /* 初始化音频输出：优先走 codec 回调 */
    if (s_audio_write && s_audio_ctrl) {
        s_audio_ctrl(true);
        vTaskDelay(pdMS_TO_TICKS(20));
    } else {
        if (i2s_speaker_init() != ESP_OK) {
            free(data);
            s_audio_playing = false;
            vTaskDelete(NULL);
            return;
        }
    }

    s_audio_playing = true;

    /* 音量缩放缓冲区（栈上，WAV_BUF_SAMPLES * 2字节 = 512B）*/
    int16_t wav_scale_buf[WAV_BUF_SAMPLES];

    /* 从文件数据区读取 PCM */
    const int16_t* pcm_data = (const int16_t*)(data + data_chunk_offset);
    size_t pcm_samples = data_chunk_size / sizeof(int16_t);
    size_t pcm_pos = 0;
    size_t bytes_written;

    while (!s_choreo_stop_req && pcm_pos < pcm_samples) {
        size_t chunk_samples = WAV_BUF_SAMPLES;
        if (pcm_pos + chunk_samples > pcm_samples) {
            chunk_samples = pcm_samples - pcm_pos;
        }

        /* 拷贝到可写缓冲区，并缩放音量 */
        for (size_t i = 0; i < chunk_samples; i++) {
            wav_scale_buf[i] = (int16_t)(pcm_data[pcm_pos + i] * CHOREO_VOLUME_SCALE);
        }

        /* 输出 PCM：优先走 codec 回调 */
        if (s_audio_write) {
            s_audio_write(wav_scale_buf, (int)chunk_samples);
        } else {
            esp_err_t wr_ret = i2s_channel_write(s_i2s_tx_chan,
                                                  wav_scale_buf,
                                                  chunk_samples * sizeof(int16_t),
                                                  &bytes_written,
                                                  portMAX_DELAY);
            if (wr_ret != ESP_OK) {
                ESP_LOGW(TAG, "I2S 写入失败: %d", wr_ret);
                break;
            }
        }
        pcm_pos += chunk_samples;
    }

    /* 等待 DMA 播完 + 关闭输出 */
    vTaskDelay(pdMS_TO_TICKS(100));
    if (s_audio_ctrl) {
        s_audio_ctrl(false);
    } else {
        i2s_speaker_stop();
    }

    ESP_LOGI(TAG, "WAV 播放结束（%s）", s_choreo_stop_req ? "被中断" : "自然结束");
    s_audio_playing = false;

    /* 释放文件数据和任务参数 */
    free(data);
    free((void*)filepath);

    vTaskDelete(NULL);
}

/*============================================================================
  OGG Opus 播放任务（mmap 读 → OGG 解析 → esp_opus_dec 解码 → I2S 写 PCM）
  格式要求：OGG 容器，24kHz / mono / 60ms Opus 帧
============================================================================*/

/*============================================================================
  OGG Opus 播放任务（mmap 读 → OGG 解析 → esp_opus_dec 解码 → I2S 写 PCM）
  格式要求：OGG 容器，24kHz / mono / 60ms Opus 帧
============================================================================*/

/* opus_task 传参结构体（choreo_opus_play_async / choreo_opus_play_from_memory 分配，play_opus_task 内 free）*/
typedef struct {
    uint8_t* ogg_data;   /* OGG 文件完整内容（malloc，play_opus_task 内 free）*/
    size_t   ogg_size;   /* OGG 文件大小 */
} opus_task_args_t;

/* choreo_task 传参：将 OGG 文件预读到调用者上下文，
 * 避免 choreo_task 栈内 mmap 读取开销导致栈溢出。 */
typedef struct {
    choreo_routine_t routine;    /* 嵌入副本（含 steps 指针，choreo_task 内 free）*/
    uint8_t*         audio_data; /* 预读的音频数据（OGG），NULL=无音频（choreo_task 内 free）*/
    size_t           audio_size;
} choreo_task_arg_t;

#define OPUS_PCM_BUF_SIZE   (24000 / 1000 * 60)   /* 每帧 PCM 样本数: 24kHz * 60ms = 1440 */
#define OPUS_PCM_BUF_BYTES  (OPUS_PCM_BUF_SIZE * sizeof(int16_t))
#define OPUS_MAX_PACKET     8192                   /* 最大 Opus 包大小 */

/*--------------------------------------------------------------------------
  最小化 OGG 页解析器（C 语言实现）
  仅提取 Opus 原始数据包，跳过 OpusHead / OpusTags
--------------------------------------------------------------------------*/

typedef struct {
    uint8_t* buf;            /* 指向 OGG 文件数据（malloc 分配）*/
    size_t          size;     /* 文件总大小 */
    size_t          pos;      /* 当前读取位置 */

    /* 当前页状态 */
    uint8_t  seg_table[255];  /* 段表 */
    size_t   seg_count;       /* 段数 */
    size_t   seg_index;       /* 当前段索引（0..seg_count）*/
    size_t   seg_remain;      /* 当前段剩余未读字节 */

    /* packet 累积状态 */
    uint8_t  pkt_buf[OPUS_MAX_PACKET];
    size_t   pkt_len;         /* 已累积的包长度 */
    bool     pkt_continued;   /* 包是否跨页延续 */

    /* 元数据标记 */
    bool     head_seen;       /* 已看到 OpusHead */
    bool     tags_seen;       /* 已看到 OpusTags */
} ogg_state_t;

/* 前进到下一个 OGG 页，解析页头和段表。
 * 返回 true 表示成功定位到新页，false 表示 EOF 或格式错误。 */
static bool ogg_next_page(ogg_state_t* s) {
    /* 扫描 "OggS" 魔数 */
    while (s->pos + 4 <= s->size) {
        if (memcmp(s->buf + s->pos, "OggS", 4) == 0) {
            break;
        }
        s->pos++;
    }
    if (s->pos + 27 > s->size) return false;  /* EOF 或不足一个页头 */

    const uint8_t* hdr = s->buf + s->pos;
    if (hdr[4] != 0) {  /* 版本必须为 0 */
        s->pos++;
        return ogg_next_page(s);
    }

    s->seg_count = hdr[26];
    if (s->seg_count > 255) {
        s->pos++;
        return ogg_next_page(s);
    }

    /* 读取段表 */
    size_t hdr_end = 27 + s->seg_count;
    if (s->pos + hdr_end > s->size) return false;

    memcpy(s->seg_table, hdr + 27, s->seg_count);
    s->seg_index = 0;
    s->seg_remain = 0;
    s->pos += hdr_end;  /* pos 现在指向数据起始 */

    return true;
}

/* 从当前页读取下一个完整的 Opus packet。
 * 数据写入 s->pkt_buf，长度由 *pkt_size 返回。
 * 跳过 OpusHead / OpusTags 包。
 * 返回 0 表示成功，-1 表示页数据耗尽（调用者需 ogg_next_page 后重试），
 * -2 表示文件结束。 */
static int ogg_read_packet(ogg_state_t* s, size_t* pkt_size) {
    while (1) {
        if (s->seg_index >= s->seg_count) {
            /* 当前页数据用完 */
            if (!s->pkt_continued) {
                s->pkt_len = 0;  /* 未完成的跨页包不清零 */
            }
            return -1;  /* 需要下一页 */
        }

        /* 开始或继续一个段 */
        if (s->seg_remain == 0) {
            s->seg_remain = s->seg_table[s->seg_index];
        }

        /* 检查缓冲区 */
        if (s->pkt_len + s->seg_remain > OPUS_MAX_PACKET) {
            ESP_LOGE(TAG, "OGG: Opus 包溢出 (%zu + %zu > %d)",
                     s->pkt_len, (size_t)s->seg_remain, OPUS_MAX_PACKET);
            /* 丢弃当前包，跳过剩余段 */
            s->pkt_len = 0;
            s->pkt_continued = false;
            while (s->seg_index < s->seg_count && s->seg_table[s->seg_index] == 255) {
                s->seg_index++;
            }
            s->seg_index++;
            s->seg_remain = 0;
            continue;
        }

        /* 检查文件边界 */
        if (s->pos + s->seg_remain > s->size) {
            return -2;  /* 文件截断 */
        }

        /* 复制段数据到包缓冲区 */
        memcpy(s->pkt_buf + s->pkt_len, s->buf + s->pos, s->seg_remain);
        s->pkt_len += s->seg_remain;
        s->pos += s->seg_remain;

        bool seg_continued = (s->seg_table[s->seg_index] == 255);
        s->seg_index++;
        s->seg_remain = 0;

        if (seg_continued) {
            s->pkt_continued = true;
            continue;  /* 包跨段，继续累积 */
        }

        /* 包结束 */
        s->pkt_continued = false;

        if (s->pkt_len == 0) {
            continue;  /* 零长度段，跳过 */
        }

        /* 跳过 OpusHead */
        if (!s->head_seen) {
            if (s->pkt_len >= 8 && memcmp(s->pkt_buf, "OpusHead", 8) == 0) {
                s->head_seen = true;
                ESP_LOGI(TAG, "OGG: OpusHead 已识别");
                s->pkt_len = 0;
                continue;
            }
        }

        /* 跳过 OpusTags */
        if (!s->tags_seen) {
            if (s->pkt_len >= 8 && memcmp(s->pkt_buf, "OpusTags", 8) == 0) {
                s->tags_seen = true;
                ESP_LOGI(TAG, "OGG: OpusTags 已识别");
                s->pkt_len = 0;
                continue;
            }
        }

        /* 有效的 Opus 数据包 */
        if (s->head_seen && s->tags_seen) {
            *pkt_size = s->pkt_len;
            s->pkt_len = 0;
            return 0;
        }

        /* 还没看到 OpusHead/Tags，丢弃数据 */
        ESP_LOGW(TAG, "OGG: 跳过未知包 (%zu 字节)", s->pkt_len);
        s->pkt_len = 0;
    }
}

static void play_opus_task(void* arg) {
    opus_task_args_t* args = (opus_task_args_t*)arg;
    if (!args || !args->ogg_data || args->ogg_size == 0) {
        ESP_LOGE(TAG, "opus_task: 无效参数");
        s_audio_playing = false;
        if (args) free(args);
        goto opus_exit;
    }

    uint8_t* ogg_data = args->ogg_data;
    size_t   ogg_size = args->ogg_size;
    free(args);  /* 参数已提取，立即释放 */

    ESP_LOGI(TAG, "OGG Opus 播放: %lu 字节", (unsigned long)ogg_size);

    if (ogg_size == 0) {
        ESP_LOGE(TAG, "OGG 文件为空");
        free(ogg_data);
        s_audio_playing = false;
        goto opus_exit;
    }

    /* 初始化 OGG 状态（堆分配：内含 8KB pkt_buf，栈放不下）*/
    ogg_state_t* ogg = calloc(1, sizeof(ogg_state_t));
    if (!ogg) {
        ESP_LOGE(TAG, "OGG 状态分配失败");
        free(ogg_data);
        s_audio_playing = false;
        goto opus_exit;
    }
    ogg->buf  = ogg_data;
    ogg->size = ogg_size;
    ogg->pos  = 0;

    /* 定位到第一个 OGG 页 */
    if (!ogg_next_page(ogg)) {
        ESP_LOGE(TAG, "OGG: 未找到有效 OGG 页");
        free(ogg_data);
        free(ogg);
        s_audio_playing = false;
        goto opus_exit;
    }

    /* 创建 Opus 解码器（24kHz, mono, 60ms, self_delimited=false）*/
    esp_opus_dec_cfg_t opus_cfg = {
        .sample_rate    = 24000,
        .channel        = ESP_AUDIO_MONO,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS,
        .self_delimited = false,
    };
    void* opus_dec = NULL;
    esp_audio_err_t dec_ret = esp_opus_dec_open(&opus_cfg, sizeof(opus_cfg), &opus_dec);
    if (dec_ret != ESP_AUDIO_ERR_OK || opus_dec == NULL) {
        ESP_LOGE(TAG, "Opus 解码器打开失败: %d", dec_ret);
        free(ogg_data);
        free(ogg);
        s_audio_playing = false;
        goto opus_exit;
    }

    /* 初始化音频输出：优先用 codec 回调（无 I2S 冲突），回退到直驱 I2S */
    if (s_audio_write && s_audio_ctrl) {
        s_audio_ctrl(true);
        vTaskDelay(pdMS_TO_TICKS(20));
    } else {
        if (i2s_speaker_init() != ESP_OK) {
            esp_opus_dec_close(opus_dec);
            free(ogg_data);
            free(ogg);
            s_audio_playing = false;
            goto opus_exit;
        }
    }

    s_audio_playing = true;

    /* 分配 PCM 输出缓冲区 */
    int16_t* pcm_buf = malloc(OPUS_PCM_BUF_BYTES);
    if (!pcm_buf) {
        ESP_LOGE(TAG, "Opus PCM 缓冲区分配失败");
        esp_opus_dec_close(opus_dec);
        free(ogg_data);
        free(ogg);
        s_audio_playing = false;
        goto opus_exit;
    }

    esp_audio_dec_in_raw_t  raw = {0};
    esp_audio_dec_out_frame_t out = {0};
    esp_audio_dec_info_t   dec_info = {0};
    size_t bytes_written;
    int pkt_count = 0;

    int pkt_ok_count = 0;  /* 成功解码并输出的包数 */
    while (!s_choreo_stop_req) {
        /* 获取下一个 Opus 包 */
        size_t pkt_size = 0;
        int pkt_ret = ogg_read_packet(ogg, &pkt_size);

        if (pkt_ret == -1) {
            /* 当前页数据耗尽，尝试下一页 */
            if (!ogg_next_page(ogg)) {
                ESP_LOGI(TAG, "OGG: 文件结束，共 %d 包 (成功输出 %d)", pkt_count, pkt_ok_count);
                break;
            }
            continue;
        }

        if (pkt_ret == -2) {
            ESP_LOGW(TAG, "OGG: 文件意外截断 (已输出 %d 包)", pkt_ok_count);
            break;
        }

        if (pkt_size == 0) continue;

        /* 解码 */
        raw.buffer   = ogg->pkt_buf;
        raw.len      = (uint32_t)pkt_size;
        raw.consumed = 0;
        raw.frame_recover = ESP_AUDIO_DEC_RECOVERY_NONE;

        out.buffer       = (uint8_t*)pcm_buf;
        out.len          = OPUS_PCM_BUF_BYTES;
        out.decoded_size = 0;
        out.needed_size  = 0;

        esp_audio_err_t err = esp_opus_dec_decode(opus_dec, &raw, &out, &dec_info);

        if (err == ESP_AUDIO_ERR_OK && out.decoded_size > 0) {
            /* 音量缩放：原地修改 pcm_buf（int16_t 样本）*/
            int16_t* s = (int16_t*)pcm_buf;
            size_t n = out.decoded_size / sizeof(int16_t);
            for (size_t i = 0; i < n; i++) {
                s[i] = (int16_t)(s[i] * CHOREO_VOLUME_SCALE);
            }

            /* 输出 PCM：优先走 codec 回调，回退到直驱 I2S */
            if (s_audio_write) {
                int written = s_audio_write((const int16_t*)pcm_buf, (int)n);
                if (pkt_ok_count < 3) {
                    ESP_LOGI(TAG, "音频输出 #%d: %d samples, write_ret=%d", pkt_ok_count, (int)n, written);
                }
            } else {
                i2s_channel_write(s_i2s_tx_chan,
                                  pcm_buf,
                                  out.decoded_size,
                                  &bytes_written,
                                  portMAX_DELAY);
            }
            pkt_ok_count++;
        } else if (err != ESP_AUDIO_ERR_OK) {
            ESP_LOGW(TAG, "Opus 解码错误: %d (包 #%d, size=%zu)", err, pkt_count, pkt_size);
            /* 不中断，尝试继续解码后续包 */
        }

        pkt_count++;

        /* 每 100 包打印一次心跳（60ms 帧 → 每 6 秒），确认解码循环存活 */
        if (pkt_count % 100 == 0) {
            ESP_LOGI(TAG, "opus 心跳: %d 包已处理, %d 成功输出, free_heap=%lu",
                     pkt_count, pkt_ok_count,
                     (unsigned long)esp_get_free_heap_size());
        }

        /* 每帧 yield 10ms，让 AFE 有机会 fetch 音频数据，避免 ringbuffer 溢出 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* 等待 DMA 播完（回调路径只做逻辑延时，I2S 路径等硬件）*/
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 关闭音频输出 */
    if (s_audio_ctrl) {
        s_audio_ctrl(false);
    } else {
        i2s_speaker_stop();
    }

    free(pcm_buf);
    free(ogg_data);  /* ogg->buf 指向这里 */
    free(ogg);
    esp_opus_dec_close(opus_dec);

    ESP_LOGI(TAG, "OGG 播放结束（%s, %d 包）, 栈剩余=%lu words",
             s_choreo_stop_req ? "被中断" : "自然结束", pkt_count,
             (unsigned long)uxTaskGetStackHighWaterMark(NULL));
    s_audio_playing = false;
    /* fall through to opus_exit for SPIRAM stack/TCB cleanup */
opus_exit:
    /* 静态任务（xTaskCreateStatic）：
     * vTaskDelete 将任务标记为待回收。FreeRTOS idle task 在 prvDeleteTCB 中
     * 检测到 ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_AND_TCB，
     * 不会尝试 vPortFree 栈/TCB。外部 choreo_task 在确认 s_audio_playing==false
     * 后负责释放 SPIRAM 栈和 TCB。
     * 注意：vTaskDelete 不会返回，此行之后的代码不会执行。 */
    vTaskDelete(NULL);
}

esp_err_t choreo_opus_play_async(const char* filepath) {
    if (s_audio_playing) {
        ESP_LOGW(TAG, "音乐已在播放，先停止");
        choreo_wav_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* 1. 提前把 OGG 文件读到堆上（在任务创建之前，不占任务栈）*/
    uint8_t* ogg_data = NULL;
    size_t  ogg_size = 0;
    if (!choreo_read_file_chunked(filepath, &ogg_data, &ogg_size)) {
        ESP_LOGE(TAG, "无法读取 OGG 文件: %s", filepath);
        return ESP_FAIL;
    }
    if (ogg_size == 0) {
        ESP_LOGE(TAG, "OGG 文件为空");
        free(ogg_data);
        return ESP_FAIL;
    }

    /* 2. 打包参数（play_opus_task 内 free）*/
    opus_task_args_t* args = malloc(sizeof(opus_task_args_t));
    if (!args) {
        ESP_LOGE(TAG, "opus_task_args 分配失败");
        free(ogg_data);
        return ESP_ERR_NO_MEM;
    }
    args->ogg_data = ogg_data;
    args->ogg_size = ogg_size;

    /* 3. 创建任务：libopus 解码器需要大量栈（备份版实测 12288w=48KB 可用）。
     *    蓝牙初始化后 internal SRAM 碎片化，显式从 SPIRAM 分配栈避免 48KB 连续块找不到。 */
    StackType_t* stack = heap_caps_malloc(12288 * sizeof(StackType_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!stack) {
        ESP_LOGE(TAG, "opus_task SPIRAM 栈分配失败! free_spiram=%lu",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        free(ogg_data);
        free(args);
        return ESP_ERR_NO_MEM;
    }
    StaticTask_t* tcb = heap_caps_malloc(sizeof(StaticTask_t),
                                          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!tcb) {
        free(stack);
        free(ogg_data);
        free(args);
        return ESP_ERR_NO_MEM;
    }

    TaskHandle_t handle = xTaskCreateStaticPinnedToCore(
        play_opus_task,
        "opus_task",
        12288,
        args,
        5,
        stack,
        tcb,
        1
    );

    if (!handle) {
        ESP_LOGE(TAG, "创建 opus_task 失败! ret=0, internal_free=%lu",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        free(stack);
        free(tcb);
        free(ogg_data);
        free(args);
        return ESP_FAIL;
    }

    s_opus_task_handle = handle;
    s_opus_stack = stack;
    s_opus_tcb   = tcb;

    ESP_LOGI(TAG, "opus_task 创建成功 (SPIRAM 栈=48KB)");
    return ESP_OK;
}

/* 同 choreo_opus_play_async，但跳过 mmap 读取，直接使用预读数据。
 * ogg_data 所有权转移：play_opus_task 内 free。
 * 用于 choreo_task 等栈受限的调用者。 */
static esp_err_t choreo_opus_play_from_memory(uint8_t* ogg_data, size_t ogg_size) {
    if (s_audio_playing) {
        ESP_LOGW(TAG, "音乐已在播放，先停止");
        choreo_wav_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!ogg_data || ogg_size == 0) {
        ESP_LOGE(TAG, "opus_play_from_memory: 无效数据");
        if (ogg_data) free(ogg_data);
        return ESP_FAIL;
    }

    /* 打包参数（play_opus_task 内 free）*/
    opus_task_args_t* args = malloc(sizeof(opus_task_args_t));
    if (!args) {
        ESP_LOGE(TAG, "opus_task_args 分配失败");
        free(ogg_data);
        return ESP_ERR_NO_MEM;
    }
    args->ogg_data = ogg_data;
    args->ogg_size = ogg_size;

    /* 同 choreo_opus_play_async：SPIRAM 分配 48KB 栈 */
    StackType_t* stack = heap_caps_malloc(12288 * sizeof(StackType_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!stack) {
        ESP_LOGE(TAG, "opus_task(from_memory) SPIRAM 栈分配失败! free_spiram=%lu",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        free(ogg_data);
        free(args);
        return ESP_ERR_NO_MEM;
    }
    StaticTask_t* tcb = heap_caps_malloc(sizeof(StaticTask_t),
                                          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!tcb) {
        free(stack);
        free(ogg_data);
        free(args);
        return ESP_ERR_NO_MEM;
    }

    TaskHandle_t handle = xTaskCreateStaticPinnedToCore(
        play_opus_task,
        "opus_task",
        12288,
        args,
        5,
        stack,
        tcb,
        1
    );

    if (!handle) {
        ESP_LOGE(TAG, "创建 opus_task 失败! ret=0, internal_free=%lu",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        free(stack);
        free(tcb);
        free(ogg_data);
        free(args);
        return ESP_FAIL;
    }

    s_opus_task_handle = handle;
    s_opus_stack = stack;
    s_opus_tcb   = tcb;

    ESP_LOGI(TAG, "opus_task 创建成功 (预读, SPIRAM 栈=48KB)");
    return ESP_OK;
}

esp_err_t choreo_wav_play_async(const char* filepath) {
    if (s_audio_playing) {
        ESP_LOGW(TAG, "WAV 已在播放，先停止");
        choreo_wav_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* 复制文件路径到任务参数 */
    char* fp = strdup(filepath);
    if (!fp) return ESP_ERR_NO_MEM;

    BaseType_t ret = xTaskCreatePinnedToCore(
        play_wav_task,
        "wav_task",
        1024,       /* 1KB words = 4KB，WAV 播放只有栈上 512B buf + I2S 写入 */
        fp,
        5,
        &s_wav_task_handle,
        1   /* APP_CPU */
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 wav_task 失败! ret=%d, internal_free=%lu",
                 (int)ret, (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        free(fp);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "wav_task 创建成功");
    return ESP_OK;
}

void choreo_wav_stop(void) {
    s_choreo_stop_req = true;
    int wait = 0;
    while (s_audio_playing && wait < 50) {
        vTaskDelay(pdMS_TO_TICKS(10));
        wait++;
    }
    s_wav_task_handle = NULL;
    s_opus_task_handle = NULL;
    s_audio_playing = false;
}

bool choreo_wav_is_playing(void) {
    return s_audio_playing;
}

/*============================================================================
  JSON 解析
============================================================================*/

static choreo_direction_t parse_direction(const cJSON* j) {
    if (!j) return CHOREO_DIR_FORWARD;
    const char* s = cJSON_GetStringValue(j);
    if (!s) return CHOREO_DIR_FORWARD;
    if (strcmp(s, "backward") == 0) return CHOREO_DIR_BACKWARD;
    if (strcmp(s, "left")     == 0) return CHOREO_DIR_LEFT;
    if (strcmp(s, "right")    == 0) return CHOREO_DIR_RIGHT;
    return CHOREO_DIR_FORWARD;
}

/* 将 JSON "type" 字符串映射为 choreo_step_type_t */
static choreo_step_type_t parse_step_type(const char* type_str) {
    if (!type_str) return CHOREO_STEP_PAUSE;
    if (strcmp(type_str, "pause")     == 0) return CHOREO_STEP_PAUSE;
    if (strcmp(type_str, "walk")      == 0) return CHOREO_STEP_WALK;
    if (strcmp(type_str, "sway_fb")    == 0) return CHOREO_STEP_SWAY_FB;
    if (strcmp(type_str, "sway_lr")    == 0) return CHOREO_STEP_SWAY_LR;
    if (strcmp(type_str, "sway_twist") == 0) return CHOREO_STEP_SWAY_TWIST;
    if (strcmp(type_str, "sway_updown")== 0) return CHOREO_STEP_SWAY_UPDOWN;
    if (strcmp(type_str, "sway_side_l") == 0) return CHOREO_STEP_SWAY_SIDE_L;
    if (strcmp(type_str, "sway_side_r") == 0) return CHOREO_STEP_SWAY_SIDE_R;
    if (strcmp(type_str, "sway_wave")  == 0) return CHOREO_STEP_SWAY_WAVE;
    if (strcmp(type_str, "sway_march") == 0) return CHOREO_STEP_SWAY_MARCH;
    if (strcmp(type_str, "sway_nod")   == 0) return CHOREO_STEP_SWAY_NOD;
    if (strcmp(type_str, "sway_tremble")==0) return CHOREO_STEP_SWAY_TREMBLE;
    return CHOREO_STEP_PAUSE;
}

/* 从 JSON 对象解析单个舞步 */
static bool parse_step(const cJSON* step_json, choreo_step_t* out) {
    if (!step_json || !out) return false;

    cJSON* j_type = cJSON_GetObjectItem(step_json, "type");
    if (!j_type) return false;

    const char* type_str = cJSON_GetStringValue(j_type);
    out->type = parse_step_type(type_str);
    out->speed = 1.0f;

    cJSON* j_dur  = cJSON_GetObjectItem(step_json, "duration_ms");
    out->duration_ms = j_dur ? j_dur->valueint : 0;

    cJSON* j_params = cJSON_GetObjectItem(step_json, "params");
    if (!j_params) return true;  /* PAUSE 等无 params */

    switch (out->type) {
        case CHOREO_STEP_WALK: {
            cJSON* j_steps = cJSON_GetObjectItem(j_params, "steps");
            cJSON* j_dir   = cJSON_GetObjectItem(j_params, "direction");
            out->p.walk.steps     = j_steps ? j_steps->valueint : 1;
            out->p.walk.direction  = parse_direction(j_dir);
            break;
        }
        case CHOREO_STEP_SWAY_FB:
        case CHOREO_STEP_SWAY_TWIST:
        case CHOREO_STEP_SWAY_UPDOWN: {
            cJSON* j_amp    = cJSON_GetObjectItem(j_params, "amplitude");
            cJSON* j_cycles = cJSON_GetObjectItem(j_params, "cycles");
            cJSON* j_half   = cJSON_GetObjectItem(j_params, "half_ms");
            out->p.sway.amplitude = j_amp    ? j_amp->valueint    : 15;
            out->p.sway.cycles    = j_cycles ? j_cycles->valueint : 2;
            out->p.sway.dir       = 1;
            out->p.sway.step_ms   = 250;
            out->p.sway.half_ms   = j_half ? j_half->valueint : 250;
            break;
        }
        case CHOREO_STEP_SWAY_LR:
        case CHOREO_STEP_SWAY_SIDE_L:
        case CHOREO_STEP_SWAY_SIDE_R: {
            cJSON* j_amp    = cJSON_GetObjectItem(j_params, "amplitude");
            cJSON* j_cycles = cJSON_GetObjectItem(j_params, "cycles");
            cJSON* j_dir    = cJSON_GetObjectItem(j_params, "dir");
            cJSON* j_half   = cJSON_GetObjectItem(j_params, "half_ms");
            out->p.sway.amplitude = j_amp    ? j_amp->valueint    : 45;
            out->p.sway.cycles    = j_cycles ? j_cycles->valueint : 2;
            out->p.sway.dir       = j_dir    ? j_dir->valueint    : 1;
            out->p.sway.step_ms   = 250;
            out->p.sway.half_ms   = j_half ? j_half->valueint : 250;
            break;
        }
        case CHOREO_STEP_SWAY_WAVE:
        case CHOREO_STEP_SWAY_MARCH:
        case CHOREO_STEP_SWAY_NOD:
        case CHOREO_STEP_SWAY_TREMBLE: {
            cJSON* j_amp    = cJSON_GetObjectItem(j_params, "amplitude");
            cJSON* j_cycles = cJSON_GetObjectItem(j_params, "cycles");
            cJSON* j_step   = cJSON_GetObjectItem(j_params, "step_ms");
            cJSON* j_half   = cJSON_GetObjectItem(j_params, "half_ms");
            out->p.sway.amplitude = j_amp    ? j_amp->valueint    : 20;
            out->p.sway.cycles    = j_cycles ? j_cycles->valueint : 2;
            out->p.sway.dir       = 1;
            out->p.sway.step_ms   = j_step ? j_step->valueint : 250;
            out->p.sway.half_ms   = j_half ? j_half->valueint : 250;
            break;
        }
        default:
            break;
    }

    cJSON* j_speed = cJSON_GetObjectItem(step_json, "speed");
    if (j_speed) out->speed = (float)j_speed->valuedouble;

    return true;
}

choreo_routine_t* choreo_load(const char* name) {
    if (!name) return NULL;

    cJSON* root = NULL;
    char*  json_buf = NULL;
    size_t json_len = 0;

    /* 策略1：直接用 <name>.json 作为 asset name 读取
     * 适用于 name 为文件名（如 "jiaqiwu" 或 "dance_01"） */
    char asset_name[64];
    snprintf(asset_name, sizeof(asset_name), "%s.json", name);

    uint8_t* data = NULL;
    size_t size = 0;
    if (choreo_read_file(asset_name, &data, &size) && size > 0) {
        json_buf = (char*)malloc(size + 1);
        if (json_buf) {
            memcpy(json_buf, data, size);
            json_buf[size] = '\0';
            json_len = size;
        }
        free(data);
        root = cJSON_Parse(json_buf);
    }

    /* 策略2：读取 choreo_index.json，用 "name" 字段匹配获取 filename
     * 适用于 name 为显示名（如 "嘉祺舞"） */
    if (!root) {
        uint8_t* idx_data = NULL;
        size_t idx_size = 0;
        if (choreo_read_file("choreo_index.json", &idx_data, &idx_size) && idx_size > 0) {
            char* idx_buf = (char*)malloc(idx_size + 1);
            if (idx_buf) {
                memcpy(idx_buf, idx_data, idx_size);
                idx_buf[idx_size] = '\0';
                cJSON* idx_root = cJSON_Parse(idx_buf);
                free(idx_buf);

                if (idx_root) {
                    cJSON* dances = cJSON_GetObjectItem(idx_root, "dances");
                    if (dances && cJSON_IsArray(dances)) {
                        cJSON* dance;
                        cJSON_ArrayForEach(dance, dances) {
                            cJSON* j_name = cJSON_GetObjectItem(dance, "name");
                            if (j_name && cJSON_IsString(j_name) &&
                                strcmp(cJSON_GetStringValue(j_name), name) == 0) {
                                cJSON* j_file = cJSON_GetObjectItem(dance, "filename");
                                if (j_file && cJSON_IsString(j_file)) {
                                    uint8_t* d2 = NULL;
                                    size_t s2 = 0;
                                    if (choreo_read_file(cJSON_GetStringValue(j_file), &d2, &s2) && s2 > 0) {
                                        if (json_buf) free(json_buf);
                                        json_buf = (char*)malloc(s2 + 1);
                                        if (json_buf) {
                                            memcpy(json_buf, d2, s2);
                                            json_buf[s2] = '\0';
                                            json_len = s2;
                                        }
                                        root = cJSON_Parse(json_buf);
                                        free(d2);
                                    }
                                }
                                break;
                            }
                        }
                    }
                    cJSON_Delete(idx_root);
                }
            }
            free(idx_data);
        }
    }

    if (!root) {
        ESP_LOGE(TAG, "未找到编排: \"%s\"", name);
        if (json_buf) free(json_buf);
        return NULL;
    }

    ESP_LOGI(TAG, "加载 \"%s\"（长度=%u 字节）", name, (unsigned int)json_len);

    /* Phase 2: 从已匹配的 root 解析完整编排 */

    choreo_routine_t* routine = calloc(1, sizeof(choreo_routine_t));
    if (!routine) {
        cJSON_Delete(root);
        if (json_buf) free(json_buf);
        return NULL;
    }

    cJSON* j_name = cJSON_GetObjectItem(root, "name");
    cJSON* j_music = cJSON_GetObjectItem(root, "music");
    cJSON* j_bpm  = cJSON_GetObjectItem(root, "bpm");
    cJSON* j_spd  = cJSON_GetObjectItem(root, "speed");
    cJSON* j_off  = cJSON_GetObjectItem(root, "offset_ms");
    cJSON* j_steps = cJSON_GetObjectItem(root, "steps");

    if (j_name) {
        strncpy(routine->name, cJSON_GetStringValue(j_name), sizeof(routine->name) - 1);
        routine->name[sizeof(routine->name) - 1] = '\0';
    }
    if (j_music) {
        strncpy(routine->music, cJSON_GetStringValue(j_music), sizeof(routine->music) - 1);
        routine->music[sizeof(routine->music) - 1] = '\0';
    }
    routine->bpm      = j_bpm  ? j_bpm->valueint : 120;
    routine->speed     = j_spd  ? (float)j_spd->valuedouble : 1.0f;
    routine->offset_ms = j_off  ? j_off->valueint : 0;

    if (!j_steps || !cJSON_IsArray(j_steps)) {
        ESP_LOGE(TAG, "缺少 steps 数组");
        free(routine);
        cJSON_Delete(root);
        if (json_buf) free(json_buf);
        return NULL;
    }

    routine->step_count = cJSON_GetArraySize(j_steps);
    routine->steps = calloc(routine->step_count, sizeof(choreo_step_t));
    if (!routine->steps) {
        free(routine);
        cJSON_Delete(root);
        if (json_buf) free(json_buf);
        return NULL;
    }

    for (int i = 0; i < routine->step_count; i++) {
        cJSON* step_json = cJSON_GetArrayItem(j_steps, i);
        if (!cJSON_IsObject(step_json)) continue;  /* 跳过分隔注释字符串 */
        if (!parse_step(step_json, &routine->steps[i])) {
            ESP_LOGW(TAG, "第 %d 步解析失败，跳过", i);
        }
    }

    cJSON_Delete(root);
    if (json_buf) free(json_buf);

    ESP_LOGI(TAG, "舞蹈编排加载成功: %s, BPM=%d, speed=%.2f, steps=%d",
             routine->name, routine->bpm, routine->speed, routine->step_count);

    return routine;
}

/*============================================================================
  舞步执行
============================================================================*/

static void execute_step(const choreo_step_t* step, float global_speed) {
    if (!step) return;

    float effective_speed = global_speed * step->speed;

    switch (step->type) {
        case CHOREO_STEP_PAUSE: {
            int scaled_ms = step->duration_ms > 0
                        ? (int)(step->duration_ms / effective_speed)
                        : 0;
            if (scaled_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(scaled_ms));
            }
            break;
        }

        case CHOREO_STEP_WALK: {
            /* walk 暂不使用 speed 缩放（servo_one_step 固定 1500ms） */
            int steps = step->p.walk.steps;
            switch (step->p.walk.direction) {
                case CHOREO_DIR_FORWARD:  Dog_ForwardSteps(steps); break;
                case CHOREO_DIR_BACKWARD: Dog_BackwardSteps(steps); break;
                case CHOREO_DIR_LEFT:     Dog_TurnLeftSteps(steps);  break;
                case CHOREO_DIR_RIGHT:    Dog_TurnRightSteps(steps); break;
            }
            break;
        }

        /* ---- 标准摇摆：2 半拍/cycle，half_ms 受 speed 缩放 ---- */
        case CHOREO_STEP_SWAY_FB: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingFB(step->p.sway.cycles, half);
            break;
        }

        case CHOREO_STEP_SWAY_LR: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingLR(step->p.sway.dir, step->p.sway.cycles, half);
            break;
        }

        case CHOREO_STEP_SWAY_TWIST: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingTwist(step->p.sway.cycles, half);
            break;
        }

        case CHOREO_STEP_SWAY_UPDOWN: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingUpDown(step->p.sway.cycles, half);
            break;
        }

        case CHOREO_STEP_SWAY_SIDE_L: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingSideLeft(step->p.sway.cycles, half);
            break;
        }

        case CHOREO_STEP_SWAY_SIDE_R: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingSideRight(step->p.sway.cycles, half);
            break;
        }

        /* ---- 波浪步：step_ms 受 speed 缩放 ---- */
        case CHOREO_STEP_SWAY_WAVE: {
            int step_t = (int)(step->p.sway.step_ms / effective_speed);
            if (step_t < 50) step_t = 50;
            Dog_SwingWave(step->p.sway.cycles, step_t);
            break;
        }

        case CHOREO_STEP_SWAY_MARCH: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingMarch(step->p.sway.cycles, half);
            break;
        }

        case CHOREO_STEP_SWAY_NOD: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingNod(step->p.sway.cycles, half);
            break;
        }

        case CHOREO_STEP_SWAY_TREMBLE: {
            int half = (int)(step->p.sway.half_ms / effective_speed);
            if (half < 50) half = 50;
            Dog_SwingTremble(step->p.sway.cycles, half);
            break;
        }

        default:
            break;
    }
}

/*============================================================================
  编排执行
============================================================================*/

static void choreo_task(void* arg) {
    choreo_task_arg_t* task_arg = (choreo_task_arg_t*)arg;
    choreo_routine_t*  routine  = &task_arg->routine;

    s_choreo_playing  = true;
    s_choreo_stop_req = false;

    ESP_LOGI(TAG, "开始舞蹈: %s", routine->name);

    /* 1. 启动音乐播放（异步）。
     *    OGG 数据已在 choreo_play_async 中预读到 task_arg->audio_data，
     *    此处直接传递，无需 fopen（避免 mmap 拷贝吃掉 choreo_task 栈）。 */
    if (strlen(routine->music) > 0) {
        esp_err_t audio_ret = ESP_FAIL;
        if (strstr(routine->music, ".ogg") || strstr(routine->music, ".OGG")) {
            /* 使用预读数据，所有权转移给 opus_task */
            audio_ret = choreo_opus_play_from_memory(task_arg->audio_data, task_arg->audio_size);
            if (audio_ret == ESP_OK) {
                task_arg->audio_data = NULL;  /* 所有权已转移 */
            }
        } else {
            audio_ret = choreo_wav_play_async(routine->music);
        }
        if (audio_ret != ESP_OK) {
            ESP_LOGE(TAG, "音频任务启动失败: %d (music=%s)", (int)audio_ret, routine->music);
        }
    }

    /* 2. 等待音频真正开始播放（解码器初始化 + I2S 启动需要时间）*/
    if (strlen(routine->music) > 0) {
        int wait_audio = 0;
        while (!s_audio_playing && wait_audio < 200) {  /* 最多等 2s，步进 10ms */
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_audio++;
        }
        if (s_audio_playing) {
            ESP_LOGI(TAG, "音频已就绪，等待 %dms", wait_audio * 10);
        } else {
            ESP_LOGW(TAG, "音频未在 2s 内就绪，直接开始舞步");
        }
    }

    /* 音频启动完成后，重新复位停止标志。
     * choreo_opus_play_async / choreo_wav_play_async 内部会在
     * s_audio_playing 残留时调用 choreo_wav_stop()，
     * 后者会把 s_choreo_stop_req 意外置为 true，
     * 导致下面的舞步循环直接 break → 立即归位。 */
    s_choreo_stop_req = false;
    ESP_LOGI(TAG, "诊断: s_choreo_stop_req=%d, step_count=%d, audio=%d",
             (int)s_choreo_stop_req, routine->step_count, (int)s_audio_playing);

    if (routine->step_count == 0) {
        ESP_LOGW(TAG, "警告: step_count=0，舞步文件可能解析异常！");
    }

    /* 3. 等待 offset_ms（艺术性延迟，比如等音乐前奏过去）*/
    if (routine->offset_ms > 0) {
        ESP_LOGI(TAG, "等待 offset_ms=%d", routine->offset_ms);
        vTaskDelay(pdMS_TO_TICKS(routine->offset_ms));
    }

    /* 4. 计算 BPM 自适应速度: 基准 120BPM → speed=1.0
     *    例如 bpm=123 → 123/120=1.025x，所有 half_ms/step_ms 自动缩放 */
    float bpm_speed = routine->speed * (routine->bpm / 120.0f);
    ESP_LOGI(TAG, "BPM=%d, 自适应速度=%.3f (speed=%.2f × bpm/120)",
             routine->bpm, bpm_speed, routine->speed);

    for (int i = 0; i < routine->step_count; i++) {
        if (s_choreo_stop_req) {
            ESP_LOGI(TAG, "舞蹈被中断 (step %d/%d)", i + 1, routine->step_count);
            break;
        }

        ESP_LOGI(TAG, "[%d/%d] 执行: type=%d duration_ms=%d, bpm_speed=%.3f",
                     i + 1, routine->step_count,
                     routine->steps[i].type, routine->steps[i].duration_ms, bpm_speed);
        execute_step(&routine->steps[i], bpm_speed);
    }

    /* 5. 等待音乐播完（如果还在播放）*/
    int wait_count = 0;
    while (s_audio_playing && wait_count < 600) {  /* 最多等 60s */
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }
    if (wait_count > 0) {
        ESP_LOGI(TAG, "等待音频结束: %d.%d 秒, s_audio_playing=%d",
                 wait_count / 10, wait_count % 10, (int)s_audio_playing);
    }

    /* 5b. 回收 opus_task 的 SPIRAM 栈和 TCB。
     *     opus_task 通过 vTaskDelete(NULL) 结束，FreeRTOS idle task
     *     在 prvDeleteTCB 中检测到 ucStaticallyAllocated ==
     *     tskSTATICALLY_ALLOCATED_STACK_AND_TCB，不会释放栈/TCB。
     *     必须由创建者 choreo_task 在确认 s_audio_playing==false 后手动释放。
     *     注意：需给 idle task 一点时间完成 TCB 清理。 */
    vTaskDelay(pdMS_TO_TICKS(50));
    if (s_opus_stack) {
        free(s_opus_stack);
        s_opus_stack = NULL;
    }
    if (s_opus_tcb) {
        free(s_opus_tcb);
        s_opus_tcb = NULL;
    }
    s_opus_task_handle = NULL;

    /* 6. 归位 */
    Dog_ResetAll();
    ESP_LOGI(TAG, "舞蹈结束，已归位");

    s_choreo_playing = false;
    s_choreo_task_handle = NULL;

    /* routine 嵌入在 task_arg 中，手动释放 steps */
    if (task_arg->routine.steps) {
        free(task_arg->routine.steps);
    }
    /* 如果 audio_data 未转移（如 WAV 或无音频），释放之 */
    if (task_arg->audio_data) {
        free(task_arg->audio_data);
    }
    free(task_arg);
    vTaskDelete(NULL);
}

esp_err_t choreo_play_async(choreo_routine_t* routine, const char* task_name) {
    if (!routine) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "choreo_play_async: %s, step_count=%d, s_playing=%d, s_stop_req=%d",
             routine->name, routine->step_count,
             (int)s_choreo_playing, (int)s_choreo_stop_req);
    if (s_choreo_playing) {
        ESP_LOGW(TAG, "已有舞蹈在执行，先停止");
        choreo_stop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    const char* name = task_name ? task_name : "choreo_task";

    /* 分配包装结构体（choreo_task 内 free）*/
    choreo_task_arg_t* task_arg = calloc(1, sizeof(choreo_task_arg_t));
    if (!task_arg) return ESP_ERR_NO_MEM;

    /* 浅拷贝 routine（steps 指针共享，choreo_task 负责 free steps）*/
    task_arg->routine = *routine;
    task_arg->audio_data = NULL;
    task_arg->audio_size = 0;

    /* 预读 OGG 音频文件：在调用者上下文（栈较大）执行 mmap 读取，
     * 避免 mmap 拷贝栈开销压垮 choreo_task（仅 6KB 栈）。 */
    if (strlen(routine->music) > 0 &&
        (strstr(routine->music, ".ogg") || strstr(routine->music, ".OGG"))) {
        if (!choreo_read_file_chunked(routine->music, &task_arg->audio_data, &task_arg->audio_size)) {
            ESP_LOGW(TAG, "预读 OGG 失败: %s (舞步仍执行，无音乐)", routine->music);
            task_arg->audio_data = NULL;
            task_arg->audio_size = 0;
        } else {
            ESP_LOGI(TAG, "OGG 预读完成: %s (%lu 字节)",
                     routine->music, (unsigned long)task_arg->audio_size);
        }
    }

    /* 提前标记 playing */
    s_choreo_playing  = true;
    s_choreo_stop_req = false;

    /* 诊断 */
    size_t sram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "创建舞蹈任务前: internal_free=%lu, free_heap=%lu",
             (unsigned long)sram_free, (unsigned long)esp_get_free_heap_size());

    /* choreo_task 栈 1536w(6KB)：mmap 读取已移出，仅剩舞步循环 + 状态管理 */
    BaseType_t ret = xTaskCreatePinnedToCore(
        choreo_task,
        name,
        1536,
        task_arg,
        5,
        &s_choreo_task_handle,
        1
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建舞蹈任务失败! ret=%d, internal_free=%lu",
                 (int)ret, (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        if (task_arg->audio_data) free(task_arg->audio_data);
        if (task_arg->routine.steps) free(task_arg->routine.steps);
        free(task_arg);
        s_choreo_playing = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "choreo_task 创建成功 (handle=%p)", s_choreo_task_handle);

    return ESP_OK;
}

void choreo_stop(void) {
    s_choreo_stop_req = true;
    choreo_wav_stop();

    /* 等待任务退出（最多 3 秒）*/
    int wait = 0;
    while (s_choreo_playing && wait < 30) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait++;
    }

    if (s_choreo_playing) {
        ESP_LOGW(TAG, "舞蹈任务未能及时退出");
    }
}

bool choreo_is_playing(void) {
    return s_choreo_playing;
}

/*============================================================================
  舞蹈编排列表（从 mmap assets 的 choreo_index.json 读取）
============================================================================*/


choreo_name_list_t choreo_list_names(void) {
    choreo_name_list_t result = {0, NULL};

    /* 读取 choreo_index.json 索引文件 */
    uint8_t* idx_data = NULL;
    size_t idx_size = 0;
    if (!choreo_read_file("choreo_index.json", &idx_data, &idx_size) || idx_size == 0) {
        ESP_LOGW(TAG, "choreo_index.json 未找到");
        return result;
    }

    char* idx_buf = (char*)malloc(idx_size + 1);
    if (!idx_buf) { free(idx_data); return result; }
    memcpy(idx_buf, idx_data, idx_size);
    idx_buf[idx_size] = '\0';
    free(idx_data);

    cJSON* idx_root = cJSON_Parse(idx_buf);
    free(idx_buf);
    if (!idx_root) {
        ESP_LOGE(TAG, "choreo_index.json 解析失败");
        return result;
    }

    cJSON* dances = cJSON_GetObjectItem(idx_root, "dances");
    if (!dances || !cJSON_IsArray(dances)) {
        ESP_LOGW(TAG, "choreo_index.json 中无 dances 数组");
        cJSON_Delete(idx_root);
        return result;
    }

    int count = cJSON_GetArraySize(dances);
    if (count == 0) {
        cJSON_Delete(idx_root);
        return result;
    }

    result.names     = (char**)calloc(count, sizeof(char*));
    result.filenames = (char**)calloc(count, sizeof(char*));
    if (!result.names || !result.filenames) {
        free(result.names);
        free(result.filenames);
        result.names     = NULL;
        result.filenames = NULL;
        cJSON_Delete(idx_root);
        return result;
    }

    int idx = 0;
    cJSON* dance;
    cJSON_ArrayForEach(dance, dances) {
        cJSON* j_name = cJSON_GetObjectItem(dance, "name");
        cJSON* j_file = cJSON_GetObjectItem(dance, "filename");

        if (j_name && cJSON_IsString(j_name)) {
            result.names[idx] = strdup(cJSON_GetStringValue(j_name));
        }
        if (j_file && cJSON_IsString(j_file)) {
            result.filenames[idx] = strdup(cJSON_GetStringValue(j_file));
        }
        idx++;
    }

    result.count = idx;
    cJSON_Delete(idx_root);

    ESP_LOGI(TAG, "发现 %d 个舞蹈编排", result.count);
    return result;
}

void choreo_name_list_free(choreo_name_list_t* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free(list->names[i]);
        free(list->filenames[i]);
    }
    free(list->names);
    free(list->filenames);
    list->names     = NULL;
    list->filenames = NULL;
    list->count      = 0;
}

/*============================================================================
  初始化 / 释放
============================================================================*/

esp_err_t choreo_init(void) {
    /* mmap assets 分区由官方 Assets 模块统一管理（Assets::GetInstance()），
     * 此处无需挂载 SPIFFS，仅做日志确认。 */
    ESP_LOGI(TAG, "choreo 模块初始化完成（mmap assets 只读模式）");
    return ESP_OK;
}

void choreo_free(choreo_routine_t* routine) {
    if (!routine) return;
    if (routine->steps) {
        free(routine->steps);
        routine->steps = NULL;
    }
    free(routine);
}
