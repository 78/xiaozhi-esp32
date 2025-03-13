#include "wifi_board.h"
#include "tcircles3_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "esp_lcd_gc9d01n.h"

#define TAG "LilygoTCircleS3Board"  // 定义日志标签

LV_FONT_DECLARE(font_puhui_16_4);  // 声明字体
LV_FONT_DECLARE(font_awesome_16_4);

// CST816x 触摸屏设备类
class Cst816x : public I2cDevice {
public:
    // 触摸点结构体
    struct TouchPoint_t {
        int num = 0;  // 触摸点数量
        int x = -1;   // 触摸点X坐标
        int y = -1;   // 触摸点Y坐标
    };

    // 构造函数，初始化I2C设备并读取芯片ID
    Cst816x(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA7);  // 读取芯片ID
        ESP_LOGI(TAG, "Get chip ID: 0x%02X", chip_id);  // 打印芯片ID
        read_buffer_ = new uint8_t[6];  // 分配读取缓冲区
    }

    // 析构函数，释放缓冲区
    ~Cst816x() {
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
    const TouchPoint_t &GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t *read_buffer_ = nullptr;  // 读取缓冲区
    TouchPoint_t tp_;  // 当前触摸点信息
};

// Lilygo T-Circle S3 开发板类
class LilygoTCircleS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;  // I2C总线句柄
    Cst816x *cst816d_;  // CST816x触摸屏设备指针
    LcdDisplay *display_;  // LCD显示设备指针
    Button boot_button_;  // 启动按钮

    // 初始化I2C总线
    void InitI2c(){
        // I2C总线配置
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = I2C_NUM_0,  // I2C端口号
            .sda_io_num = TOUCH_I2C_SDA_PIN,  // SDA引脚
            .scl_io_num = TOUCH_I2C_SCL_PIN,  // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,  // 时钟源
            .glitch_ignore_cnt = 7,  // 毛刺忽略计数
            .intr_priority = 0,  // 中断优先级
            .trans_queue_depth = 0,  // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,  // 启用内部上拉
            }
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_));  // 初始化I2C总线
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

    // 触摸屏守护任务
    static void touchpad_daemon(void *param) {
        vTaskDelay(pdMS_TO_TICKS(2000));  // 延迟2秒
        auto &board = (LilygoTCircleS3Board&)Board::GetInstance();  // 获取开发板实例
        auto touchpad = board.GetTouchpad();  // 获取触摸屏设备
        bool was_touched = false;  // 是否被触摸
        while (1) {
            touchpad->UpdateTouchPoint();  // 更新触摸点信息
            if (touchpad->GetTouchPoint().num > 0){  // 如果有触摸点
                // 按下事件
                if (!was_touched) {
                    was_touched = true;
                    Application::GetInstance().ToggleChatState();  // 切换聊天状态
                }
            }
            // 释放事件
            else if (was_touched) {
                was_touched = false;
            }
            vTaskDelay(pdMS_TO_TICKS(50));  // 延迟50ms
        }
        vTaskDelete(NULL);  // 删除任务
    }

    // 初始化CST816x触摸屏
    void InitCst816d() {
        ESP_LOGI(TAG, "Init CST816x");  // 打印日志
        cst816d_ = new Cst816x(i2c_bus_, 0x15);  // 初始化CST816x设备
        xTaskCreate(touchpad_daemon, "tp", 2048, NULL, 5, NULL);  // 创建触摸屏守护任务
    }

    // 初始化SPI总线
    void InitSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI;  // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;  // MISO引脚（未连接）
        buscfg.sclk_io_num = DISPLAY_SCLK;  // SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;  // QUADWP引脚（未连接）
        buscfg.quadhd_io_num = GPIO_NUM_NC;  // QUADHD引脚（未连接）
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));  // 初始化SPI总线
    }

    // 初始化GC9D01N显示屏
    void InitGc9d01nDisplay() {
        ESP_LOGI(TAG, "Init GC9D01N");  // 打印日志

        esp_lcd_panel_io_handle_t panel_io = nullptr;  // 面板IO句柄
        esp_lcd_panel_handle_t panel = nullptr;  // 面板句柄

        ESP_LOGD(TAG, "Install panel IO");  // 打印调试日志
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;  // CS引脚
        io_config.dc_gpio_num = DISPLAY_DC;  // DC引脚
        io_config.spi_mode = 0;  // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000;  // 像素时钟频率
        io_config.trans_queue_depth = 10;  // 传输队列深度
        io_config.lcd_cmd_bits = 8;  // LCD命令位数
        io_config.lcd_param_bits = 8;  // LCD参数位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 初始化面板IO

        ESP_LOGD(TAG, "Install LCD driver");  // 打印调试日志
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST;  // 复位引脚
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;  // RGB元素顺序
        panel_config.bits_per_pixel = 16;  // 每像素位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9d01n(panel_io, &panel_config, &panel));  // 初始化LCD面板

        esp_lcd_panel_reset(panel);  // 复位面板
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_invert_color(panel, false);  // 反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);  // 交换XY轴
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 镜像显示

        // 初始化LCD显示设备
        display_ = new SpiLcdDisplay(panel_io, panel,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                  DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                  {
                                      .text_font = &font_puhui_16_4,  // 文本字体
                                      .icon_font = &font_awesome_16_4,  // 图标字体
                                      .emoji_font = font_emoji_32_init(),  // 表情字体
                                  });

        // 配置背光引脚
        gpio_config_t config;
        config.pin_bit_mask = BIT64(DISPLAY_BL);  // 背光引脚
        config.mode = GPIO_MODE_OUTPUT;  // 输出模式
        config.pull_up_en = GPIO_PULLUP_DISABLE;  // 禁用上拉
        config.pull_down_en = GPIO_PULLDOWN_ENABLE;  // 启用下拉
        config.intr_type = GPIO_INTR_DISABLE;  // 禁用中断
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
        config.hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE;  // 启用滞后滤波
#endif
        gpio_config(&config);  // 配置GPIO
        gpio_set_level(DISPLAY_BL, 0);  // 设置背光电平
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {  // 按钮点击事件
            auto& app = Application::GetInstance();  // 获取应用实例
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置WiFi配置
            }
            app.ToggleChatState();  // 切换聊天状态
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto &thing_manager = iot::ThingManager::GetInstance();  // 获取物联网设备管理器
        thing_manager.AddThing(iot::CreateThing("Speaker"));  // 添加扬声器设备
    }

public:
    // 构造函数，初始化开发板
    LilygoTCircleS3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitI2c();  // 初始化I2C
        InitCst816d();  // 初始化触摸屏
        I2cDetect();  // 检测I2C设备
        InitSpi();  // 初始化SPI
        InitGc9d01nDisplay();  // 初始化显示屏
        InitializeButtons();  // 初始化按钮
        InitializeIot();  // 初始化物联网
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
    }

    // 获取音频编解码器
    virtual AudioCodec *GetAudioCodec() override {
        static Tcircles3AudioCodec audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,  // 输入采样率
            AUDIO_OUTPUT_SAMPLE_RATE,  // 输出采样率
            AUDIO_MIC_I2S_GPIO_BCLK,  // 麦克风I2S BCLK引脚
            AUDIO_MIC_I2S_GPIO_WS,  // 麦克风I2S WS引脚
            AUDIO_MIC_I2S_GPIO_DATA,  // 麦克风I2S数据引脚
            AUDIO_SPKR_I2S_GPIO_BCLK,  // 扬声器I2S BCLK引脚
            AUDIO_SPKR_I2S_GPIO_LRCLK,  // 扬声器I2S LRCLK引脚
            AUDIO_SPKR_I2S_GPIO_DATA,  // 扬声器I2S数据引脚
            AUDIO_INPUT_REFERENCE);  // 输入参考电压
        return &audio_codec;
    }

    // 获取显示设备
    virtual Display *GetDisplay() override{
        return display_;
    }
    
    // 获取背光控制器
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 初始化背光控制器
        return &backlight;
    }

    // 获取触摸屏设备
    Cst816x *GetTouchpad() {
        return cst816d_;
    }
};

// 声明开发板实例
DECLARE_BOARD(LilygoTCircleS3Board);