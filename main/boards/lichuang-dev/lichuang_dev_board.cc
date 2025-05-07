#include "wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include "audio_processing/audio_processor.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>




#include "images/doufu/output_0001.h"
#include "images/doufu/output_0002.h"
#include "images/doufu/output_0003.h"
#include "images/doufu/output_0004.h"
#include "images/doufu/output_0005.h"
#include "images/doufu/output_0006.h"
#include "images/doufu/output_0007.h"
#include "images/doufu/output_0008.h"
#include "images/doufu/output_0009.h"
#include "images/doufu/output_0010.h"

#define TAG "LichuangDevBoard"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class Pca9557 : public I2cDevice {
public:
    Pca9557(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x01, 0x03);
        WriteReg(0x03, 0xf8);
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data = ReadReg(0x01);
        data = (data & ~(1 << bit)) | (level << bit);
        WriteReg(0x01, data);
    }
};


class LichuangDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t pca9557_handle_;
    Button boot_button_;
    LcdDisplay* display_;
    Pca9557* pca9557_;
    TaskHandle_t image_task_handle_ = nullptr; // 图片显示任务句柄
    
    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // Initialize PCA9557
        pca9557_ = new Pca9557(i2c_bus_, 0x19);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_40;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_41;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC;
        io_config.dc_gpio_num = GPIO_NUM_39;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        pca9557_->SetOutputState(0, 0);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
                                        .emoji_font = font_emoji_32_init(),
#else
                                        .emoji_font = font_emoji_64_init(),
#endif
                                    });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
    }
    
    // 启动图片循环显示任务
    void StartImageSlideshow() {
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 4096, this, 3, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
    }
    
    // 图片循环显示任务函数
    static void ImageSlideshowTask(void* arg) {
        LichuangDevBoard* board = static_cast<LichuangDevBoard*>(arg);
        Display* display = board->GetDisplay();
        
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示设备");
            vTaskDelete(NULL);
            return;
        }
        
        // 获取Application实例
        auto& app = Application::GetInstance();
        
        // 创建画布（如果不存在）
        if (!display->HasCanvas()) {
            display->CreateCanvas();
        }
        
        // 设置图片显示参数
        int imgWidth = 320;
        int imgHeight = 240;
        int x = 0;
        int y = 0;
        
        // 设置图片数组
        const uint8_t* imageArray[] = {
            gImage_output_0001,
            gImage_output_0002,
            gImage_output_0003,
            gImage_output_0004,
            gImage_output_0005,
            gImage_output_0006,
            gImage_output_0007,
            gImage_output_0008,
            gImage_output_0009,
            gImage_output_0010,
            gImage_output_0009,
            gImage_output_0008,
            gImage_output_0007,
            gImage_output_0006,
            gImage_output_0005,
            gImage_output_0004,
            gImage_output_0003,
            gImage_output_0002,
            gImage_output_0001
        };
        const int totalImages = sizeof(imageArray) / sizeof(imageArray[0]);
        
        // 创建临时缓冲区用于字节序转换
        uint16_t* convertedData = new uint16_t[imgWidth * imgHeight];
        if (!convertedData) {
            ESP_LOGE(TAG, "无法分配内存进行图像转换");
            vTaskDelete(NULL);
            return;
        }
        
        // 先显示第一张图片
        int currentIndex = 0;
        const uint8_t* currentImage = imageArray[currentIndex];
        
        // 转换并显示第一张图片
        for (int i = 0; i < imgWidth * imgHeight; i++) {
            uint16_t pixel = ((uint16_t*)currentImage)[i];
            convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
        }
        
        // 使用DrawImageOnCanvas而不是DrawImage
        display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
        ESP_LOGI(TAG, "初始显示图片");
        
        // 持续监控和处理图片显示
        TickType_t lastUpdateTime = xTaskGetTickCount();
        const TickType_t cycleInterval = pdMS_TO_TICKS(120); // 图片切换间隔60毫秒
        
        // 定义用于判断是否正在播放音频的变量
        bool isAudioPlaying = false;
        bool wasAudioPlaying = false;
        
        while (true) {
            // 检查是否正在播放音频 - 使用应用程序状态判断
            isAudioPlaying = (app.GetDeviceState() == kDeviceStateSpeaking);
            
            TickType_t currentTime = xTaskGetTickCount();
            
            // 如果正在播放音频且时间到了切换间隔
            if (isAudioPlaying && (currentTime - lastUpdateTime >= cycleInterval)) {
                // 更新索引到下一张图片
                currentIndex = (currentIndex + 1) % totalImages;
                currentImage = imageArray[currentIndex];
                
                // 转换并显示新图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                // 使用DrawImageOnCanvas而不是DrawImage
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                ESP_LOGI(TAG, "循环显示图片");
                
                // 更新上次更新时间
                lastUpdateTime = currentTime;
            }
            // 如果不在播放音频但上一次检查时在播放，或者当前不在第一张图片
            else if ((!isAudioPlaying && wasAudioPlaying) || (!isAudioPlaying && currentIndex != 0)) {
                // 切换回第一张图片
                currentIndex = 0;
                currentImage = imageArray[currentIndex];
                
                // 转换并显示第一张图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                // 使用DrawImageOnCanvas而不是DrawImage
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                ESP_LOGI(TAG, "返回显示初始图片");
            }
            
            // 更新上一次音频播放状态
            wasAudioPlaying = isAudioPlaying;
            
            // 短暂延时，避免CPU占用过高
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 释放资源
        delete[] convertedData;
        vTaskDelete(NULL);
    }

public:
    LichuangDevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
        
        // 启动图片循环显示任务
        StartImageSlideshow();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(LichuangDevBoard);
