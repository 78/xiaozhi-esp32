#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "esp32_camera.h"
#include "skills/animation.h"
#include "qmi8658.h"
#include "interaction/event_engine.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <mutex>

#include "images/emotions/neutral/1.h"
#include "images/emotions/angry/1.h"
#include "images/emotions/angry/2.h"
#include "images/emotions/angry/3.h"
#include "images/emotions/angry/4.h"
#include "images/emotions/happy/1.h"
#include "images/emotions/happy/2.h"
#include "images/emotions/happy/3.h"
#include "images/emotions/laughting/1.h"
#include "images/emotions/sad/1.h"
#include "images/emotions/sad/2.h"
#include "images/emotions/sad/3.h"
#include "images/emotions/surprised/2.h"
#include "images/emotions/surprised/4.h"
#include "images/emotions/surprised/6.h"


#define TAG "ALichuangTest"

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

class CustomAudioCodec : public BoxAudioCodec {
private:
    Pca9557* pca9557_;

public:
    CustomAudioCodec(i2c_master_bus_handle_t i2c_bus, Pca9557* pca9557) 
        : BoxAudioCodec(i2c_bus, 
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
                       AUDIO_INPUT_REFERENCE),
          pca9557_(pca9557) {
    }

    virtual void EnableOutput(bool enable) override {
        BoxAudioCodec::EnableOutput(enable);
        if (enable) {
            pca9557_->SetOutputState(1, 1);
        } else {
            pca9557_->SetOutputState(1, 0);
        }
    }
};

class ALichuangTest : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t pca9557_handle_;
    Button boot_button_;
    AnimaDisplay* display_;
    Pca9557* pca9557_;
    Esp32Camera* camera_;
    Qmi8658* imu_ = nullptr;
    EventEngine* event_engine_ = nullptr;
    esp_timer_handle_t event_timer_ = nullptr;
    TaskHandle_t image_task_handle_ = nullptr; // 图片显示任务句柄
    // 情感相关成员变量
    std::string current_emotion_ = "neutral";
    mutable std::mutex emotion_mutex_;

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

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
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
        display_ = new AnimaDisplay(panel_io, panel,
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
    
    void StartImageSlideshow() {
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 4096, this, 3, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
    }
    
    // 根据情感获取对应的图片数组
    std::pair<const uint8_t**, int> GetEmotionImageArray(const std::string& emotion) {
        // 默认图片数组（neutral或未知情感时使用）
        static const uint8_t* neutral_images[] = {
            gImage_1  // neutral时只显示第一张静态图片
        };
        
        // 根据情感返回对应的图片数组
        if (emotion == "happy" || emotion == "funny") {
            // 开心相关情感 - 使用快节奏动画
            static const uint8_t* happy_images[] = {
                gImage_9, gImage_10, gImage_11
            };
            return {happy_images, 3};
        }
        else if (emotion == "laughting") {
            // 大笑情感
            static const uint8_t* angry_images[] = {
                gImage_12
            };
            return {angry_images, 1};
        }
        else if (emotion == "angry") {
            // 愤怒情感 - 使用较强烈的图片
            static const uint8_t* angry_images[] = {
                gImage_2, gImage_3, gImage_4, gImage_5
            };
            return {angry_images, 4};
        }
        else if (emotion == "sad" || emotion == "crying") {
            // 悲伤相关情感 - 使用较慢的动画
            static const uint8_t* sad_images[] = {
                gImage_23, gImage_24, gImage_25
            };
            return {sad_images, 3};
        }
        else if (emotion == "surprised" || emotion == "shocked") {
            // 惊讶相关情感 - 使用跳跃式动画
            static const uint8_t* surprised_images[] = {
                gImage_27, gImage_29, gImage_31
            };
            return {surprised_images, 3};
        }
        else {
            // neutral或其他情感 - 只显示静态图片
            return {neutral_images, 1};
        }
    }
    
    // 获取当前情感状态
    std::string GetCurrentEmotion() {
        std::lock_guard<std::mutex> lock(emotion_mutex_);
        return current_emotion_;
    }
    
    // 设置当前情感状态
    void SetCurrentEmotion(const std::string& emotion) {
        std::lock_guard<std::mutex> lock(emotion_mutex_);
        current_emotion_ = emotion;
        ESP_LOGI(TAG, "情感状态变更为: %s", emotion.c_str());
    }
    
    // 根据情感获取播放间隔（毫秒）
    int GetEmotionPlayInterval(const std::string& emotion) {
        if (emotion == "happy" || emotion == "laughing" || emotion == "funny") {
            return 50;  // 开心情感 - 快速播放
        }
        else if (emotion == "angry") {
            return 40;  // 愤怒情感 - 很快播放，表达强烈情绪
        }
        else if (emotion == "sad" || emotion == "crying") {
            return 120; // 悲伤情感 - 慢速播放
        }
        else if (emotion == "surprised" || emotion == "shocked") {
            return 80;  // 惊讶情感 - 中等速度
        }
        else if (emotion == "thinking") {
            return 150; // 思考情感 - 最慢播放
        }
        else {
            return 60;  // 默认间隔
        }
    }
    
    // 图片循环显示任务函数
    static void ImageSlideshowTask(void* arg) {
        ALichuangTest* board = static_cast<ALichuangTest*>(arg);
        AnimaDisplay* display = board->GetDisplay();
        
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示设备");
            vTaskDelete(NULL);
            return;
        }
        
        // 获取AudioProcessor实例的事件组 - 从application.h中直接获取
        auto& app = Application::GetInstance();
        // 这里使用Application中可用的方法来判断音频状态
        // 根据编译错误修改为可用的方法
        
        // 创建画布（如果不存在）
        if (!display->HasCanvas()) {
            display->CreateCanvas();
        }
        
        // 设置图片显示参数
        int imgWidth = 320;
        int imgHeight = 240;
        int x = 0;
        int y = 0;
        
        // 根据当前情感动态获取图片数组
        std::string current_emotion = board->GetCurrentEmotion();
        auto [imageArray, totalImages] = board->GetEmotionImageArray(current_emotion);
        
        ESP_LOGI(TAG, "当前情感: %s, 图片数量: %d", current_emotion.c_str(), totalImages);
        
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
        display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
        ESP_LOGI(TAG, "初始显示图片");
        
        // 持续监控和处理图片显示
        TickType_t lastUpdateTime = xTaskGetTickCount();
        TickType_t cycleInterval = pdMS_TO_TICKS(60); // 图片切换间隔，会根据情感动态调整
        
        // 定义用于判断是否正在播放音频的变量
        bool isAudioPlaying = false;
        
        // 定义用于检测情感变化的变量
        std::string lastEmotion = current_emotion;
        
        // 定义用于判断是否应该播放情感动画的变量
        bool shouldPlayAnimation = false;
        bool wasPlayingAnimation = false;
        
        // 定义自动回归neutral的超时机制（10秒无音频播放后自动回到neutral）
        TickType_t lastAudioTime = xTaskGetTickCount();
        const TickType_t neutralTimeout = pdMS_TO_TICKS(10000); // 10秒超时
        
        while (true) {
            // 检查情感是否发生变化
            std::string currentEmotion = board->GetCurrentEmotion();
            if (currentEmotion != lastEmotion) {
                ESP_LOGI(TAG, "情感变化检测: %s -> %s", lastEmotion.c_str(), currentEmotion.c_str());
                // 重新获取图片数组
                auto [newImageArray, newTotalImages] = board->GetEmotionImageArray(currentEmotion);
                imageArray = newImageArray;
                totalImages = newTotalImages;
                lastEmotion = currentEmotion;
                currentIndex = 0; // 重置到第一张图片
                
                // 根据新情感调整播放间隔
                int intervalMs = board->GetEmotionPlayInterval(currentEmotion);
                cycleInterval = pdMS_TO_TICKS(intervalMs);
                ESP_LOGI(TAG, "调整播放间隔为: %d毫秒", intervalMs);
                
                // 立即显示新情感的第一张图片
                currentImage = imageArray[currentIndex];
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                ESP_LOGI(TAG, "切换到新情感图片组: %s，图片数: %d", currentEmotion.c_str(), totalImages);
            }
            
            // 检查是否正在播放音频 - 使用应用程序状态判断
            isAudioPlaying = (app.GetDeviceState() == kDeviceStateSpeaking);
            
            // 更新最后一次音频播放时间
            if (isAudioPlaying) {
                lastAudioTime = xTaskGetTickCount();
            }
            
            // 检查是否需要自动回归neutral状态
            TickType_t timeSinceLastAudio = xTaskGetTickCount() - lastAudioTime;
            if (!isAudioPlaying && currentEmotion != "neutral" && timeSinceLastAudio > neutralTimeout) {
                ESP_LOGI(TAG, "长时间无音频播放，自动回归neutral状态");
                board->SetCurrentEmotion("neutral");
                // 注意：这里不直接修改currentEmotion，让下次循环检测情感变化时处理
            }
            
            // 判断是否应该播放情感动画：情绪不为neutral且正在说话
            bool isEmotionalState = (currentEmotion != "neutral") && (currentEmotion != "sleepy") && (currentEmotion != "");
            shouldPlayAnimation = isEmotionalState && isAudioPlaying;
            
            // 输出调试信息（每10次循环输出一次，避免日志过多）
            static int debugCount = 0;
            if (++debugCount >= 10) {
                ESP_LOGD(TAG, "状态检查 - 情绪: %s, 说话: %s, 播放动画: %s", 
                    currentEmotion.c_str(), 
                    isAudioPlaying ? "是" : "否",
                    shouldPlayAnimation ? "是" : "否");
                debugCount = 0;
            }
            
            TickType_t currentTime = xTaskGetTickCount();
            
            // 如果应该播放情感动画且时间到了切换间隔
            if (shouldPlayAnimation && (currentTime - lastUpdateTime >= cycleInterval)) {
                // 更新索引到下一张图片
                currentIndex = (currentIndex + 1) % totalImages;
                currentImage = imageArray[currentIndex];
                
                // 转换并显示新图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                ESP_LOGI(TAG, "播放情感动画: %s, 图片索引: %d", currentEmotion.c_str(), currentIndex);
                
                // 更新上次更新时间
                lastUpdateTime = currentTime;
            }
            // 如果不应该播放情感动画但之前在播放，或者当前不在第一张图片
            else if ((!shouldPlayAnimation && wasPlayingAnimation) || (!shouldPlayAnimation && currentIndex != 0)) {
                // 切换回第一张图片
                currentIndex = 0;
                currentImage = imageArray[currentIndex];
                
                // 转换并显示第一张图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                ESP_LOGI(TAG, "停止情感动画，显示初始图片 - 情绪: %s, 说话: %s", 
                    currentEmotion.c_str(), isAudioPlaying ? "是" : "否");
            }
            
            // 更新上一次动画播放状态
            wasPlayingAnimation = shouldPlayAnimation;
            
            // 短暂延时，避免CPU占用过高
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 释放资源（实际上不会执行到这里，除非任务被外部终止）
        delete[] convertedData;
        vTaskDelete(NULL);
    }

    void InitializeTouch()
    {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
            .int_gpio_num = GPIO_NUM_NC, 
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 1,
                .mirror_x = 1,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_config.scl_speed_hz = 400000;

        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp);
        assert(tp);

        /* Add touch input (for selected screen) */
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(), 
            .handle = tp,
        };

        lvgl_port_add_touch(&touch_cfg);
    }

    void InitializeCamera() {
        // Open camera power
        pca9557_->SetOutputState(2, 0);

        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2;  // LEDC通道选择  用于生成XCLK时钟 但是S3不用
        config.ledc_timer = LEDC_TIMER_2; // LEDC timer选择  用于生成XCLK时钟 但是S3不用
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = -1;   // 这里写-1 表示使用已经初始化的I2C接口
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 1;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        camera_ = new Esp32Camera(config);
    }
    
    void InitializeImu() {
        // 仅初始化IMU硬件
        imu_ = new Qmi8658(i2c_bus_);
        
        if (imu_->Initialize() == ESP_OK) {
            ESP_LOGI(TAG, "IMU initialized successfully");
        } else {
            ESP_LOGW(TAG, "Failed to initialize IMU");
            delete imu_;
            imu_ = nullptr;
        }
    }
    
    void InitializeInteractionSystem() {
        // 创建事件引擎
        event_engine_ = new EventEngine();
        event_engine_->Initialize();
        
        // 初始化运动引擎（如果IMU可用）
        if (imu_) {
            event_engine_->InitializeMotionEngine(imu_, true);  // 启用调试输出
        }
        
        // 初始化触摸引擎
        event_engine_->InitializeTouchEngine();
        
        // 事件处理策略已通过配置文件自动加载
        // 如需覆盖特定策略，可在此处调用：
        // event_engine_->ConfigureEventProcessing(EventType::TOUCH_TAP, custom_config);
        
        // 设置事件回调
        event_engine_->RegisterCallback([this](const Event& event) {
            HandleEvent(event);
        });
        
        // 创建定时器，每50ms处理一次事件
        esp_timer_create_args_t event_timer_args = {};
        event_timer_args.callback = [](void* arg) {
            auto* engine = static_cast<EventEngine*>(arg);
            engine->Process();
        };
        event_timer_args.arg = event_engine_;
        event_timer_args.dispatch_method = ESP_TIMER_TASK;
        event_timer_args.name = "event_timer";
        event_timer_args.skip_unhandled_events = true;
        
        esp_timer_create(&event_timer_args, &event_timer_);
        esp_timer_start_periodic(event_timer_, 50000);  // 50ms
        
        ESP_LOGI(TAG, "Interaction system initialized and started");
    }
    
    void HandleEvent(const Event& event) {
        // 在终端输出事件信息
        const char* event_name = "";
        const ImuData& data = event.data.imu_data;
        
        switch (event.type) {
            case EventType::MOTION_FREE_FALL:
                event_name = "FREE_FALL";
                ESP_LOGW(TAG, "⚠️ FREE FALL DETECTED! Accel magnitude: %.3f g", 
                        std::sqrt(data.accel_x * data.accel_x + 
                                data.accel_y * data.accel_y + 
                                data.accel_z * data.accel_z));
                break;
            case EventType::MOTION_SHAKE_VIOLENTLY:
                event_name = "SHAKE_VIOLENTLY";
                ESP_LOGW(TAG, "⚡ VIOLENT SHAKE! Accel: X=%.2f Y=%.2f Z=%.2f g", 
                        data.accel_x, data.accel_y, data.accel_z);
                break;
            case EventType::MOTION_FLIP: 
                event_name = "FLIP";
                ESP_LOGI(TAG, "🔄 Device flipped! (gyro: x=%.1f y=%.1f z=%.1f deg/s)", 
                        data.gyro_x, data.gyro_y, data.gyro_z);
                break;
            case EventType::MOTION_SHAKE: 
                event_name = "SHAKE";
                ESP_LOGI(TAG, "🔔 Device shaken!");
                break;
            case EventType::MOTION_PICKUP: 
                event_name = "PICKUP";
                ESP_LOGI(TAG, "📱 Device picked up!");
                break;
            case EventType::MOTION_UPSIDE_DOWN:
                event_name = "UPSIDE_DOWN";
                ESP_LOGI(TAG, "🙃 Device is upside down! (Z-axis: %.2f g)", data.accel_z);
                break;
            // 处理触摸事件
            case EventType::TOUCH_TAP:
                event_name = "TOUCH_TAP";
                // touch_data.x: -1表示左侧，1表示右侧
                // touch_data.y: 持续时间（毫秒）
                ESP_LOGI(TAG, "👆 Touch TAP on %s side! (duration: %d ms)", 
                        event.data.touch_data.x < 0 ? "LEFT" : "RIGHT",
                        event.data.touch_data.y);
                break;
            case EventType::TOUCH_DOUBLE_TAP:
                event_name = "TOUCH_DOUBLE_TAP";
                ESP_LOGI(TAG, "👆👆 Touch DOUBLE TAP on RIGHT side! (duration: %d ms)", 
                        event.data.touch_data.y);
                break;
            case EventType::TOUCH_LONG_PRESS:
                event_name = "TOUCH_LONG_PRESS";
                ESP_LOGI(TAG, "👇 Touch LONG PRESS on %s side! (duration: %d ms)", 
                        event.data.touch_data.x < 0 ? "LEFT" : "RIGHT",
                        event.data.touch_data.y);
                break;
            default: 
                return;
        }
        
        // 显示详细的IMU数据
        ESP_LOGD(TAG, "IMU Event [%s] - Accel(g): X=%.2f Y=%.2f Z=%.2f | Angles(°): X=%.1f Y=%.1f Z=%.1f",
                event_name,
                data.accel_x, data.accel_y, data.accel_z,
                data.angle_x, data.angle_y, data.angle_z);
    }

public:
    ALichuangTest() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeTouch();
        InitializeButtons();
        InitializeCamera();
        InitializeImu();  // 初始化IMU硬件
        InitializeInteractionSystem();  // 初始化交互系统

        GetBacklight()->RestoreBrightness();

        // 设置情感变化回调
        auto display = GetDisplay();
        if (display) {
            display->OnEmotionChanged([this](const std::string& emotion) {
                ESP_LOGI(TAG, "接收到情感变化回调: %s", emotion.c_str());
                SetCurrentEmotion(emotion);
            });
        }
        
        // 启动图片循环显示任务
        StartImageSlideshow();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static CustomAudioCodec audio_codec(
            i2c_bus_, 
            pca9557_);
        return &audio_codec;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual AnimaDisplay* GetDisplay() override {
        return display_;
    }

    virtual std::string GetBoardType() override {
        return "lingxi";
    }
    
    // 获取运动检测器（可选，用于外部访问）
    EventEngine* GetEventEngine() {
        return event_engine_;
    }
    
    // 获取IMU（可选，用于外部访问）
    Qmi8658* GetImu() {
        return imu_;
    }
};

DECLARE_BOARD(ALichuangTest);
