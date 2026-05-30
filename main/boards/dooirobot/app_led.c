#include "app_led.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "led_strip.h"

static const char *TAG = "APP_LED";

/* ================= 配置参数 ================= */
#define LED_TASK_STACK_SIZE     1024        
#define LED_TASK_PRIORITY       2     
#define LED_QUEUE_SIZE          2          
#define UPDATE_INTERVAL_MS      20                  // 刷新率 50Hz
#define LED_RMT_RESOLUTION_HZ   (10 * 1000 * 1000)  // 10MHz Resolution

/* ================= 内部数据结构 ================= */

typedef struct {
    led_effect_type_t type;
    struct {
        uint32_t colors[2]; // [0]Left, [1]Right
        uint16_t period;
        bool     symmetric;
    } data;
} led_cmd_t;

typedef struct {
    led_strip_handle_t strip_left;
    led_strip_handle_t strip_right;
    QueueHandle_t      cmd_queue;
    TaskHandle_t       task_handle;

    // 运行时状态
    led_cmd_t          current_effect;
    uint32_t           tick_count;
    bool               is_animating;
    
    // 缓存上一帧颜色，用于去重 (减少硬件IO)
    uint32_t           last_color_l;
    uint32_t           last_color_r;
} led_ctx_t;

static led_ctx_t g_led_ctx = {0};


/* 自动生成的彩虹表 (HSV色相环 GRB格式) */
const uint32_t rainbow_table[360] = {
    /* 000° */ 
    0x00FF00, 0x04FF00, 0x08FF00, 0x0DFF00, 0x11FF00, 0x15FF00, 0x19FF00, 0x1EFF00, 0x22FF00, 0x26FF00, 
    0x2AFF00, 0x2FFF00, 0x33FF00, 0x37FF00, 0x3CFF00, 0x40FF00, 0x44FF00, 0x48FF00, 0x4DFF00, 0x51FF00, 
    0x55FF00, 0x59FF00, 0x5EFF00, 0x62FF00, 0x66FF00, 0x6AFF00, 0x6EFF00, 0x73FF00, 0x77FF00, 0x7BFF00, 
    0x80FF00, 0x84FF00, 0x88FF00, 0x8CFF00, 0x90FF00, 0x95FF00, 0x99FF00, 0x9DFF00, 0xA2FF00, 0xA6FF00, 
    0xAAFF00, 0xAEFF00, 0xB2FF00, 0xB7FF00, 0xBBFF00, 0xBFFF00, 0xC3FF00, 0xC8FF00, 0xCCFF00, 0xD0FF00, 
    0xD4FF00, 0xD9FF00, 0xDDFF00, 0xE1FF00, 0xE5FF00, 0xEAFF00, 0xEEFF00, 0xF2FF00, 0xF7FF00, 0xFBFF00, 
    /* 060° */ 
    0xFFFF00, 0xFFFB00, 0xFFF700, 0xFFF200, 0xFFEE00, 0xFFEA00, 0xFFE600, 0xFFE100, 0xFFDD00, 0xFFD900,
    0xFFD400, 0xFFD000, 0xFFCC00, 0xFFC800, 0xFFC300, 0xFFBF00, 0xFFBB00, 0xFFB700, 0xFFB200, 0xFFAE00,
    0xFFAA00, 0xFFA600, 0xFFA200, 0xFF9D00, 0xFF9900, 0xFF9500, 0xFF9000, 0xFF8C00, 0xFF8800, 0xFF8400,
    0xFF8000, 0xFF7B00, 0xFF7700, 0xFF7300, 0xFF6E00, 0xFF6A00, 0xFF6600, 0xFF6200, 0xFF5E00, 0xFF5900,
    0xFF5500, 0xFF5100, 0xFF4D00, 0xFF4800, 0xFF4400, 0xFF4000, 0xFF3C00, 0xFF3700, 0xFF3300, 0xFF2F00,
    0xFF2A00, 0xFF2600, 0xFF2200, 0xFF1E00, 0xFF1A00, 0xFF1500, 0xFF1100, 0xFF0D00, 0xFF0800, 0xFF0400,
    /* 120° */ 
    0xFF0000, 0xFF0004, 0xFF0008, 0xFF000D, 0xFF0011, 0xFF0015, 0xFF0019, 0xFF001E, 0xFF0022, 0xFF0026,
    0xFF002A, 0xFF002F, 0xFF0033, 0xFF0037, 0xFF003C, 0xFF0040, 0xFF0044, 0xFF0048, 0xFF004D, 0xFF0051,
    0xFF0055, 0xFF0059, 0xFF005E, 0xFF0062, 0xFF0066, 0xFF006A, 0xFF006F, 0xFF0073, 0xFF0077, 0xFF007B,
    0xFF0080, 0xFF0084, 0xFF0088, 0xFF008C, 0xFF0090, 0xFF0095, 0xFF0099, 0xFF009D, 0xFF00A2, 0xFF00A6,
    0xFF00AA, 0xFF00AE, 0xFF00B3, 0xFF00B7, 0xFF00BB, 0xFF00BF, 0xFF00C3, 0xFF00C8, 0xFF00CC, 0xFF00D0,
    0xFF00D4, 0xFF00D9, 0xFF00DD, 0xFF00E1, 0xFF00E5, 0xFF00EA, 0xFF00EE, 0xFF00F2, 0xFF00F7, 0xFF00FB,
    /* 180° */ 
    0xFF00FF, 0xFB00FF, 0xF700FF, 0xF200FF, 0xEE00FF, 0xEA00FF, 0xE500FF, 0xE100FF, 0xDD00FF, 0xD900FF,
    0xD400FF, 0xD000FF, 0xCC00FF, 0xC800FF, 0xC300FF, 0xBF00FF, 0xBB00FF, 0xB700FF, 0xB200FF, 0xAE00FF,
    0xAA00FF, 0xA600FF, 0xA200FF, 0x9D00FF, 0x9900FF, 0x9500FF, 0x9100FF, 0x8C00FF, 0x8800FF, 0x8400FF,
    0x8000FF, 0x7B00FF, 0x7700FF, 0x7300FF, 0x6F00FF, 0x6A00FF, 0x6600FF, 0x6200FF, 0x5E00FF, 0x5900FF,
    0x5500FF, 0x5100FF, 0x4C00FF, 0x4800FF, 0x4400FF, 0x4000FF, 0x3C00FF, 0x3700FF, 0x3300FF, 0x2F00FF,
    0x2B00FF, 0x2600FF, 0x2200FF, 0x1E00FF, 0x1900FF, 0x1500FF, 0x1100FF, 0x0D00FF, 0x0800FF, 0x0400FF,
    /* 240° */ 
    0x0000FF, 0x0004FF, 0x0008FF, 0x000DFF, 0x0011FF, 0x0015FF, 0x0019FF, 0x001EFF, 0x0022FF, 0x0026FF,
    0x002AFF, 0x002FFF, 0x0033FF, 0x0037FF, 0x003CFF, 0x0040FF, 0x0044FF, 0x0048FF, 0x004CFF, 0x0051FF,
    0x0055FF, 0x0059FF, 0x005DFF, 0x0062FF, 0x0066FF, 0x006AFF, 0x006FFF, 0x0073FF, 0x0077FF, 0x007BFF,
    0x0080FF, 0x0084FF, 0x0088FF, 0x008CFF, 0x0090FF, 0x0095FF, 0x0099FF, 0x009DFF, 0x00A2FF, 0x00A6FF,
    0x00AAFF, 0x00AEFF, 0x00B3FF, 0x00B7FF, 0x00BBFF, 0x00BFFF, 0x00C3FF, 0x00C8FF, 0x00CCFF, 0x00D0FF,
    0x00D5FF, 0x00D9FF, 0x00DDFF, 0x00E1FF, 0x00E6FF, 0x00EAFF, 0x00EEFF, 0x00F2FF, 0x00F7FF, 0x00FBFF,
    /* 300° */ 
    0x00FFFF, 0x00FFFB, 0x00FFF7, 0x00FFF2, 0x00FFEE, 0x00FFEA, 0x00FFE6, 0x00FFE1, 0x00FFDD, 0x00FFD9,
    0x00FFD4, 0x00FFD0, 0x00FFCC, 0x00FFC8, 0x00FFC3, 0x00FFBF, 0x00FFBB, 0x00FFB7, 0x00FFB3, 0x00FFAE,
    0x00FFAA, 0x00FFA6, 0x00FFA1, 0x00FF9D, 0x00FF99, 0x00FF95, 0x00FF90, 0x00FF8C, 0x00FF88, 0x00FF84,
    0x00FF80, 0x00FF7B, 0x00FF77, 0x00FF73, 0x00FF6F, 0x00FF6A, 0x00FF66, 0x00FF62, 0x00FF5E, 0x00FF59,
    0x00FF55, 0x00FF51, 0x00FF4D, 0x00FF48, 0x00FF44, 0x00FF40, 0x00FF3C, 0x00FF37, 0x00FF33, 0x00FF2F,
    0x00FF2B, 0x00FF26, 0x00FF22, 0x00FF1E, 0x00FF1A, 0x00FF15, 0x00FF11, 0x00FF0D, 0x00FF08, 0x00FF04,
};

/* ================= 算法辅助函数 ================= */

/**
 * @brief Gamma 2.0 近似校正
 * 人眼对低亮度敏感，线性调节会导致低亮度区域变化过快。
 * 使用平方算法近似 Gamma 2.2，无需查表，效率高。
 */
static inline uint8_t gamma_correct(uint8_t val) {
    // 算法: (val * val + 255) >> 8; 
    // +255 是为了让 1*1 结果不为 0
    return (uint8_t)(((uint16_t)val * val + 255) >> 8);
}

// 快速整数HSV转RGB
static uint32_t hsv2rgb_fast(uint16_t h, uint8_t s, uint8_t v) {
    uint8_t r, g, b;
    uint8_t region, remainder, p, q, t;

    if (s == 0) return (v << 16) | (v << 8) | v;

    region = h / 60;
    remainder = (h - (region * 60)) * 6; 

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return (r << 16) | (g << 8) | b;
}

// 硬件写入封装 (增加去重判断，大幅降低CPU占用)
static void apply_color_hardware(uint32_t c_left, uint32_t c_right) {
    bool need_refresh_l = (c_left != g_led_ctx.last_color_l);
    bool need_refresh_r = (c_right != g_led_ctx.last_color_r);

    if (need_refresh_l && g_led_ctx.strip_left) {
        led_strip_set_pixel(g_led_ctx.strip_left, 0, (c_left >> 16) & 0xFF, (c_left >> 8) & 0xFF, c_left & 0xFF);
        led_strip_refresh(g_led_ctx.strip_left);
        g_led_ctx.last_color_l = c_left;
    }

    if (need_refresh_r && g_led_ctx.strip_right) {
        led_strip_set_pixel(g_led_ctx.strip_right, 0, (c_right >> 16) & 0xFF, (c_right >> 8) & 0xFF, c_right & 0xFF);
        led_strip_refresh(g_led_ctx.strip_right);
        g_led_ctx.last_color_r = c_right;
    }
}

// 辅助初始化
static led_strip_handle_t init_single_strip(int gpio_num) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = 1, // 修正为1，如有多灯请修改
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_RMT_RESOLUTION_HZ,
        .flags.with_dma = false, // 单像素不需要DMA，节省资源
    };
    led_strip_handle_t handle = NULL;
    if (led_strip_new_rmt_device(&strip_config, &rmt_config, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Install strip failed on GPIO %d", gpio_num);
        return NULL;
    }
    return handle;
}

/* ================= 核心逻辑引擎 ================= */

static void process_animation_frame(void) {
    if (!g_led_ctx.is_animating) return;

    g_led_ctx.tick_count++;
    uint32_t c_out_l = 0, c_out_r = 0;
    uint32_t period = g_led_ctx.current_effect.data.period;

    // 安全保护：防止除以0
    if (period < UPDATE_INTERVAL_MS) period = 1000;

    switch (g_led_ctx.current_effect.type) {
        case LED_EFFECT_BREATHING: {
            uint32_t total_ticks = period / UPDATE_INTERVAL_MS;
            uint32_t pos = g_led_ctx.tick_count % total_ticks;
            
            // 使用正弦波太慢，使用三角波 + Gamma校正
            // value 0 ~ 255
            uint32_t value;
            if (pos < (total_ticks / 2)) {
                value = (pos * 255) / (total_ticks / 2);
            } else {
                value = 255 - ((pos - total_ticks/2) * 255) / (total_ticks / 2);
            }

            // 应用Gamma校正，使呼吸更自然
            uint8_t scale = gamma_correct((uint8_t)value);

            uint32_t base = g_led_ctx.current_effect.data.colors[0];
            uint8_t r = (((base >> 16) & 0xFF) * scale) >> 8;
            uint8_t g = (((base >> 8) & 0xFF) * scale) >> 8;
            uint8_t b = ((base & 0xFF) * scale) >> 8;
            
            uint32_t final = (r << 16) | (g << 8) | b;
            c_out_l = final;
            c_out_r = g_led_ctx.current_effect.data.symmetric ? final : 0;
            break;
        }

        case LED_EFFECT_RAINBOW: {
            uint16_t hue = (g_led_ctx.tick_count * 360 * UPDATE_INTERVAL_MS / period) % 360;
            c_out_l = rainbow_table[hue]; // 直接取值
            
            if (g_led_ctx.current_effect.data.symmetric) {
                c_out_r = c_out_l;
            } else {
                c_out_r = rainbow_table[(hue + 180) % 360];
            }
            break;
        }

        case LED_EFFECT_BLINK: {
            uint32_t ticks = period / UPDATE_INTERVAL_MS;
            bool on = (g_led_ctx.tick_count % ticks) < (ticks / 2);
            
            uint32_t color = on ? g_led_ctx.current_effect.data.colors[0] : 0;
            c_out_l = color;
            c_out_r = g_led_ctx.current_effect.data.symmetric ? color : 0;
            break;
        }

        default:
            g_led_ctx.is_animating = false;
            return;
    }

    apply_color_hardware(c_out_l, c_out_r);
}

/* ================= 任务主体 ================= */

static void led_engine_task(void *arg) {
    led_cmd_t cmd_buf;
    TickType_t wait_time = portMAX_DELAY; 

    ESP_LOGI(TAG, "LED Task Running");

    while (1) {
        // 等待队列消息。
        // 如果正在动画，wait_time = UPDATE_INTERVAL_MS，超时即视为下一帧信号
        // 如果静止，wait_time = portMAX_DELAY，彻底挂起任务，0% CPU占用
        if (xQueueReceive(g_led_ctx.cmd_queue, &cmd_buf, wait_time) == pdTRUE) {
            
            // --- 收到新指令 ---
            g_led_ctx.current_effect = cmd_buf;
            g_led_ctx.tick_count = 0;

            ESP_LOGD(TAG, "Cmd: %d", cmd_buf.type);

            switch (cmd_buf.type) {
                case LED_EFFECT_OFF:
                    g_led_ctx.is_animating = false;
                    apply_color_hardware(0, 0);
                    wait_time = portMAX_DELAY; 
                    break;

                case LED_EFFECT_RGB:
                    g_led_ctx.is_animating = false;
                    apply_color_hardware(cmd_buf.data.colors[0], cmd_buf.data.colors[1]);
                    wait_time = portMAX_DELAY;
                    break;

                default: // BREATHING, RAINBOW, BLINK
                    g_led_ctx.is_animating = true;
                    // 立即处理第一帧，避免等待
                    process_animation_frame(); 
                    wait_time = pdMS_TO_TICKS(UPDATE_INTERVAL_MS);
                    break;
            }
        } else {
            // --- 超时 (即需要刷新动画帧) ---
            if (g_led_ctx.is_animating) {
                process_animation_frame();
            } else {
                // 异常保护：不应进入此分支，如果进入则强制休眠
                wait_time = portMAX_DELAY;
            }
        }
    }
}

/* ================= 外部接口实现 ================= */

static void send_cmd_internal(led_cmd_t *cmd) {
    if (g_led_ctx.cmd_queue == NULL) {
        ESP_LOGW(TAG, "LED not init");
        return;
    }
    // 使用 xQueueSendToBack (发送值的拷贝)
    // 0延时：如果队列满了，直接丢弃旧的，保证UI响应最新指令
    if (xQueueSend(g_led_ctx.cmd_queue, cmd, 0) != pdTRUE) {
        ESP_LOGD(TAG, "Q Full, drop");
    }
}

esp_err_t app_led_init(void) {
    if (g_led_ctx.cmd_queue != NULL) return ESP_OK; 

    // 1. 硬件初始化
    g_led_ctx.strip_left = init_single_strip(LED_GPIO_LEFT);
    g_led_ctx.strip_right = init_single_strip(LED_GPIO_RIGHT);

    apply_color_hardware(0x0000FF, 0xFF0000);
    
    // 初始化状态，强制刷新一次全黑
    led_strip_clear(g_led_ctx.strip_left);
    led_strip_clear(g_led_ctx.strip_right);
    g_led_ctx.last_color_l = 0;
    g_led_ctx.last_color_r = 0;

    // 2. 创建队列
    g_led_ctx.cmd_queue = xQueueCreate(LED_QUEUE_SIZE, sizeof(led_cmd_t));
    if (g_led_ctx.cmd_queue == NULL) return ESP_FAIL;

    // 3. 创建任务
    BaseType_t ret = xTaskCreatePinnedToCore(
        led_engine_task, 
        "led_task", 
        LED_TASK_STACK_SIZE, 
        NULL, 
        LED_TASK_PRIORITY, 
        &g_led_ctx.task_handle, 
        1
    );

    if (ret != pdPASS) return ESP_FAIL;

    // 默认开机演示 (可选)
    // app_led_set_rainbow(3000, true);

    app_led_set_rgb(0x0000FF, 0xFF0000);

   
    
    return ESP_OK;
}

void app_led_deinit(void) {
    if (g_led_ctx.task_handle) {
        vTaskDelete(g_led_ctx.task_handle);
        g_led_ctx.task_handle = NULL;
    }
    if (g_led_ctx.cmd_queue) {
        vQueueDelete(g_led_ctx.cmd_queue);
        g_led_ctx.cmd_queue = NULL;
    }
    if (g_led_ctx.strip_left) {
        led_strip_del(g_led_ctx.strip_left);
        g_led_ctx.strip_left = NULL;
    }
    if (g_led_ctx.strip_right) {
        led_strip_del(g_led_ctx.strip_right);
        g_led_ctx.strip_right = NULL;
    }
}

void app_led_set_rgb(uint32_t left_color, uint32_t right_color) {
    led_cmd_t cmd = {
        .type = LED_EFFECT_RGB,
        .data = { .colors = {left_color, right_color} } // 修正结构体初始化语法
    };
    send_cmd_internal(&cmd);
}

void app_led_set_breathing(uint32_t color, uint16_t period_ms, bool symmetric) {
    led_cmd_t cmd = {
        .type = LED_EFFECT_BREATHING,
        .data = { 
            .colors = {color, 0}, 
            .period = period_ms, 
            .symmetric = symmetric 
        }
    };
    send_cmd_internal(&cmd);
}

void app_led_set_rainbow(uint16_t period_ms, bool symmetric) {
    led_cmd_t cmd = {
        .type = LED_EFFECT_RAINBOW,
        .data = { .period = period_ms, .symmetric = symmetric }
    };
    send_cmd_internal(&cmd);
}

void app_led_set_blink(uint32_t color, uint16_t period_ms, bool symmetric) {
    led_cmd_t cmd = {
        .type = LED_EFFECT_BLINK,
        .data = { 
            .colors = {color, 0}, 
            .period = period_ms, 
            .symmetric = symmetric 
        }
    };
    send_cmd_internal(&cmd);
}

void app_led_off(void) {
    led_cmd_t cmd = { .type = LED_EFFECT_OFF };
    send_cmd_internal(&cmd);
}

void app_led_random_test(void) {
    uint32_t r_color = hsv2rgb_fast(esp_random() % 360, 255, 255);
    int effect = esp_random() % 4; // 0-3

    switch (effect) {
        case 0: app_led_set_rgb(r_color, ~r_color & 0xFFFFFF); break; // 互补色
        case 1: app_led_set_breathing(r_color, 2000, true); break;
        case 2: app_led_set_rainbow(1500, true); break;
        case 3: app_led_set_blink(r_color, 300, true); break;
    }
}