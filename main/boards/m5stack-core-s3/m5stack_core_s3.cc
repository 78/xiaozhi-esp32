#include "wifi_board.h"
#include "audio_codecs/cores3_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9341.h>
#include <esp_timer.h>

#define TAG "M5StackCoreS3Board"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_20_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_20_4);

// AXP2101 电源管理芯片类
class Axp2101 : public I2cDevice {
public:
    // 电源初始化
    Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t data = ReadReg(0x90);
        data |= 0b10110100;  // 设置寄存器值
        WriteReg(0x90, data);
        WriteReg(0x99, (0b11110 - 5));  // 设置亮度
        WriteReg(0x97, (0b11110 - 2));  // 设置亮度
        WriteReg(0x69, 0b00110101);  // 配置寄存器
        WriteReg(0x30, 0b111111);  // 配置寄存器
        WriteReg(0x90, 0xBF);  // 配置寄存器
        WriteReg(0x94, 33 - 5);  // 设置亮度
        WriteReg(0x95, 33 - 5);  // 设置亮度
    }

    // 获取电池电流方向
    int GetBatteryCurrentDirection() {
        return (ReadReg(0x01) & 0b01100000) >> 5;
    }

    // 判断是否正在充电
    bool IsCharging() {
        return GetBatteryCurrentDirection() == 1;
    }

    // 获取电池电量
    int GetBatteryLevel() {
        return ReadReg(0xA4);
    }

    // 设置背光亮度
    void SetBrightness(uint8_t brightness) {
        brightness = ((brightness + 641) >> 5);  // 亮度计算
        WriteReg(0x99, brightness);  // 设置亮度
    }
};

// AXP2101 背光控制类
class Axp2101Backlight : public Backlight {
public:
    Axp2101Backlight(Axp2101 *axp2101) : axp2101_(axp2101) {}

    ~Axp2101Backlight() { ESP_LOGI(TAG, "Destroy Axp2101Backlight"); }

    // 设置背光亮度实现
    void SetBrightnessImpl(uint8_t brightness) override;

private:
    Axp2101 *axp2101_;  // AXP2101 实例
};
    
// 设置背光亮度实现
void Axp2101Backlight::SetBrightnessImpl(uint8_t brightness) {
    axp2101_->SetBrightness(brightness);  // 调用AXP2101设置亮度
}

// AW9523 IO扩展芯片类
class Aw9523 : public I2cDevice {
public:
    // IO扩展初始化
    Aw9523(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x02, 0b00000111);  // 配置P0
        WriteReg(0x03, 0b10001111);  // 配置P1
        WriteReg(0x04, 0b00011000);  // 配置CONFIG_P0
        WriteReg(0x05, 0b00001100);  // 配置CONFIG_P1
        WriteReg(0x11, 0b00010000);  // 配置GCR，P0端口为推挽模式
        WriteReg(0x12, 0b11111111);  // 配置LEDMODE_P0
        WriteReg(0x13, 0b11111111);  // 配置LEDMODE_P1
    }

    // 重置AW88298音频芯片
    void ResetAw88298() {
        ESP_LOGI(TAG, "Reset AW88298");
        WriteReg(0x02, 0b00000011);  // 重置AW88298
        vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(0x02, 0b00000111);  // 恢复AW88298
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 重置ILI9342显示屏
    void ResetIli9342() {
        ESP_LOGI(TAG, "Reset IlI9342");
        WriteReg(0x03, 0b10000001);  // 重置ILI9342
        vTaskDelay(pdMS_TO_TICKS(20));
        WriteReg(0x03, 0b10000011);  // 恢复ILI9342
        vTaskDelay(pdMS_TO_TICKS(10));
    }
};

// FT6336 触摸屏芯片类
class Ft6336 : public I2cDevice {
public:
    // 触摸点结构体
    struct TouchPoint_t {
        int num = 0;  // 触摸点数量
        int x = -1;   // 触摸点X坐标
        int y = -1;   // 触摸点Y坐标
    };
    
    // 构造函数，初始化触摸屏
    Ft6336(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA3);  // 读取芯片ID
        ESP_LOGI(TAG, "Get chip ID: 0x%02X", chip_id);  // 打印芯片ID
        read_buffer_ = new uint8_t[6];  // 分配读取缓冲区
    }

    // 析构函数，释放缓冲区
    ~Ft6336() {
        delete[] read_buffer_;
    }

    // 更新触摸点信息
    void UpdateTouchPoint() {
        ReadRegs(0x02, read_buffer_, 6);  // 从寄存器读取触摸点数据
        tp_.num = read_buffer_[0] & 0x0F;  // 解析触摸点数量
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];  // 解析X坐标
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];  // 解析Y坐标
    }

    // 获取当前触摸点信息
    const TouchPoint_t& GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t* read_buffer_ = nullptr;  // 读取缓冲区
    TouchPoint_t tp_;  // 当前触摸点信息
};

// M5StackCoreS3Board 类
class M5StackCoreS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;  // I2C总线句柄
    Axp2101* axp2101_;  // AXP2101电源管理芯片实例
    Aw9523* aw9523_;  // AW9523 IO扩展芯片实例
    Ft6336* ft6336_;  // FT6336触摸屏芯片实例
    LcdDisplay* display_;  // LCD显示设备实例
    esp_timer_handle_t touchpad_timer_;  // 触摸屏定时器句柄

    // 初始化I2C总线
    void InitializeI2c() {
        // I2C总线配置
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,  // I2C端口号
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,  // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,  // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // 时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));  // 初始化I2C总线
    }

    // I2C设备检测
    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));  // 探测设备
                if (ret == ESP_OK) {
                    printf("%02x ", address);  // 设备存在
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");  // 超时
                } else {
                    printf("-- ");  // 设备不存在
                }
            }
            printf("\r\n");
        }
    }

    // 初始化AXP2101电源管理芯片
    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        axp2101_ = new Axp2101(i2c_bus_, 0x34);  // 初始化AXP2101
    }

    // 初始化AW9523 IO扩展芯片
    void InitializeAw9523() {
        ESP_LOGI(TAG, "Init AW9523");
        aw9523_ = new Aw9523(i2c_bus_, 0x58);  // 初始化AW9523
        vTaskDelay(pdMS_TO_TICKS(50));  // 延迟50ms
    }

    // 触摸屏定时器回调函数
    static void touchpad_timer_callback(void* arg) {
        auto& board = (M5StackCoreS3Board&)Board::GetInstance();
        auto touchpad = board.GetTouchpad();
        static bool was_touched = false;
        static int64_t touch_start_time = 0;
        const int64_t TOUCH_THRESHOLD_MS = 500;  // 触摸时长阈值，超过500ms视为长按
        
        touchpad->UpdateTouchPoint();  // 更新触摸点信息
        auto touch_point = touchpad->GetTouchPoint();
        
        // 检测触摸开始
        if (touch_point.num > 0 && !was_touched) {
            was_touched = true;
            touch_start_time = esp_timer_get_time() / 1000; // 转换为毫秒
        } 
        // 检测触摸释放
        else if (touch_point.num == 0 && was_touched) {
            was_touched = false;
            int64_t touch_duration = (esp_timer_get_time() / 1000) - touch_start_time;
            
            // 只有短触才触发
            if (touch_duration < TOUCH_THRESHOLD_MS) {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateStarting && 
                    !WifiStation::GetInstance().IsConnected()) {
                    board.ResetWifiConfiguration();  // 重置WiFi配置
                }
                app.ToggleChatState();  // 切换聊天状态
            }
        }
    }

    // 初始化FT6336触摸屏
    void InitializeFt6336TouchPad() {
        ESP_LOGI(TAG, "Init FT6336");
        ft6336_ = new Ft6336(i2c_bus_, 0x38);  // 初始化FT6336
        
        // 创建定时器，10ms 间隔
        esp_timer_create_args_t timer_args = {
            .callback = touchpad_timer_callback,  // 定时器回调函数
            .arg = NULL,  // 回调函数参数
            .dispatch_method = ESP_TIMER_TASK,  // 定时器分发方法
            .name = "touchpad_timer",  // 定时器名称
            .skip_unhandled_events = true,  // 跳过未处理的事件
        };
        
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touchpad_timer_));  // 创建定时器
        ESP_ERROR_CHECK(esp_timer_start_periodic(touchpad_timer_, 10 * 1000)); // 10ms = 10000us
    }

    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_37;  // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO引脚（未连接）
        buscfg.sclk_io_num = GPIO_NUM_36;  // SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // QUADWP引脚（未连接）
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // QUADHD引脚（未连接）
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化SPI总线
    }

    // 初始化ILI9342显示屏
    void InitializeIli9342Display() {
        ESP_LOGI(TAG, "Init IlI9342");

        esp_lcd_panel_io_handle_t panel_io = nullptr;  // 面板IO句柄
        esp_lcd_panel_handle_t panel = nullptr;  // 面板句柄

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_3;  // CS引脚
        io_config.dc_gpio_num = GPIO_NUM_35;  // DC引脚
        io_config.spi_mode = 2;  // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000;  // 像素时钟频率
        io_config.trans_queue_depth = 10;  // 传输队列深度
        io_config.lcd_cmd_bits = 8;  // LCD命令位数
        io_config.lcd_param_bits = 8;  // LCD参数位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 初始化面板IO

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;  // 复位引脚（未连接）
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;  // RGB元素顺序
        panel_config.bits_per_pixel = 16;  // 每像素位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));  // 初始化LCD面板
        
        esp_lcd_panel_reset(panel);  // 复位面板
        aw9523_->ResetIli9342();  // 重置ILI9342显示屏

        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_invert_color(panel, true);  // 反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换XY轴
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
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Backlight"));  // 添加背光设备
        thing_manager.AddThing(iot::CreateThing("Battery"));  // 添加电池设备
    }

public:
    // 构造函数，初始化开发板
    M5StackCoreS3Board() {
        InitializeI2c();  // 初始化I2C
        InitializeAxp2101();  // 初始化AXP2101
        InitializeAw9523();  // 初始化AW9523
        I2cDetect();  // 检测I2C设备
        InitializeSpi();  // 初始化SPI
        InitializeIli9342Display();  // 初始化显示屏
        InitializeIot();  // 初始化物联网
        InitializeFt6336TouchPad();  // 初始化触摸屏
        GetBacklight()->SetBrightness(100);  // 设置背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec* GetAudioCodec() override {
        static CoreS3AudioCodec audio_codec(i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,  // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,  // 输出采样率
            AUDIO_I2S_GPIO_MCLK,  // MCLK引脚
            AUDIO_I2S_GPIO_BCLK,  // BCLK引脚
            AUDIO_I2S_GPIO_WS,  // WS引脚
            AUDIO_I2S_GPIO_DOUT,  // 数据输出引脚
            AUDIO_I2S_GPIO_DIN,  // 数据输入引脚
            AUDIO_CODEC_AW88298_ADDR,  // AW88298地址
            AUDIO_CODEC_ES7210_ADDR,  // ES7210地址
            AUDIO_INPUT_REFERENCE);  // 输入参考电压
        return &audio_codec;
    }

    // 获取显示设备
    virtual Display* GetDisplay() override {
        return display_;
    }

    // 获取电池电量和充电状态
    virtual bool GetBatteryLevel(int &level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;
        level = axp2101_->GetBatteryLevel();  // 获取电池电量
        charging = axp2101_->IsCharging();  // 获取充电状态
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);  // 打印电池信息
        }
        return true;
    }

    // 获取背光控制器
    virtual Backlight *GetBacklight() override {
        static Axp2101Backlight backlight(axp2101_);  // 初始化背光控制器
        return &backlight;
    }

    // 获取触摸屏设备
    Ft6336* GetTouchpad() {
        return ft6336_;
    }
};

// 声明开发板实例
DECLARE_BOARD(M5StackCoreS3Board);