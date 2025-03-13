#include "wifi_board.h"
#include "tcamerapluss3_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>

#define TAG "LilygoTCameraPlusS3Board" // 日志标签

// 声明字体
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

// Cst816x 类，继承自 I2cDevice，用于控制 CST816x 触摸芯片
class Cst816x : public I2cDevice {
public:
    // 触摸点结构体
    struct TouchPoint_t {
        int num = 0; // 触摸点数量
        int x = -1;  // 触摸点 X 坐标
        int y = -1;  // 触摸点 Y 坐标
    };

    // 构造函数，初始化 CST816x
    Cst816x(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA7); // 读取芯片 ID
        ESP_LOGI(TAG, "Get chip ID: 0x%02X", chip_id); // 打印芯片 ID
        read_buffer_ = new uint8_t[6]; // 分配读取缓冲区
    }

    // 析构函数，释放资源
    ~Cst816x() {
        delete[] read_buffer_; // 释放缓冲区
    }

    // 更新触摸点信息
    void UpdateTouchPoint() {
        ReadRegs(0x02, read_buffer_, 6); // 读取触摸点数据
        tp_.num = read_buffer_[0] & 0x0F; // 解析触摸点数量
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2]; // 解析 X 坐标
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4]; // 解析 Y 坐标
    }

    // 获取触摸点信息
    const TouchPoint_t &GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t *read_buffer_ = nullptr; // 读取缓冲区
    TouchPoint_t tp_; // 触摸点信息
};

// LilygoTCameraPlusS3Board 类，继承自 WifiBoard
class LilygoTCameraPlusS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // I2C 总线句柄
    Cst816x *cst816d_; // CST816x 触摸芯片对象
    LcdDisplay *display_; // LCD 显示对象
    Button key1_button_; // 按钮对象，用于处理按钮事件

    // 初始化 I2C 总线
    void InitI2c() {
        // 配置 I2C 总线
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = I2C_NUM_0, // I2C 端口号
            .sda_io_num = TOUCH_I2C_SDA_PIN, // SDA 引脚号
            .scl_io_num = TOUCH_I2C_SCL_PIN, // SCL 引脚号
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 毛刺忽略计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉
            }
        };
        // 创建 I2C 主总线
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_));
    }

    // I2C 设备探测
    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200)); // 探测设备
                if (ret == ESP_OK) {
                    printf("%02x ", address); // 设备存在
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU "); // 超时
                } else {
                    printf("-- "); // 设备不存在
                }
            }
            printf("\r\n");
        }
    }

    // 触摸板守护任务
    static void touchpad_daemon(void *param) {
        vTaskDelay(pdMS_TO_TICKS(2000)); // 延迟 2 秒
        auto &board = (LilygoTCameraPlusS3Board&)Board::GetInstance(); // 获取板级对象
        auto touchpad = board.GetTouchpad(); // 获取触摸板对象
        bool was_touched = false; // 是否被触摸
        while (1) {
            touchpad->UpdateTouchPoint(); // 更新触摸点信息
            if (touchpad->GetTouchPoint().num > 0) { // 如果有触摸点
                // 按下事件
                if (!was_touched) {
                    was_touched = true;
                    Application::GetInstance().ToggleChatState(); // 切换聊天状态
                }
            }
            // 释放事件
            else if (was_touched) {
                was_touched = false;
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // 延迟 50ms
        }
        vTaskDelete(NULL); // 删除任务
    }

    // 初始化 CST816x 触摸芯片
    void InitCst816d() {
        ESP_LOGI(TAG, "Init CST816x"); // 打印日志
        cst816d_ = new Cst816x(i2c_bus_, 0x15); // 创建 CST816x 对象
        xTaskCreate(touchpad_daemon, "tp", 2048, NULL, 5, NULL); // 创建触摸板守护任务
    }

    // 初始化 SPI 总线
    void InitSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI; // MOSI 引脚号
        buscfg.miso_io_num = GPIO_NUM_NC; // MISO 引脚未使用
        buscfg.sclk_io_num = DISPLAY_SCLK; // SCK 引脚号
        buscfg.quadwp_io_num = GPIO_NUM_NC; // QUADWP 引脚未使用
        buscfg.quadhd_io_num = GPIO_NUM_NC; // QUADHD 引脚未使用
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t); // 最大传输大小
        // 初始化 SPI 总线
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // 初始化 ST7789 显示屏
    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr; // LCD 面板 IO 句柄
        esp_lcd_panel_handle_t panel = nullptr; // LCD 面板句柄

        // 初始化液晶屏控制 IO
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = LCD_CS; // CS 引脚号
        io_config.dc_gpio_num = LCD_DC; // DC 引脚号
        io_config.spi_mode = 0; // SPI 模式
        io_config.pclk_hz = 60 * 1000 * 1000; // 时钟频率
        io_config.trans_queue_depth = 10; // 传输队列深度
        io_config.lcd_cmd_bits = 8; // LCD 命令位数
        io_config.lcd_param_bits = 8; // LCD 参数位数
        // 创建 SPI 面板 IO
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片 ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = LCD_RST; // 复位引脚号
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; // RGB 元素顺序
        panel_config.bits_per_pixel = 16; // 每个像素的位数
        // 创建 ST7789 面板
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel)); // 复位面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel)); // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY)); // 交换 XY 轴
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y)); // 镜像显示
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true)); // 反转颜色

        // 创建 SPI LCD 显示对象
        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                     {
                                         .text_font = &font_puhui_16_4, // 文本字体
                                         .icon_font = &font_awesome_16_4, // 图标字体
                                         .emoji_font = font_emoji_32_init(), // 表情字体
                                     });
    }

    // 初始化按钮
    void InitializeButtons() {
        // 设置按钮点击事件
        key1_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置 WiFi 配置
            }
            app.ToggleChatState(); // 切换聊天状态
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加扬声器设备
    }

public:
    // 构造函数
    LilygoTCameraPlusS3Board() : key1_button_(KEY1_BUTTON_GPIO) {
        InitI2c(); // 初始化 I2C 总线
        InitCst816d(); // 初始化 CST816x 触摸芯片
        I2cDetect(); // 探测 I2C 设备
        InitSpi(); // 初始化 SPI 总线
        InitializeSt7789Display(); // 初始化 ST7789 显示屏
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
        GetBacklight()->RestoreBrightness(); // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec *GetAudioCodec() override {
        static Tcamerapluss3AudioCodec audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, // 音频输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE, // 音频输出采样率
            AUDIO_MIC_I2S_GPIO_BCLK, // 麦克风 I2S 位时钟引脚
            AUDIO_MIC_I2S_GPIO_WS, // 麦克风 I2S 字选择引脚
            AUDIO_MIC_I2S_GPIO_DATA, // 麦克风 I2S 数据引脚
            AUDIO_SPKR_I2S_GPIO_BCLK, // 扬声器 I2S 位时钟引脚
            AUDIO_SPKR_I2S_GPIO_LRCLK, // 扬声器 I2S 字选择引脚
            AUDIO_SPKR_I2S_GPIO_DATA, // 扬声器 I2S 数据引脚
            AUDIO_INPUT_REFERENCE); // 音频输入参考
        return &audio_codec;
    }

    // 获取显示对象
    virtual Display *GetDisplay() override {
        return display_;
    }

    // 获取背光对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    // 获取触摸板对象
    Cst816x *GetTouchpad() {
        return cst816d_;
    }
};

// 声明板级对象
DECLARE_BOARD(LilygoTCameraPlusS3Board);