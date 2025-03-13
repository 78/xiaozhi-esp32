#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "i2c_device.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/ledc.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st77916.h>
#include <esp_timer.h>

#define TAG "TaijiPiS3Board"  // 定义日志标签

// 声明字体
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

// CST816S 触摸屏设备类
class Cst816s : public I2cDevice {
public:
    // 触摸点结构体
    struct TouchPoint_t {
        int num = 0;  // 触摸点数量
        int x = -1;   // 触摸点 X 坐标
        int y = -1;   // 触摸点 Y 坐标
    };

    // 构造函数，初始化 I2C 设备并读取芯片 ID
    Cst816s(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA3);  // 读取芯片 ID
        ESP_LOGI(TAG, "Get chip ID: 0x%02X", chip_id);  // 打印芯片 ID
        read_buffer_ = new uint8_t[6];  // 分配读取缓冲区
    }

    // 析构函数，释放缓冲区
    ~Cst816s() {
        delete[] read_buffer_;
    }

    // 更新触摸点数据
    void UpdateTouchPoint() {
        ReadRegs(0x02, read_buffer_, 6);  // 从寄存器读取触摸点数据
        tp_.num = read_buffer_[0] & 0x0F;  // 解析触摸点数量
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];  // 解析 X 坐标
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];  // 解析 Y 坐标
    }

    // 获取当前触摸点数据
    const TouchPoint_t& GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t* read_buffer_ = nullptr;  // 读取缓冲区
    TouchPoint_t tp_;  // 当前触摸点数据
};

// TaijiPi S3 开发板类
class TaijiPiS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;  // I2C 总线句柄
    Cst816s* cst816s_;  // CST816S 触摸屏设备
    LcdDisplay* display_;  // LCD 显示屏
    esp_timer_handle_t touchpad_timer_;  // 触摸屏定时器

    // 初始化 I2C 总线
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,  // I2C 端口号
            .sda_io_num = TP_PIN_NUM_TP_SDA,  // SDA 引脚号
            .scl_io_num = TP_PIN_NUM_TP_SCL,  // SCL 引脚号
            .clk_source = I2C_CLK_SRC_DEFAULT,  // 时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));  // 创建 I2C 总线
    }

    // 触摸屏定时器回调函数
    static void touchpad_timer_callback(void* arg) {
        auto& board = (TaijiPiS3Board&)Board::GetInstance();  // 获取开发板实例
        auto touchpad = board.GetTouchpad();  // 获取触摸屏设备
        static bool was_touched = false;  // 上次触摸状态
        static int64_t touch_start_time = 0;  // 触摸开始时间
        const int64_t TOUCH_THRESHOLD_MS = 500;  // 触摸时长阈值，超过500ms视为长按
        
        touchpad->UpdateTouchPoint();  // 更新触摸点数据
        auto touch_point = touchpad->GetTouchPoint();  // 获取当前触摸点数据
        
        // 检测触摸开始
        if (touch_point.num > 0 && !was_touched) {
            was_touched = true;
            touch_start_time = esp_timer_get_time() / 1000;  // 记录触摸开始时间
        } 
        // 检测触摸释放
        else if (touch_point.num == 0 && was_touched) {
            was_touched = false;
            int64_t touch_duration = (esp_timer_get_time() / 1000) - touch_start_time;  // 计算触摸时长
            
            // 只有短触才触发
            if (touch_duration < TOUCH_THRESHOLD_MS) {
                auto& app = Application::GetInstance();  // 获取应用实例
                if (app.GetDeviceState() == kDeviceStateStarting && 
                    !WifiStation::GetInstance().IsConnected()) {
                    board.ResetWifiConfiguration();  // 重置 WiFi 配置
                }
                app.ToggleChatState();  // 切换聊天状态
            }
        }
    }

    // 初始化 CST816S 触摸屏
    void InitializeCst816sTouchPad() {
        ESP_LOGI(TAG, "Init Cst816s");
        cst816s_ = new Cst816s(i2c_bus_, 0x15);  // 创建 CST816S 设备实例
        
        // 创建定时器，10ms 间隔
        esp_timer_create_args_t timer_args = {
            .callback = touchpad_timer_callback,  // 定时器回调函数
            .arg = NULL,  // 回调函数参数
            .dispatch_method = ESP_TIMER_TASK,  // 定时器调度方式
            .name = "touchpad_timer",  // 定时器名称
            .skip_unhandled_events = true,  // 跳过未处理的事件
        };
        
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touchpad_timer_));  // 创建定时器
        ESP_ERROR_CHECK(esp_timer_start_periodic(touchpad_timer_, 10 * 1000));  // 启动定时器，10ms 间隔
    }

    // 设置 LCD 背光亮度
    void BspLcdBlSet(int brightness_percent) {
        if (brightness_percent > 100) {
            brightness_percent = 100;  // 限制最大亮度为 100%
        }
        if (brightness_percent < 0) {
            brightness_percent = 0;  // 限制最小亮度为 0%
        }

        ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
        uint32_t duty_cycle = (1023 * brightness_percent) / 100;  // 计算占空比
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle);  // 设置占空比
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);  // 更新占空比
    }

    // 初始化 SPI 总线
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");

        const spi_bus_config_t bus_config = TAIJIPI_ST77916_PANEL_BUS_QSPI_CONFIG(QSPI_PIN_NUM_LCD_PCLK,
                                                                        QSPI_PIN_NUM_LCD_DATA0,
                                                                        QSPI_PIN_NUM_LCD_DATA1,
                                                                        QSPI_PIN_NUM_LCD_DATA2,
                                                                        QSPI_PIN_NUM_LCD_DATA3,
                                                                        QSPI_LCD_H_RES * 80 * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(QSPI_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));  // 初始化 SPI 总线
    }

    // 初始化 ST77916 显示屏
    void Initializest77916Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;  // 面板 IO 句柄
        esp_lcd_panel_handle_t panel = nullptr;  // 面板句柄

        ESP_LOGI(TAG, "Install panel IO");
        
        const esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(QSPI_PIN_NUM_LCD_CS, NULL, NULL);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_LCD_HOST, &io_config, &panel_io));  // 创建面板 IO

        ESP_LOGI(TAG, "Install ST77916 panel driver");
        
        st77916_vendor_config_t vendor_config = {
            .flags = {
                .use_qspi_interface = 1,  // 使用 QSPI 接口
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = QSPI_PIN_NUM_LCD_RST,  // 复位引脚
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  // RGB 元素顺序
            .bits_per_pixel = QSPI_LCD_BIT_PER_PIXEL,  // 每像素位数
            .vendor_config = &vendor_config,  // 供应商配置
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(panel_io, &panel_config, &panel));  // 创建 ST77916 面板

        esp_lcd_panel_reset(panel);  // 复位面板
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_disp_on_off(panel, true);  // 打开显示
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换 XY 轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,  // 文本字体
                                        .icon_font = &font_awesome_20_4,  // 图标字体
                                        .emoji_font = font_emoji_64_init(),  // 表情字体
                                    });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();  // 获取物联网设备管理器实例
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
    }

    // 初始化静音功能
    void InitializeMute() {
        gpio_reset_pin(AUDIO_MUTE_PIN);  // 复位静音引脚
        gpio_set_direction(AUDIO_MUTE_PIN, GPIO_MODE_OUTPUT);  // 设置引脚为输出模式
        gpio_set_level(AUDIO_MUTE_PIN, 1);  // 设置引脚电平为高（静音）
    }

public:
    // 构造函数，初始化开发板
    TaijiPiS3Board() {
        InitializeI2c();  // 初始化 I2C
        InitializeCst816sTouchPad();  // 初始化触摸屏
        InitializeSpi();  // 初始化 SPI
        Initializest77916Display();  // 初始化显示屏
        InitializeIot();  // 初始化物联网
        InitializeMute();  // 初始化静音
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,  // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,  // 输出采样率
            AUDIO_I2S_GPIO_BCLK,  // I2S 位时钟引脚
            AUDIO_I2S_GPIO_WS,  // I2S 字选择引脚
            AUDIO_I2S_GPIO_DOUT,  // I2S 数据输出引脚
            AUDIO_MIC_SCK_PIN,  // 麦克风时钟引脚
            AUDIO_MIC_WS_PIN,  // 麦克风字选择引脚
            AUDIO_MIC_SD_PIN  // 麦克风数据引脚
        );
        return &audio_codec;
    }

    // 获取显示屏
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // 获取背光控制器
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建背光控制器实例
        return &backlight;
    }

    // 获取触摸屏设备
    Cst816s* GetTouchpad() {
        return cst816s_;
    }
};

// 声明开发板实例
DECLARE_BOARD(TaijiPiS3Board);