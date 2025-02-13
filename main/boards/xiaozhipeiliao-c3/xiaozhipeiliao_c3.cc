#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <ssid_manager.h>
#include "settings.h"
#include "esp_sleep.h"
#include "font_awesome_symbols.h"
#include <string.h>  // 添加字符串处理头文件
#include "xiaozipeiliao_display.h"

#if CONFIG_LCD_CONTROLLER_ILI9341
    #include "esp_lcd_ili9341.h"
    #include <driver/spi_common.h>
    #include "display/lcd_display.h"
    LV_FONT_DECLARE(font_puhui_16_4);
    LV_FONT_DECLARE(font_awesome_16_4);
#elif CONFIG_LCD_CONTROLLER_ST7789
    #include <esp_lcd_panel_vendor.h>
    #include <driver/spi_common.h>
    #include "display/lcd_display.h"
    LV_FONT_DECLARE(font_puhui_16_4);
    LV_FONT_DECLARE(font_awesome_16_4);
#endif

#define TAG "XiaoZhiPeiliaoC3"

RTC_DATA_ATTR int bootCNT;
esp_lcd_panel_handle_t panel = nullptr;
static QueueHandle_t gpio_evt_queue = NULL;
uint16_t battCnt;//闪灯次数
uint16_t battLife; //电量

// 中断服务程序
static void IRAM_ATTR batt_mon_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// 添加任务处理函数
static void batt_mon_task(void* arg) {
    uint32_t io_num;
    while(1) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            battCnt++;
        }
    }
}

static void calBattLife(TimerHandle_t xTimer) {
    // 计算电量
    battLife = 5 * battCnt;
    if (battLife >= 1 && battLife <= 80){
        battLife -= 1;
    }
    // 判断IO脚状态
    if (battCnt == 0 && gpio_get_level(PIN_BATT_MON) == 1) {
        battLife = 100;
    }
    if (battLife > 100){
        battLife = 100;
    }
    // ESP_LOGI(TAG, "Battery num %d\n", (int)battCnt);
    // ESP_LOGI(TAG, "Battery  life %d\n", (int)battLife);
    // 重置计数器
    battCnt = 0;
}


class XiaoZhiPeiliaoC3 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    TimerHandle_t batt_ticker_;
#if defined(CONFIG_LCD_CONTROLLER_ILI9341) || defined(CONFIG_LCD_CONTROLLER_ST7789)
    XiaozipeiliaoDisplay* display_;

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_PIN_MOSI;
        buscfg.miso_io_num = DISPLAY_SPI_PIN_MISO;
        buscfg.sclk_io_num = DISPLAY_SPI_PIN_SCLK;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeBattMon() {
        // 电池电量监测引脚配置
        gpio_config_t io_conf_batt_mon = {
            .pin_bit_mask = 1<<PIN_BATT_MON,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_POSEDGE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf_batt_mon));
        // 创建电量GPIO事件队列
        gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
        // 安装电量GPIO ISR服务
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        // 添加中断处理
        ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BATT_MON, batt_mon_isr_handler, (void*)PIN_BATT_MON));
         // 创建监控任务
        xTaskCreate(&batt_mon_task, "batt_mon_task", 2048, NULL, 10, NULL);
    }

    void InitializeLCDDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
         // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_PIN_LCD_CS;
        io_config.dc_gpio_num = DISPLAY_SPI_PIN_LCD_DC;
#if CONFIG_LCD_CONTROLLER_ILI9341
        io_config.spi_mode = 0;
#elif CONFIG_LCD_CONTROLLER_ST7789
        io_config.spi_mode = 3;
#endif        
        io_config.pclk_hz = DISPLAY_SPI_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_LCD_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_PIN_LCD_RST;
        panel_config.bits_per_pixel = 16;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER_COLOR;
#if CONFIG_LCD_CONTROLLER_ILI9341
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif CONFIG_LCD_CONTROLLER_ST7789
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new XiaozipeiliaoDisplay(
            panel_io,       // 保持参数传递
            panel,          // 保持参数传递
            DISPLAY_BACKLIGHT_PIN, 
            DISPLAY_BACKLIGHT_OUTPUT_INVERT,
            DISPLAY_WIDTH, 
            DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, 
            DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, 
            DISPLAY_MIRROR_Y, 
            DISPLAY_SWAP_XY, 
            {                
                .text_font = &font_puhui_16_4,
                .icon_font = &font_awesome_16_4,
                .emoji_font = font_emoji_64_init(),
            });
        display_->SetupUI();
        display_->SetBacklight(60);
        display_->SetLogo("小智陪聊");
        display_->SetConfigPage(
                   "按键功能:\n"
                    "  长按通话\n"
                    "  双击切换页面\n"
                    "语音指令集:\n"
                    "  关机/休眠\n"
                    "  调整亮度\n"
                    "  调整音量\n"
                    "  重新配网",  // 左侧说明文本
                "扫码访问管理后台",    // 二维码上方说明文字
                "https://xiaozhi.me/"// 二维码实际内容
        );
        display_->SetChatMessage("user", "长按按键开始对话\n双击按键进入帮助");
    }
#endif    
    void Start5V(){
        gpio_set_level(BOOT_5V_GPIO, 1); // 输出高电平
    }
    void Shutdown5V(){
        gpio_set_level(BOOT_5V_GPIO, 0); // 输出低电平
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
        gpio_config_t io_conf_5v = {
            .pin_bit_mask = 1<<BOOT_5V_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf_5v));
    }

    void InitializeButtons() {
        // boot_button_.OnClick([this]() {
        //     ESP_LOGI(TAG, "Button OnClick");
        // });
        boot_button_.OnPressDown([this]() {
            if (!wifi_config_mode_) {
                Application::GetInstance().StartListening();
            }
        });
        boot_button_.OnPressUp([this]() {
            if (!wifi_config_mode_) {
                Application::GetInstance().StopListening();
            }
        });
        boot_button_.OnLongPress([this]() {
            // ESP_LOGI(TAG, "Button LongPress");
            if (wifi_config_mode_) {
                StopNetwork();
                vTaskDelay(pdMS_TO_TICKS(1000));
                Sleep();
            }
        });    
        boot_button_.OnDoubleClick([this]() {
            // ESP_LOGI(TAG, "Button OnDoubleClick");
            if (display_ && !wifi_config_mode_) {
                display_->lv_switch_page();
            }
        });  
        // boot_button_.OnThreeClick([this]() {
        //     ESP_LOGI(TAG, "Button OnThreeClick");
        // });  
        boot_button_.OnFourClick([this]() {
            ESP_LOGI(TAG, "Button OnFourClick");
            if (display_->getlvpage() == LcdDisplay::PageIndex::PAGE_CONFIG) {
                auto &ssid_manager = SsidManager::GetInstance();
                ssid_manager.Clear();
                ESP_LOGI(TAG, "WiFi configuration and SSID list cleared");
                ResetWifiConfiguration();
                return;
            }
#if defined(CONFIG_WIFI_FACTORY_SSID)
            if (wifi_config_mode_) {
                auto &ssid_manager = SsidManager::GetInstance();
                auto ssid_list = ssid_manager.GetSsidList();
                if (strlen(CONFIG_WIFI_FACTORY_SSID) > 0){
                    ssid_manager.Clear();
                    ssid_manager.AddSsid(CONFIG_WIFI_FACTORY_SSID, CONFIG_WIFI_FACTORY_PASSWORD);
                    Settings settings("wifi", true);
                    settings.SetInt("force_ap", 0);
                    esp_restart();
                }
            }
#endif
        });
    }
    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("LCDScreen"));
        thing_manager.AddThing(iot::CreateThing("BoardControl"));
    }

    void InitializeBattTimers() {
        batt_ticker_ = xTimerCreate("BattTicker", pdMS_TO_TICKS(12500), pdTRUE, (void*)0, calBattLife);
        if (batt_ticker_ != NULL) {
            xTimerStart(batt_ticker_, 0);
        }
    }


public:
    XiaoZhiPeiliaoC3() : boot_button_(BOOT_BUTTON_GPIO, false, 800) {  
        InitializeI2c();
        InitializeButtons();
        InitializeIot();
#if defined(CONFIG_LCD_CONTROLLER_ILI9341) || defined(CONFIG_LCD_CONTROLLER_ST7789)
        InitializeSpi();
        InitializeLCDDisplay();
#endif
        Start5V();
        //电量计算定时任务
        InitializeBattMon();
        InitializeBattTimers();
        ESP_LOGI(TAG, "Inited");
    }
#if defined(CONFIG_LCD_CONTROLLER_ILI9341) || defined(CONFIG_LCD_CONTROLLER_ST7789)
    virtual Display* GetDisplay() override {
        return display_;
    }
#endif    

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    bool GetBatteryLevel(int &level, bool& charging) override {
        level = battLife;
        charging = false;
        return true;
    }

    void Sleep() override {
        ESP_LOGI(TAG, "Entering deep sleep");
        Application::GetInstance().StopListening();
        if (auto* codec = GetAudioCodec()) {
            codec->EnableOutput(false);
            codec->EnableInput(false);
        }
        
        // 移除电量中断处理
        ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_BATT_MON));
        // 删除电量队列和任务
        if(gpio_evt_queue) {
            vQueueDelete(gpio_evt_queue);
            gpio_evt_queue = NULL;
        }
        display_->SetBacklight(0);
        Shutdown5V();
        if (panel) {
            esp_lcd_panel_reset(panel);
            esp_lcd_panel_disp_sleep(panel, true);
        }
        // 配置唤醒源
        gpio_deep_sleep_hold_dis();
        esp_deep_sleep_enable_gpio_wakeup(0b0010, ESP_GPIO_WAKEUP_GPIO_LOW);
        gpio_set_direction(GPIO_NUM_1, GPIO_MODE_INPUT);
        esp_deep_sleep_start();
    }
};

DECLARE_BOARD(XiaoZhiPeiliaoC3);
