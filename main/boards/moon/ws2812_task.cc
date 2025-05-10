#include "led_strip.h"
#include "config.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <settings.h>
#include <esp_random.h>
#include <cmath>  // Add this for M_PI and sin function

// Define M_PI if it's not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char* TAG = "WS2812Task";

// 定义LED灯带工作模式
typedef enum {
    WS2812_MODE_BLINK_BLUE = 0,  // 蓝色闪烁
    WS2812_MODE_BLINK_RED,       // 红色闪烁
    WS2812_MODE_BLINK_GREEN,     // 绿色闪烁
    WS2812_MODE_RAINBOW,        // 彩虹模式
    WS2812_MODE_CHASE,        // 跑马灯模式
    WS2812_MODE_SOLID,        // 全亮模式
    WS2812_MODE_DIM,          // 微光模式
    WS2812_MODE_OFF,          // 关闭模式
    WS2812_MODE_BREATHING,    // 呼吸灯模式
    WS2812_MODE_FIRE,         // 火焰效果
    WS2812_MODE_TWINKLE,      // 闪烁效果
    WS2812_MODE_WAVE,         // 波浪效果
    WS2812_MODE_ALTERNATE,    // 交替闪烁
    WS2812_MODE_PULSE,        // 脉冲效果
    WS2812_MODE_COMET,        // 彗星效果
    WS2812_MODE_RAINBOW_CYCLE, // 彩虹循环
    WS2812_MODE_RAINBOW_CHASE, // 彩虹追逐
    WS2812_MODE_RAINBOW_WAVE,  // 彩虹波浪
    WS2812_MODE_RAINBOW_FIRE,  // 彩虹火焰
    WS2812_MODE_RAINBOW_TWINKLE, // 彩虹闪烁
    WS2812_MODE_RAINBOW_BREATHING, // 彩虹呼吸
    WS2812_MODE_RAINBOW_PULSE, // 彩虹脉冲
    WS2812_MODE_RAINBOW_ALTERNATE, // 彩虹交替
    WS2812_MODE_RAINBOW_COMET, // 彩虹彗星
    WS2812_MODE_COLOR_WIPE,   // 颜色擦除
    WS2812_MODE_COLOR_CHASE,  // 颜色追逐
    WS2812_MODE_COLOR_WAVE,   // 颜色波浪
    WS2812_MODE_COLOR_FIRE,   // 颜色火焰
    WS2812_MODE_COLOR_TWINKLE, // 颜色闪烁
    WS2812_MODE_COLOR_BREATHING, // 颜色呼吸
    WS2812_MODE_COLOR_PULSE,  // 颜色脉冲
    WS2812_MODE_COLOR_ALTERNATE, // 颜色交替
    WS2812_MODE_COLOR_COMET,  // 颜色彗星
    WS2812_MODE_MAX           // 模式数量
} ws2812_mode_t;

typedef enum {
    WS2812_STATE_BOOTING = 0,
    WS2812_STATE_LISTENING_NO_VOICE,
    WS2812_STATE_LISTENING_VOICE,
    WS2812_STATE_SPEAKING,
    WS2812_STATE_IDLE,
    WS2812_STATE_MAX
} ws2812_state_t;

//律动模式编号,目前定义了4种律动模式，每种律动模式可以从上面的灯带模式中选择
#define WAVE_MODE_MAX 4
static int current_wave_mode = 0;

//当前工作状态


static ws2812_state_t current_state = WS2812_STATE_BOOTING; 

// 当前工作模式
static ws2812_mode_t current_mode = WS2812_MODE_RAINBOW;
static led_strip_handle_t g_led_strip = NULL;
// 全局亮度参数 (0-100)
volatile static uint32_t global_brightness = 100;
static bool is_on = true;


 
// 根据律动模式和当前工作状态决定灯带模式
static void update_led_mode_by_state() {
    if (current_state == 0 || current_state >= WS2812_STATE_MAX) {
        return;
    }
    
    switch (current_state) {           
        case WS2812_STATE_LISTENING_VOICE:
            // 监听且有声状态，根据律动模式选择
            switch (current_wave_mode) {
                case 0:
                    current_mode = WS2812_MODE_CHASE;
                    break;
                case 1:
                    current_mode = WS2812_MODE_WAVE;
                    break;
                case 2:
                    current_mode = WS2812_MODE_TWINKLE;
                    break;
                case 3:
                    current_mode = WS2812_MODE_COMET;
                    break;
                default:
                    current_mode = WS2812_MODE_RAINBOW;
                    break;
            }
            break;
            
        case WS2812_STATE_LISTENING_NO_VOICE:
            // 监听且无声状态， 
            current_mode = WS2812_MODE_DIM;
            break;
            
        case WS2812_STATE_SPEAKING:
            // 说话状态，根据律动模式选择
            switch (current_wave_mode) {
                case 0:
                    current_mode = WS2812_MODE_COLOR_CHASE;
                    break;
                case 1:
                    current_mode = WS2812_MODE_COLOR_WAVE;
                    break;
                case 2:
                    current_mode = WS2812_MODE_COLOR_TWINKLE;
                    break;
                case 3:
                    current_mode = WS2812_MODE_COLOR_COMET;
                    break;
                default:
                    current_mode = WS2812_MODE_RAINBOW;
                    break;
            }
            break;

        case WS2812_STATE_IDLE:
            // 空闲状态使用呼吸灯模式
            current_mode = WS2812_MODE_BREATHING;
            break;
            
        default:
            // 默认使用呼吸灯模式
            current_mode = WS2812_MODE_BREATHING;
            break;
    }
    
    ESP_LOGD(TAG, "更新LED模式: 状态=%d, 律动模式=%d, 当前模式=%d", 
             current_state, current_wave_mode, current_mode);
}

// 打开LED灯带
extern "C" void ws2812_turn_on(void) {
    is_on = true;
    ESP_LOGI(TAG, "打开WS2812灯带");
}

// 关闭LED灯带
extern "C" void ws2812_turn_off(void) {
    is_on = false;
    ESP_LOGI(TAG, "关闭WS2812灯带");
}
 

extern "C" void ws2812_set_mode(ws2812_mode_t mode) {
    if (mode < WS2812_MODE_MAX) {
        current_mode = mode;
        ESP_LOGI(TAG, "设置WS2812工作模式: %d", mode);
    } else {
        ESP_LOGE(TAG, "无效的WS2812工作模式: %d", mode);
    }
}



// 获取当前LED灯带工作模式
extern "C" int ws2812_get_wave_mode(void) {
    return current_wave_mode;
}

// 设置LED工作状态
extern "C" void ws2812_set_state(ws2812_state_t state) {
    if (state < WS2812_STATE_MAX) {
        current_state = state;
        update_led_mode_by_state();
        ESP_LOGI(TAG, "设置WS2812工作状态: %d", state);
    } else {
        ESP_LOGE(TAG, "无效的WS2812工作状态: %d", state);
    }
}




// 设置全局亮度参数 (0-100)
extern "C" void ws2812_set_brightness(uint8_t brightness) {
    if (brightness <= 100) {
        global_brightness = brightness;
        Settings settings("led_strip", true);
        settings.SetInt("brightness", brightness);
        ESP_LOGI(TAG, "设置WS2812亮度: %d", brightness);
    } else {
        ESP_LOGE(TAG, "无效的亮度值: %d, 有效范围为0-100", brightness);
    }
}

// 设置LED灯带律动模式
extern "C" void ws2812_set_wave_mode(int mode) {
    if (mode < WAVE_MODE_MAX) {
        current_wave_mode = mode;
        update_led_mode_by_state();
        Settings settings("led_strip", true);
        settings.SetInt("effect_mode", mode);
        ESP_LOGI(TAG, "设置WS2812律动模式: %d", mode);
    } else {
        ESP_LOGE(TAG, "无效的WS2812律动模式: %d", mode);
    }
}

// 获取当前全局亮度参数
extern "C" uint8_t ws2812_get_brightness(void) {
    return global_brightness;
}

// 闪烁动画, 与律动效果无关，不能关闭和设置亮度
static void run_blink_blue_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 100;
    for (int i = 0; i < 12; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int i = 0; i < 12; i++) {
        led_strip_set_pixel(led_strip, i, 0, 0, 0);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
}


static void run_blink_red_animation(led_strip_handle_t led_strip) {
    uint8_t r = 100, g = 0, b = 0;
    for (int i = 0; i < 12; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int i = 0; i < 12; i++) {
        led_strip_set_pixel(led_strip, i, 0, 0, 0);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void run_blink_green_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 100, b = 0;
    for (int i = 0; i < 12; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int i = 0; i < 12; i++) {
        led_strip_set_pixel(led_strip, i, 0, 0, 0);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
}


// 应用亮度参数到RGB颜色
static void apply_brightness(uint8_t &r, uint8_t &g, uint8_t &b) {

    if (is_on && global_brightness <= 100) {
        r = (global_brightness * r) / 100;
        g = (global_brightness * g) / 100;
        b = (global_brightness * b) / 100;
    }else{
        r = 0;
        g = 0;
        b = 0;
    }
}

// 彩虹动画
static void run_rainbow_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int j = 0; j < 255 && current_mode == WS2812_MODE_RAINBOW; j++) {
        // 清除所有像素
        led_strip_clear(led_strip);
        
        // 设置彩虹颜色
        for (int i = 0; i < 12; i++) {
            // 计算每个LED的颜色
            uint32_t color = j + i * 32;
            
            // 简单的HSV到RGB转换
            if (color < 85) {
                r = color * 3;
                g = 255 - color * 3;
                b = 0;
            } else if (color < 170) {
                color -= 85;
                r = 255 - color * 3;
                g = 0;
                b = color * 3;
            } else {
                color -= 170;
                r = 0;
                g = color * 3;
                b = 255 - color * 3;
            }
            
            // 应用亮度参数
            apply_brightness(r, g, b);
            
            // 设置LED颜色
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
        
        // 更新LED灯带
        led_strip_refresh(led_strip);
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 跑马灯动画
static void run_chase_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int j = 0; j < 3 && current_mode == WS2812_MODE_CHASE; j++) {
        switch (j) {
            case 0: r = 255; g = 0; b = 0; break;  // 红色
            case 1: r = 0; g = 255; b = 0; break;  // 绿色
            case 2: r = 0; g = 0; b = 255; break;  // 蓝色
        }
        
        for (int i = 0; i < 12 * 3 && current_mode == WS2812_MODE_CHASE; i++) {
            led_strip_clear(led_strip);
            
            // 应用亮度参数
            uint8_t r_adjusted = r, g_adjusted = g, b_adjusted = b;
            apply_brightness(r_adjusted, g_adjusted, b_adjusted);
            
            led_strip_set_pixel(led_strip, i % 12, r_adjusted, g_adjusted, b_adjusted);
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// 全亮动画
static void run_solid_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int j = 0; j < 3 && current_mode == WS2812_MODE_SOLID; j++) {
        switch (j) {
            case 0: r = 255; g = 0; b = 0; break;  // 红色
            case 1: r = 0; g = 255; b = 0; break;  // 绿色
            case 2: r = 0; g = 0; b = 255; break;  // 蓝色
        }
        
        // 应用亮度参数
        uint8_t r_adjusted = r, g_adjusted = g, b_adjusted = b;
        apply_brightness(r_adjusted, g_adjusted, b_adjusted);
        
        // 设置所有LED为相同颜色
        for (int i = 0; i < 12; i++) {
            led_strip_set_pixel(led_strip, i, r_adjusted, g_adjusted, b_adjusted);
        }
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}



// 微光模式
static void run_dim_animation(led_strip_handle_t led_strip) {
    uint8_t r = 5, g = 5, b = 5;
    // 微光模式就不再调节亮度了
    // 设置所有LED为相同颜色
    for (int i = 0; i < 12; i++) {
        if(is_on){  
            led_strip_set_pixel(led_strip, i, r, g, b);      
        }else{
            led_strip_set_pixel(led_strip, i, 0, 0, 0);
        }
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(50));
}

// 关闭所有LED
static void turn_off_leds(led_strip_handle_t led_strip) {
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(100));
}

// 呼吸灯动画
static void run_breathing_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    // 呼吸灯颜色
    r = 0;
    g = 100;
    b = 255;
    
    for (int i = 0; i < 100 && current_mode == WS2812_MODE_BREATHING; i++) {
        // 计算亮度 (0-100)
        int brightness = (i < 50) ? i * 2 : (100 - i) * 2;
        
        // 应用亮度参数
        uint8_t r_adjusted = (r * brightness) / 100;
        uint8_t g_adjusted = (g * brightness) / 100;
        uint8_t b_adjusted = (b * brightness) / 100;
        
        apply_brightness(r_adjusted, g_adjusted, b_adjusted);
        // 设置所有LED为相同颜色
        for (int j = 0; j < 12; j++) {
            led_strip_set_pixel(led_strip, j, r_adjusted, g_adjusted, b_adjusted);
        }
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 火焰效果动画
static void run_fire_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 100 && current_mode == WS2812_MODE_FIRE; i++) {
        // 清除所有像素
        led_strip_clear(led_strip);
        
        // 设置火焰颜色
        for (int j = 0; j < 12; j++) {
            // 随机生成火焰效果
            int random_value = esp_random() % 100;
            
            if (random_value < 30) {
                // 火焰核心 - 亮黄色
                r = 255;
                g = 255;
                b = 0;
            } else if (random_value < 60) {
                // 火焰中部 - 橙色
                r = 255;
                g = 100;
                b = 0;
            } else if (random_value < 80) {
                // 火焰外部 - 红色
                r = 255;
                g = 0;
                b = 0;
            } else {
                // 火焰边缘 - 暗红色
                r = 100;
                g = 0;
                b = 0;
            }
            
            // 应用亮度参数
            apply_brightness(r, g, b);
            
            // 设置LED颜色
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        // 更新LED灯带
        led_strip_refresh(led_strip);
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 闪烁效果动画
static void run_twinkle_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 100 && current_mode == WS2812_MODE_TWINKLE; i++) {
        // 清除所有像素
        led_strip_clear(led_strip);
        
        // 设置闪烁颜色
        for (int j = 0; j < 12; j++) {
            // 随机生成闪烁效果
            int random_value = esp_random() % 100;
            
            if (random_value < 20) {
                // 闪烁 - 白色
                r = 255;
                g = 255;
                b = 255;
            } else {
                // 不闪烁 - 关闭
                r = 0;
                g = 0;
                b = 0;
            }
            
            // 应用亮度参数
            apply_brightness(r, g, b);
            
            // 设置LED颜色
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        // 更新LED灯带
        led_strip_refresh(led_strip);
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 波浪效果动画
static void run_wave_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 100 && current_mode == WS2812_MODE_WAVE; i++) {
        // 清除所有像素
        led_strip_clear(led_strip);
        
        // 设置波浪颜色
        for (int j = 0; j < 12; j++) {
            // 计算波浪位置
            int position = (j + i) % 12;
            int brightness = (position < 6) ? (position * 42) : ((12 - position) * 42);
            
            // 波浪颜色 - 蓝色
            r = 0;
            g = 0;
            b = brightness;
            
            // 应用亮度参数
            apply_brightness(r, g, b);
            
            // 设置LED颜色
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        // 更新LED灯带
        led_strip_refresh(led_strip);
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 交替闪烁动画
static void run_alternate_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 10 && current_mode == WS2812_MODE_ALTERNATE; i++) {
        // 清除所有像素
        led_strip_clear(led_strip);
        
        // 设置交替颜色
        for (int j = 0; j < 12; j++) {
            if (j % 2 == 0) {
                // 偶数LED - 红色
                r = 255;
                g = 0;
                b = 0;
            } else {
                // 奇数LED - 绿色
                r = 0;
                g = 255;
                b = 0;
            }
            
            // 应用亮度参数
            apply_brightness(r, g, b);
            
            // 设置LED颜色
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        // 更新LED灯带
        led_strip_refresh(led_strip);
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // 清除所有像素
        led_strip_clear(led_strip);
        
        // 设置交替颜色（反转）
        for (int j = 0; j < 12; j++) {
            if (j % 2 == 0) {
                // 偶数LED - 绿色
                r = 0;
                g = 255;
                b = 0;
            } else {
                // 奇数LED - 红色
                r = 255;
                g = 0;
                b = 0;
            }
            
            // 应用亮度参数
            apply_brightness(r, g, b);
            
            // 设置LED颜色
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        // 更新LED灯带
        led_strip_refresh(led_strip);
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// 脉冲效果动画
static void run_pulse_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 100 && current_mode == WS2812_MODE_PULSE; i++) {
        // 清除所有像素
        led_strip_clear(led_strip);
        
        // 计算脉冲位置
        int pulse_position = (i / 10) % 12;
        
        // 设置脉冲颜色
        for (int j = 0; j < 12; j++) {
            // 计算与脉冲位置的距离
            int distance = abs(j - pulse_position);
            
            if (distance == 0) {
                // 脉冲中心 - 白色
                r = 255;
                g = 255;
                b = 255;
            } else if (distance == 1) {
                // 脉冲边缘 - 淡白色
                r = 100;
                g = 100;
                b = 100;
            } else {
                // 其他位置 - 关闭
                r = 0;
                g = 0;
                b = 0;
            }
            
            // 应用亮度参数
            apply_brightness(r, g, b);
            
            // 设置LED颜色
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        // 更新LED灯带
        led_strip_refresh(led_strip);
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彗星效果动画
static void run_comet_animation(led_strip_handle_t led_strip) {
    uint8_t r = 255, g = 255, b = 255;
    uint8_t tail_length = 4;
    
    for (int i = 0; i < 24 && current_mode == WS2812_MODE_COMET; i++) {
        led_strip_clear(led_strip);
        
        int head = i % 12;
        for (int j = 0; j < tail_length; j++) {
            int pos = (head - j + 12) % 12;
            uint8_t brightness = 255 * (tail_length - j) / tail_length;
            uint8_t r_adj = r * brightness / 255;
            uint8_t g_adj = g * brightness / 255;
            uint8_t b_adj = b * brightness / 255;
            
            apply_brightness(r_adj, g_adj, b_adj);
            led_strip_set_pixel(led_strip, pos, r_adj, g_adj, b_adj);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹循环动画
static void run_rainbow_cycle_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_CYCLE; i++) {
        for (int j = 0; j < 12; j++) {
            uint32_t color = (i * 20 + j * 21) % 255;
            
            if (color < 85) {
                r = color * 3;
                g = 255 - color * 3;
                b = 0;
            } else if (color < 170) {
                color -= 85;
                r = 255 - color * 3;
                g = 0;
                b = color * 3;
            } else {
                color -= 170;
                r = 0;
                g = color * 3;
                b = 255 - color * 3;
            }
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹追逐动画
static void run_rainbow_chase_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 36 && current_mode == WS2812_MODE_RAINBOW_CHASE; i++) {
        led_strip_clear(led_strip);
        
        for (int j = 0; j < 3; j++) {
            int pos = (i + j * 4) % 12;
            uint32_t color = (i * 20 + j * 85) % 255;
            
            if (color < 85) {
                r = color * 3;
                g = 255 - color * 3;
                b = 0;
            } else if (color < 170) {
                color -= 85;
                r = 255 - color * 3;
                g = 0;
                b = color * 3;
            } else {
                color -= 170;
                r = 0;
                g = color * 3;
                b = 255 - color * 3;
            }
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, pos, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹波浪动画
static void run_rainbow_wave_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_WAVE; i++) {
        for (int j = 0; j < 12; j++) {
            int brightness = (int)(127.5 * (1 + sin(2 * M_PI * (i + j) / 12.0)));
            uint32_t color = (i * 20 + j * 21) % 255;
            
            if (color < 85) {
                r = color * 3;
                g = 255 - color * 3;
                b = 0;
            } else if (color < 170) {
                color -= 85;
                r = 255 - color * 3;
                g = 0;
                b = color * 3;
            } else {
                color -= 170;
                r = 0;
                g = color * 3;
                b = 255 - color * 3;
            }
            
            r = r * brightness / 255;
            g = g * brightness / 255;
            b = b * brightness / 255;
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹火焰动画
static void run_rainbow_fire_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_FIRE; i++) {
        for (int j = 0; j < 12; j++) {
            int random_value = esp_random() % 100;
            uint32_t color = (i * 20 + j * 21) % 255;
            
            if (random_value < 30) {
                // 火焰核心
                if (color < 85) {
                    r = color * 3;
                    g = 255 - color * 3;
                    b = 0;
                } else if (color < 170) {
                    color -= 85;
                    r = 255 - color * 3;
                    g = 0;
                    b = color * 3;
                } else {
                    color -= 170;
                    r = 0;
                    g = color * 3;
                    b = 255 - color * 3;
                }
            } else {
                // 火焰边缘
                r = r / 2;
                g = g / 2;
                b = b / 2;
            }
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹闪烁动画
static void run_rainbow_twinkle_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_TWINKLE; i++) {
        for (int j = 0; j < 12; j++) {
            if (esp_random() % 100 < 30) {
                uint32_t color = (i * 20 + j * 21) % 255;
                
                if (color < 85) {
                    r = color * 3;
                    g = 255 - color * 3;
                    b = 0;
                } else if (color < 170) {
                    color -= 85;
                    r = 255 - color * 3;
                    g = 0;
                    b = color * 3;
                } else {
                    color -= 170;
                    r = 0;
                    g = color * 3;
                    b = 255 - color * 3;
                }
            } else {
                r = 0;
                g = 0;
                b = 0;
            }
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹呼吸动画
static void run_rainbow_breathing_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_BREATHING; i++) {
        int brightness = (i < 25) ? i * 4 : (100 - i * 4);
        
        for (int j = 0; j < 12; j++) {
            uint32_t color = (i * 20 + j * 21) % 255;
            
            if (color < 85) {
                r = color * 3;
                g = 255 - color * 3;
                b = 0;
            } else if (color < 170) {
                color -= 85;
                r = 255 - color * 3;
                g = 0;
                b = color * 3;
            } else {
                color -= 170;
                r = 0;
                g = color * 3;
                b = 255 - color * 3;
            }
            
            r = r * brightness / 100;
            g = g * brightness / 100;
            b = b * brightness / 100;
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹脉冲动画
static void run_rainbow_pulse_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_PULSE; i++) {
        led_strip_clear(led_strip);
        
        int pulse_pos = i % 12;
        for (int j = 0; j < 12; j++) {
            int distance = abs(j - pulse_pos);
            if (distance <= 2) {
                uint32_t color = (i * 20 + j * 21) % 255;
                int brightness = (2 - distance) * 127;
                
                if (color < 85) {
                    r = color * 3;
                    g = 255 - color * 3;
                    b = 0;
                } else if (color < 170) {
                    color -= 85;
                    r = 255 - color * 3;
                    g = 0;
                    b = color * 3;
                } else {
                    color -= 170;
                    r = 0;
                    g = color * 3;
                    b = 255 - color * 3;
                }
                
                r = r * brightness / 255;
                g = g * brightness / 255;
                b = b * brightness / 255;
            } else {
                r = 0;
                g = 0;
                b = 0;
            }
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹交替动画
static void run_rainbow_alternate_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_ALTERNATE; i++) {
        for (int j = 0; j < 12; j++) {
            uint32_t color = ((i + j) * 20) % 255;
            
            if (j % 2 == (i / 10) % 2) {
                if (color < 85) {
                    r = color * 3;
                    g = 255 - color * 3;
                    b = 0;
                } else if (color < 170) {
                    color -= 85;
                    r = 255 - color * 3;
                    g = 0;
                    b = color * 3;
                } else {
                    color -= 170;
                    r = 0;
                    g = color * 3;
                    b = 255 - color * 3;
                }
            } else {
                r = 0;
                g = 0;
                b = 0;
            }
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 彩虹彗星动画
static void run_rainbow_comet_animation(led_strip_handle_t led_strip) {
    uint8_t r = 0, g = 0, b = 0;
    uint8_t tail_length = 4;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_RAINBOW_COMET; i++) {
        led_strip_clear(led_strip);
        
        int head = i % 12;
        uint32_t base_color = i * 20;
        
        for (int j = 0; j < tail_length; j++) {
            int pos = (head - j + 12) % 12;
            uint32_t color = (base_color + j * 20) % 255;
            uint8_t brightness = 255 * (tail_length - j) / tail_length;
            
            if (color < 85) {
                r = color * 3;
                g = 255 - color * 3;
                b = 0;
            } else if (color < 170) {
                color -= 85;
                r = 255 - color * 3;
                g = 0;
                b = color * 3;
            } else {
                color -= 170;
                r = 0;
                g = color * 3;
                b = 255 - color * 3;
            }
            
            r = r * brightness / 255;
            g = g * brightness / 255;
            b = b * brightness / 255;
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, pos, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 颜色擦除动画
static void run_color_wipe_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255}   // 紫色
    };
    const int num_colors = 6;
    
    for (int i = 0; i < num_colors && current_mode == WS2812_MODE_COLOR_WIPE; i++) {
        for (int j = 0; j < 12; j++) {
            uint8_t r = colors[i][0];
            uint8_t g = colors[i][1];
            uint8_t b = colors[i][2];
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

// 颜色追逐动画
static void run_color_chase_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255}   // 紫色
    };
    const int num_colors = 6;
    
    for (int i = 0; i < 36 && current_mode == WS2812_MODE_COLOR_CHASE; i++) {
        led_strip_clear(led_strip);
        
        for (int j = 0; j < 3; j++) {
            int pos = (i + j * 4) % 12;
            int color_idx = (i / 6 + j) % num_colors;
            
            uint8_t r = colors[color_idx][0];
            uint8_t g = colors[color_idx][1];
            uint8_t b = colors[color_idx][2];
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, pos, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 颜色波浪动画
static void run_color_wave_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255}   // 紫色
    };
    const int num_colors = 6;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_COLOR_WAVE; i++) {
        for (int j = 0; j < 12; j++) {
            int color_idx = (i / 8 + j / 2) % num_colors;
            int brightness = (int)(127.5 * (1 + sin(2 * M_PI * (i + j) / 12.0)));
            
            uint8_t r = colors[color_idx][0] * brightness / 255;
            uint8_t g = colors[color_idx][1] * brightness / 255;
            uint8_t b = colors[color_idx][2] * brightness / 255;
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 颜色火焰动画
static void run_color_fire_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {255, 128, 0},  // 橙色
        {255, 255, 0},  // 黄色
        {255, 0, 255},  // 紫色
        {0, 0, 255},    // 蓝色
        {0, 255, 0}     // 绿色
    };
    const int num_colors = 6;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_COLOR_FIRE; i++) {
        int base_color = (i / 8) % num_colors;
        
        for (int j = 0; j < 12; j++) {
            int random_value = esp_random() % 100;
            uint8_t r, g, b;
            
            if (random_value < 30) {
                // 火焰核心 - 主色
                r = colors[base_color][0];
                g = colors[base_color][1];
                b = colors[base_color][2];
            } else if (random_value < 60) {
                // 火焰中部 - 减弱的主色
                r = colors[base_color][0] * 2 / 3;
                g = colors[base_color][1] * 2 / 3;
                b = colors[base_color][2] * 2 / 3;
            } else {
                // 火焰边缘 - 更弱的主色
                r = colors[base_color][0] / 3;
                g = colors[base_color][1] / 3;
                b = colors[base_color][2] / 3;
            }
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 颜色呼吸动画
static void run_color_breathing_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255}   // 紫色
    };
    const int num_colors = 6;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_COLOR_BREATHING; i++) {
        int color_idx = (i / 8) % num_colors;
        int brightness = (i < 25) ? i * 4 : (100 - i * 4);
        
        uint8_t r = colors[color_idx][0] * brightness / 100;
        uint8_t g = colors[color_idx][1] * brightness / 100;
        uint8_t b = colors[color_idx][2] * brightness / 100;
        
        for (int j = 0; j < 12; j++) {
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 颜色脉冲动画
static void run_color_pulse_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255}   // 紫色
    };
    const int num_colors = 6;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_COLOR_PULSE; i++) {
        led_strip_clear(led_strip);
        
        int color_idx = (i / 8) % num_colors;
        int pulse_pos = i % 12;
        
        for (int j = 0; j < 12; j++) {
            int distance = abs(j - pulse_pos);
            if (distance <= 2) {
                int brightness = (2 - distance) * 127;
                
                uint8_t r = colors[color_idx][0] * brightness / 255;
                uint8_t g = colors[color_idx][1] * brightness / 255;
                uint8_t b = colors[color_idx][2] * brightness / 255;
                
                apply_brightness(r, g, b);
                led_strip_set_pixel(led_strip, j, r, g, b);
            }
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 颜色交替动画
static void run_color_alternate_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255}   // 紫色
    };
    const int num_colors = 6;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_COLOR_ALTERNATE; i++) {
        int color_idx1 = (i / 8) % num_colors;
        int color_idx2 = (color_idx1 + 1) % num_colors;
        
        for (int j = 0; j < 12; j++) {
            int current_color = (j % 2 == (i / 4) % 2) ? color_idx1 : color_idx2;
            
            uint8_t r = colors[current_color][0];
            uint8_t g = colors[current_color][1];
            uint8_t b = colors[current_color][2];
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, j, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 颜色彗星动画
static void run_color_comet_animation(led_strip_handle_t led_strip) {
    const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255}   // 紫色
    };
    const int num_colors = 6;
    uint8_t tail_length = 4;
    
    for (int i = 0; i < 50 && current_mode == WS2812_MODE_COLOR_COMET; i++) {
        led_strip_clear(led_strip);
        
        int color_idx = (i / 8) % num_colors;
        int head = i % 12;
        
        for (int j = 0; j < tail_length; j++) {
            int pos = (head - j + 12) % 12;
            uint8_t brightness = 255 * (tail_length - j) / tail_length;
            
            uint8_t r = colors[color_idx][0] * brightness / 255;
            uint8_t g = colors[color_idx][1] * brightness / 255;
            uint8_t b = colors[color_idx][2] * brightness / 255;
            
            apply_brightness(r, g, b);
            led_strip_set_pixel(led_strip, pos, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ws2812任务
extern "C" void ws2812_task(void *pvParameters) {
    // 创建LED灯带控制器
    led_strip_config_t strip_config = {
        .strip_gpio_num = BUILTIN_LED_GPIO,
        .max_leds = 12, // 使用12个LED灯
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false
        }
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags = {
            .with_dma = false
        }
    };
    
    led_strip_handle_t led_strip = NULL;
    
    // 初始化
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812初始化失败: %d", ret);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "WS2812初始化成功");
    g_led_strip = led_strip; // 保存全局句柄

 

    Settings settings("led_strip");
    global_brightness = settings.GetInt("brightness", 100);
    ESP_LOGI(TAG, "WS2812亮度: %ld", global_brightness);
    current_wave_mode = settings.GetInt("effect_mode", 0);
    ESP_LOGI(TAG, "WS2812律动模式: %d", current_wave_mode);
    
    
    // 循环运行动画
    while (true) {
        // 根据当前模式执行相应动画
        switch (current_mode) {
            case WS2812_MODE_BLINK_BLUE:
                run_blink_blue_animation(led_strip);
                break;

            case WS2812_MODE_BLINK_RED:
                run_blink_red_animation(led_strip);
                break;

            case WS2812_MODE_BLINK_GREEN:
                run_blink_green_animation(led_strip);
                break;

            case WS2812_MODE_RAINBOW:
                run_rainbow_animation(led_strip);
                break;

            case WS2812_MODE_CHASE:
                run_chase_animation(led_strip);
                break;
                
            case WS2812_MODE_SOLID:
                run_solid_animation(led_strip);
                break;

            case WS2812_MODE_DIM:
                run_dim_animation(led_strip);
                break;
                
            case WS2812_MODE_OFF:
                turn_off_leds(led_strip);
                break;
                
            case WS2812_MODE_BREATHING:
                run_breathing_animation(led_strip);
                break;
                
            case WS2812_MODE_FIRE:
                run_fire_animation(led_strip);
                break;
                
            case WS2812_MODE_TWINKLE:
                run_twinkle_animation(led_strip);
                break;
                
            case WS2812_MODE_WAVE:
                run_wave_animation(led_strip);
                break;
                
            case WS2812_MODE_ALTERNATE:
                run_alternate_animation(led_strip);
                break;
                
            case WS2812_MODE_PULSE:
                run_pulse_animation(led_strip);
                break;
                
            case WS2812_MODE_COMET:
                run_comet_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_CYCLE:
                run_rainbow_cycle_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_CHASE:
                run_rainbow_chase_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_WAVE:
                run_rainbow_wave_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_FIRE:
                run_rainbow_fire_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_TWINKLE:
                run_rainbow_twinkle_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_BREATHING:
                run_rainbow_breathing_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_PULSE:
                run_rainbow_pulse_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_ALTERNATE:
                run_rainbow_alternate_animation(led_strip);
                break;
                
            case WS2812_MODE_RAINBOW_COMET:
                run_rainbow_comet_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_WIPE:
                run_color_wipe_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_CHASE:
                run_color_chase_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_WAVE:
                run_color_wave_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_FIRE:
                run_color_fire_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_BREATHING:
                run_color_breathing_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_PULSE:
                run_color_pulse_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_ALTERNATE:
                run_color_alternate_animation(led_strip);
                break;
                
            case WS2812_MODE_COLOR_COMET:
                run_color_comet_animation(led_strip);
                break;
                
            default:
                // 默认为彩虹模式
                run_rainbow_animation(led_strip);
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延时，防止CPU占用过高
    }
}

// 启动测试任务
extern "C" void ws2812_start() {
    xTaskCreate(ws2812_task, "ws2812_test", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "WS2812测试任务已启动");
} 