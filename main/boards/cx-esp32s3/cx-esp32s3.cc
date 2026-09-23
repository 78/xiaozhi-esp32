/*
 * @LastEditors: T sir
 * @LastEditTime: 2026-05-28 14:35:45
 * @brief: CX-ESP32S3 开发板硬件配置与初始化
 */

// ==================== 应用层头文件 ====================
#include "application.h"      // 应用程序单例管理
#include "button.h"           // 按钮事件处理
#include "config.h"           // 项目配置文件
#include "wifi_board.h"       // WiFi 板基类

// ==================== ESP-IDF 驱动头文件 ====================
#include <esp_log.h>               // ESP 日志系统

// ==================== 自定义编解码器 ====================
#include "codecs/no_audio_codec.h"  // 无音频编解码器（PDM）


#define TAG "CXESP32S3"  // 日志标签，用于标识日志来源

/**
 * @brief CX-ESP32S3 板级配置类
 * @note 继承自 WifiBoard，实现硬件初始化和外设访问接口
 */
class cx_esp32s3 : public WifiBoard {
private:
    Button boot_button_;   // 启动按钮对象

    /**
     * @brief 初始化按钮事件
     * @note 单击按钮：启动时进入 WiFi 配网模式，运行时切换聊天状态
     */
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();  // 进入 WiFi 配网模式
                return;
            }
            app.ToggleChatState();  // 切换聊天状态（开始/停止对话）
        });

        // 双击功能已禁用（用于切换 AEC 回声消除模式）
        // #if CONFIG_USE_DEVICE_AEC
        //         boot_button_.OnDoubleClick([this]() {
        //             auto& app = Application::GetInstance();
        //             if (app.GetDeviceState() == kDeviceStateIdle) {
        //                 app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
        //             }
        //         });
        // #endif
    }

    /**
     * @brief 初始化 ST7789 LCD 显示屏（I80 接口）
     * @note 配置 I80 总线、面板 IO、面板驱动，并创建显示对象
     */
//     void st7789_i80_display() {
//         esp_lcd_panel_io_handle_t panel_io = nullptr;  // 面板 IO 句柄
//         esp_lcd_panel_handle_t panel = nullptr;        // 面板句柄
//         esp_lcd_i80_bus_handle_t i80_bus = NULL;       // I80 总线句柄
        
//         // 配置 I80 总线参数
//         esp_lcd_i80_bus_config_t bus_config = {
//             .dc_gpio_num = GPIO_NUM_1,    // DC 引脚：数据/命令选择
//             .wr_gpio_num = GPIO_NUM_41,   // WR 引脚：写时钟
//             .clk_src = LCD_CLK_SRC_DEFAULT,  // 时钟源
//             .data_gpio_nums = {           // 8 位数据线 GPIO
//                 GPIO_NUM_40, GPIO_NUM_38, GPIO_NUM_39, GPIO_NUM_48,
//                 GPIO_NUM_45, GPIO_NUM_21, GPIO_NUM_47, GPIO_NUM_14,
//             },
//             .bus_width = 8,               // 总线宽度：8 位
//             .max_transfer_bytes = DISPLAY_HEIGHT * DISPLAY_WIDTH * 2,  // 最大传输字节数
//             .dma_burst_size = 64,         // DMA 突发大小
//         };
//         ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));  // 创建 I80 总线

//         // 配置面板 IO 参数
//         esp_lcd_panel_io_i80_config_t io_config = {
//             .cs_gpio_num = GPIO_NUM_2,    // 片选引脚
//             .pclk_hz = 30 * DISPLAY_HEIGHT * DISPLAY_WIDTH,  // 像素时钟频率
//             .trans_queue_depth = 10,      // 传输队列深度
//             .lcd_cmd_bits = 8,            // 命令位数
//             .lcd_param_bits = 8,          // 参数位数
//             .dc_levels = {                // DC 电平配置
//                 .dc_idle_level = 0,
//                 .dc_cmd_level = 0,
//                 .dc_dummy_level = 0,
//                 .dc_data_level = 1,
//             },
//         };
//         ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &panel_io));  // 创建面板 IO

//         ESP_LOGI(TAG, "Install LCD driver of st7789");
        
//         // 配置面板设备参数
//         esp_lcd_panel_dev_config_t panel_config = {
//                 .reset_gpio_num = GPIO_NUM_NC,  // 复位引脚：无（通过 XL9555 控制）
//             .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  // RGB 元素顺序
//             .bits_per_pixel = 16,         // 每像素位数：16 位（RGB565）
//         };
//         ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));  // 创建 ST7789 面板

//         // 通过 XL9555 控制 LCD 复位
//         xl9555_pin_write(IO1_3, 1);       // 拉高复位引脚
//         vTaskDelay(pdMS_TO_TICKS(200));   // 延时 200ms
//         esp_lcd_panel_reset(panel);       // 复位面板
//         vTaskDelay(pdMS_TO_TICKS(200));   // 延时 200ms
//         esp_lcd_panel_init(panel);        // 初始化面板
        
//         // 配置显示方向
//         // esp_lcd_panel_invert_color(panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 颜色反转（已禁用）
//         esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);        // XY 交换
//         esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像翻转
//         esp_lcd_panel_disp_on_off(panel, true);  // 开启显示

//         // 根据配置创建显示对象
// #if CONFIG_USE_EMOTE_MESSAGE_STYLE
//         display_ = new emote::EmoteDisplay(panel, panel_io, DISPLAY_WIDTH, DISPLAY_HEIGHT);
// #else
//         display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
//                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
//                                      DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
// #endif
//     }

public:
    /**
     * @brief 构造函数：初始化板级硬件
     * @note 初始化顺序：XL9555 -> 显示屏 -> 按钮 -> 输出控制
     */
    cx_esp32s3() : boot_button_(BOOT_BUTTON_GPIO) {
        // InitializeI2c();   // I2C 初始化（已禁用）
        // InitializeSpi();   // SPI 初始化（已禁用）
        
        // xl9555_init(GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_NC, NULL);  // 初始化 XL9555（SCL/SDA 引脚）
        // xl9555_ioconfig(~(IO0_0 | IO1_2 | IO1_3) & 0xFFFF);        // 配置 IO 方向（输出引脚）
        // st7789_i80_display();   // 初始化显示屏
        InitializeButtons();    // 初始化按钮
        // xl9555_pin_write(IO0_0, 1);  // 控制 IO0_0 输出高电平
        // xl9555_pin_write(IO1_2, 1);  // 控制 IO1_2 输出高电平
        gpio_config_t io_conf = {}; 
        io_conf.intr_type = GPIO_INTR_DISABLE;       // 禁用中断
        io_conf.mode = GPIO_MODE_OUTPUT;             // 设置为输出模式
        io_conf.pin_bit_mask = (1ULL << GPIO_NUM_10); // 选择 IO10 引脚
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     // 禁用上拉（根据硬件需求，如果需要默认高电平且外部无下拉，可禁用上拉；若需强上拉可启用）
        
        gpio_config(&io_conf);                       // 应用配置
        gpio_set_level(GPIO_NUM_10, 1);              // 设置 IO10 输出高电平
               // ==================== 配置 IO18 (新增部分) ====================
        gpio_config_t io_conf_18 = {}; 
        io_conf_18.intr_type = GPIO_INTR_DISABLE;       // 禁用中断
        io_conf_18.mode = GPIO_MODE_OUTPUT;             // 设置为输出模式
        io_conf_18.pin_bit_mask = (1ULL << GPIO_NUM_18); // 选择 IO18 引脚
        io_conf_18.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉
        io_conf_18.pull_up_en = GPIO_PULLUP_ENABLE;     // 【关键】开启上拉
        
        gpio_config(&io_conf_18);                       // 应用配置
        gpio_set_level(GPIO_NUM_18, 0);                 // 【关键】设置 IO18 输出低电平
        // ============================================================
    }
    
    /**
     * @brief 获取音频编解码器对象
     * @return AudioCodec* 音频编解码器指针
     * @note 使用 PDM 接口，无外部编解码芯片
     */
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplexPdm audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,   // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,  // 输出采样率
            GPIO_NUM_46,               // PDM 数据输入引脚
            GPIO_NUM_9,                // PDM 时钟引脚
            GPIO_NUM_8,                // I2S 时钟引脚
            I2S_STD_SLOT_RIGHT,        // I2S 槽位
            GPIO_NUM_3,                // I2S 数据输出引脚
            GPIO_NUM_42                // I2S 帧时钟引脚
        );
        return &audio_codec;  
    }

    /**
     * @brief 获取显示对象
     * @return Display* 显示对象指针
     */
    // virtual Display* GetDisplay() override { 
    //     return display_; 
    // } 
};

// 注册板级配置到系统
DECLARE_BOARD(cx_esp32s3);