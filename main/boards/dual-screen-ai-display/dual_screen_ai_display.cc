#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"
#include "font_awesome_symbols.h"
#include "audio_codecs/no_audio_codec.h"
#include <esp_sleep.h>
#include <vector>
#include <cmath>
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "encoder.h"
#include <sstream>
#include <string>
#include "led/single_led.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "bmp280.h"
#include <esp_wifi.h>
#include "rx8900.h"
#include "esp_sntp.h"
#include "settings.h"
#include "pt6324.h"
#include "driver/usb_serial_jtag.h"

#define TAG "DualScreenAIDisplay"

LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_puhui_16_4);

#define LCD_BIT_PER_PIXEL (16)

#define LCD_OPCODE_WRITE_CMD (0x02ULL)
#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    // {0x44, (uint8_t []){0x01, 0xD1}, 2, 0},
    // {0x35, (uint8_t []){0x00}, 1, 0},
    {0x36, (uint8_t[]){0xF0}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0}, // 16bits-RGB565
    {0x2A, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4, 0},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

class CustomLcdDisplay : public LcdDisplay
{
private:
    uint8_t brightness_ = 0;
    lv_obj_t *time_label_ = nullptr;
    lv_style_t style_user;
    lv_style_t style_assistant;
    std::vector<lv_obj_t *> labelContainer; // 存储 label 指针的容器
    lv_anim_t anim[3];

    void RemoveOldestLabel()
    {
        if (!labelContainer.empty())
        {
            lv_obj_t *oldestLabel = labelContainer.front();
            labelContainer.erase(labelContainer.begin()); // 从容器中移除最早的 label 指针

            lv_obj_t *label = lv_obj_get_child(oldestLabel, 0);
            lv_obj_del(label);
            lv_obj_del(oldestLabel); // 删除 lvgl 对象
        }
    }

public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle,
                     gpio_num_t backlight_pin,
                     bool backlight_output_invert,
                     int width,
                     int height,
                     int offset_x,
                     int offset_y,
                     bool mirror_x,
                     bool mirror_y,
                     bool swap_xy)
        : LcdDisplay(io_handle, panel_handle, backlight_pin, backlight_output_invert,
                     width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                     {
                         .text_font = &font_puhui_16_4,
                         .icon_font = &font_awesome_16_4,
                         .emoji_font = font_emoji_32_init(),
                     })
    {

        DisplayLockGuard lock(this);
        // 由于屏幕是带圆角的，所以状态栏需要增加左右内边距
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.1, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.1, 0);

        InitializeBacklight();
        SetupUI();
    }

    void InitializeBacklight()
    {
        Settings settings("display", false);
        brightness_ = settings.GetInt("bright", 80);
        SetBacklight(brightness_);
    }

    void Sleep()
    {
        ESP_LOGI(TAG, "LCD sleep");
        uint8_t data[1] = {1};
        int lcd_cmd = 0x10;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }

    void UpdateTime(struct tm *time)
    {
        char time_str[6];
        strftime(time_str, sizeof(time_str), "%H:%M", time);
        DisplayLockGuard lock(this);
        lv_label_set_text(time_label_, time_str);
    }

    static void set_width(void *var, int32_t v)
    {
        lv_obj_set_width((lv_obj_t *)var, v);
    }

    static void set_height(void *var, int32_t v)
    {
        lv_obj_set_height((lv_obj_t *)var, v);
    }

    virtual int GetBacklight() override { return brightness_; }

    virtual void SetBacklight(uint8_t brightness) override
    {
        brightness_ = brightness;
        if (brightness > 100)
        {
            brightness = 100;
        }
        Settings settings("display", true);
        settings.SetInt("bright", brightness_);

        ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness);
        // LEDC resolution set to 10bits, thus: 100% = 255
        uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }

    virtual void SetupUI() override
    {
        DisplayLockGuard lock(this);

        ESP_LOGI(TAG, "SetupUI");
        auto screen = lv_disp_get_scr_act(lv_disp_get_default());
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        lv_obj_set_style_text_font(screen, &font_puhui_16_4, 0);
        lv_obj_set_style_text_color(screen, lv_color_white(), 0);

        /* Container */
        container_ = lv_obj_create(screen);
        lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(container_, 0, 0);
        lv_obj_set_style_border_width(container_, 0, 0);
        lv_obj_set_style_pad_row(container_, 0, 0);

        /* Status bar */
        status_bar_ = lv_obj_create(container_);
        lv_obj_set_size(status_bar_, LV_HOR_RES, 18 + 2);
        lv_obj_set_style_radius(status_bar_, 0, 0);

        /* Status bar */
        lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(status_bar_, 0, 0);
        lv_obj_set_style_border_width(status_bar_, 0, 0);
        lv_obj_set_style_pad_column(status_bar_, 4, 0);

        /* Content */
        content_ = lv_obj_create(container_);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_ACTIVE);
        lv_obj_set_style_radius(content_, 0, 0);
        lv_obj_set_width(content_, LV_HOR_RES);
        lv_obj_set_flex_grow(content_, 1);

        /* Content */
        lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(content_, 0, 0);
        lv_obj_set_style_border_width(content_, 1, 0);

        network_label_ = lv_label_create(status_bar_);
        lv_label_set_text(network_label_, "");
        lv_obj_set_style_text_font(network_label_, &font_awesome_16_4, 0);

        time_label_ = lv_label_create(status_bar_);
        lv_label_set_text(time_label_, "");
        lv_obj_set_style_text_font(time_label_, &font_puhui_16_4, 0);

        notification_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(notification_label_, 1);
        lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(notification_label_, "通知");
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

        status_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(status_label_, 1);
        lv_label_set_text(status_label_, "正在初始化");
        lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

        emotion_label_ = lv_label_create(status_bar_);
        lv_obj_set_style_text_font(emotion_label_, &font_awesome_16_4, 0);
        lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
        lv_obj_center(emotion_label_);

        mute_label_ = lv_label_create(status_bar_);
        lv_label_set_text(mute_label_, "");
        lv_obj_set_style_text_font(mute_label_, &font_awesome_16_4, 0);

        battery_label_ = lv_label_create(status_bar_);

        lv_label_set_text(battery_label_, "");
        lv_obj_set_style_text_font(battery_label_, &font_awesome_16_4, 0);

        lv_style_init(&style_user);
        lv_style_set_radius(&style_user, 5);
        lv_style_set_bg_opa(&style_user, LV_OPA_COVER);
        lv_style_set_border_width(&style_user, 2);
        lv_style_set_border_color(&style_user, lv_color_hex(0));
        lv_style_set_pad_all(&style_user, 10);

        lv_style_set_text_color(&style_user, lv_color_hex(0xffffff));
        lv_style_set_bg_color(&style_user, lv_color_hex(0x00B050));

        lv_style_init(&style_assistant);
        lv_style_set_radius(&style_assistant, 5);
        lv_style_set_bg_opa(&style_assistant, LV_OPA_COVER);
        lv_style_set_border_width(&style_assistant, 2);
        lv_style_set_border_color(&style_assistant, lv_color_hex(0));
        lv_style_set_pad_all(&style_assistant, 10);

        lv_style_set_text_color(&style_assistant, lv_color_hex(0));
        lv_style_set_bg_color(&style_assistant, lv_color_hex(0xE0E0E0));
    }

    virtual void SetChatMessage(const std::string &role, const std::string &content) override
    {
        if (role == "")
            return;
        std::stringstream ss;
        ss << "role: " << role << ", content: " << content << std::endl;
        std::string logMessage = ss.str();
        // auto sdcard = Board::GetInstance().GetSdcard();
        // sdcard->Write("/sdcard/log.txt", logMessage.c_str());
        ESP_LOGI(TAG, "%s", logMessage.c_str());

        DisplayLockGuard lock(this);
        if (labelContainer.size() >= 10)
        {
            RemoveOldestLabel(); // 当 label 数量达到 10 时移除最早的
        }
        lv_obj_t *container = lv_obj_create(content_);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_radius(container, 0, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_width(container, LV_HOR_RES - 2);
        lv_obj_set_style_pad_all(container, 0, 0);

        lv_obj_t *label = lv_label_create(container);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

        if (role == "user")
        {
            lv_obj_add_style(label, &style_user, 0);
            lv_obj_align(label, LV_ALIGN_RIGHT_MID, 0, 0);
        }
        else
        {
            lv_obj_add_style(label, &style_assistant, 0);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
        }
        lv_obj_set_style_text_font(label, &font_puhui_16_4, 0);
        lv_label_set_text(label, content.c_str());
        // lv_obj_center(label);

        lv_obj_set_style_pad_all(label, 5, LV_PART_MAIN);

        lv_obj_update_layout(label);
        ESP_LOGI(TAG, "Label Width: %ld-%ld", lv_obj_get_width(label), (LV_HOR_RES - 2));
        if (lv_obj_get_width(label) >= (LV_HOR_RES - 2))
            lv_obj_set_width(label, (LV_HOR_RES - 2));
        lv_obj_scroll_to_view(container, LV_ANIM_ON);

        for (size_t i = 0; i < 2; i++)
        {
            lv_anim_init(&anim[i]);
            lv_anim_set_var(&anim[i], label);
            lv_anim_set_early_apply(&anim[i], false);
            lv_anim_set_path_cb(&anim[i], lv_anim_path_overshoot);
            lv_anim_set_time(&anim[i], 300);
            lv_anim_set_delay(&anim[i], 200);
        }
        lv_anim_set_values(&anim[0], 0, lv_obj_get_width(label));
        lv_anim_set_exec_cb(&anim[0], (lv_anim_exec_xcb_t)set_width);
        lv_anim_start(&anim[0]);

        lv_anim_set_values(&anim[1], 0, lv_obj_get_height(label));
        lv_anim_set_exec_cb(&anim[1], (lv_anim_exec_xcb_t)set_height);
        lv_anim_start(&anim[1]);

        lv_obj_set_width(label, 0);
        lv_obj_set_height(label, 0);

        lv_anim_init(&anim[2]);
        lv_anim_set_var(&anim[2], container);
        lv_anim_set_early_apply(&anim[2], true);
        lv_anim_set_path_cb(&anim[2], lv_anim_path_overshoot);
        lv_anim_set_time(&anim[2], 200);
        lv_anim_set_values(&anim[2], 0, lv_obj_get_height(label));
        lv_anim_set_exec_cb(&anim[2], (lv_anim_exec_xcb_t)set_height);
        lv_anim_start(&anim[2]);

        labelContainer.push_back(container);
    }
};

class VFDDisplay : public PT6324Writer
{
#define BUF_SIZE (1024)
private:
    int last_values[12] = {0};
    int target_values[12] = {0};
    int current_values[12] = {0};
    int animation_steps[12] = {0};
    int total_steps = 20; // 动画总步数

    void animate()
    {
        for (int i = 0; i < 12; i++)
        {
            if (animation_steps[i] < total_steps)
            {
                // 使用指数衰减函数计算当前值
                float progress = static_cast<float>(animation_steps[i]) / total_steps;
                float factor = 1 - std::exp(-3 * progress); // 指数衰减因子
                current_values[i] = last_values[i] + static_cast<int>((target_values[i] - last_values[i]) * factor);
                pt6324_wavehelper(i, current_values[i] * 8 / 90);
                animation_steps[i]++;
            }
            else
            {
                last_values[i] = target_values[i];
                pt6324_wavehelper(i, target_values[i] * 8 / 90);
            }
        }
    }

public:
    VFDDisplay(spi_device_handle_t spi_device) : PT6324Writer(spi_device)
    {
        pt6324_init();
        xTaskCreate(
            [](void *arg)
            {
                VFDDisplay *vfd = static_cast<VFDDisplay *>(arg);
                while(true)
                {
                    vfd->pt6324_refrash();
                    vfd->animate();
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            vTaskDelete(NULL); }, "vfd", 4096, this, 4, nullptr);
    }

    void SpectrumPresent(uint8_t *buf) // 0-100
    {
        for (size_t i = 0; i < 12; i++)
        {
            last_values[i] = target_values[i];
            target_values[i] = buf[i];
            animation_steps[i] = 0;
        }
    }

    void test()
    {
        xTaskCreate(
            [](void *arg)
            {
                VFDDisplay *vfd = static_cast<VFDDisplay *>(arg);
                // Configure USB SERIAL JTAG
                usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
                    .tx_buffer_size = BUF_SIZE,
                    .rx_buffer_size = BUF_SIZE,
                };
                uint8_t testbuff[12];
                ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
                uint8_t *recv_data = (uint8_t *)malloc(BUF_SIZE);
                while (1)
                {
                    memset(recv_data, 0, BUF_SIZE);
                    int len = usb_serial_jtag_read_bytes(recv_data, BUF_SIZE - 1, 0x20 / portTICK_PERIOD_MS);
                    if (len > 0)
                    {
                        vfd->pt6324_dotshelper((Dots)((recv_data[0] - '0') % 4));
                        for (int i = 0; i < 10; i++)
                            vfd->pt6324_numhelper(i, recv_data[0]);
                        for (int i = 0; i < 12; i++)
                            testbuff[i] = (recv_data[0] - '0') * 10;
                        vfd->SpectrumPresent(testbuff);
                        // int index = 0, data = 0;
        
                        // sscanf((char *)recv_data, "%d:%X", &index, &data);
                        // printf("Parsed numbers: %d and 0x%02X\n", index, data);
                        // gram[index] = data;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            vTaskDelete(NULL); }, "vfd1", 4096, this, 4, nullptr);
    }
};

static rx8900_handle_t _rx8900 = NULL;
class DualScreenAIDisplay : public WifiBoard
{
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button touch_button_;
    Encoder volume_encoder_;
    // SystemReset system_reset_;
    CustomLcdDisplay *display_;
    VFDDisplay *vfd;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
    i2c_bus_handle_t i2c_bus = NULL;
    bmp280_handle_t bmp280 = NULL;
    rx8900_handle_t rx8900 = NULL;

    void InitializeI2c()
    {
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = IIC_SDA_NUM,
            .scl_io_num = IIC_SCL_NUM,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {0},
            .clk_flags = 0,
        };
        conf.master.clk_speed = 400000,
        i2c_bus = i2c_bus_create(IIC_MASTER_NUM, &conf);
        bmp280 = bmp280_create(i2c_bus, BMP280_I2C_ADDRESS_DEFAULT);
        ESP_LOGI(TAG, "bmp280_default_init:%d", bmp280_default_init(bmp280));
        rx8900 = rx8900_create(i2c_bus, RX8900_I2C_ADDRESS_DEFAULT);
        ESP_LOGI(TAG, "rx8900_default_init:%d", rx8900_default_init(rx8900));
        _rx8900 = rx8900;
        xTaskCreate([](void *arg)
                    {
            sntp_set_time_sync_notification_cb([](struct timeval *t){
                struct tm tm_info;
                localtime_r(&t->tv_sec, &tm_info);
                char time_str[50];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);

                ESP_LOGW(TAG, "The net time is: %s", time_str);
                rx8900_write_time(_rx8900, &tm_info);
            });
            esp_netif_init();
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, (char*)NTP_SERVER1);
            esp_sntp_setservername(1, (char*)NTP_SERVER2);
            esp_sntp_init();
            setenv("TZ", DEFAULT_TIMEZONE, 1);
            tzset();
        // configTzTime(DEFAULT_TIMEZONE, NTP_SERVER1, NTP_SERVER2);
        vTaskDelete(NULL); }, "timesync", 4096, NULL, 4, nullptr);
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();           
             if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });

        boot_button_.OnLongPress([this]
                                 {
            ESP_LOGI(TAG, "System Sleeped");
            ((CustomLcdDisplay *)GetDisplay())->Sleep();
            gpio_set_level(PIN_NUM_LCD_POWER, 0);
            // esp_sleep_enable_ext0_wakeup(TOUCH_BUTTON_GPIO, 0);
            i2c_bus_delete(&i2c_bus);
            esp_deep_sleep_start(); });

        touch_button_.OnPressDown([this]()
                                  { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]()
                                { Application::GetInstance().StopListening(); });
    }

    void InitializeEncoder()
    {
        volume_encoder_.OnPcntReach([this](int value)
                                    {
            static int lastvalue = 0;
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume();
            if(value>lastvalue)
            {
                volume += 4;
                if (volume > 100) {
                    volume = 100;
                }
            }
            else if(value<lastvalue)
            {
                volume -= 4;
                if (volume < 0) {
                    volume = 0;
                }
            }
            lastvalue = value;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("音量 " + std::to_string(volume)); });
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {0};
        ESP_LOGI(TAG, "Initialize VFD SPI bus");
        buscfg.sclk_io_num = PIN_NUM_VFD_PCLK;
        buscfg.data0_io_num = PIN_NUM_VFD_DATA0;
        buscfg.max_transfer_sz = 256;
        ESP_ERROR_CHECK(spi_bus_initialize(VFD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        spi_device_handle_t spidevice;
        spi_device_interface_config_t devcfg = {
            .mode = 3,                      // SPI mode 3
            .clock_speed_hz = 1000000,      // 1MHz
            .spics_io_num = PIN_NUM_VFD_CS, // CS pin
            .flags = SPI_DEVICE_BIT_LSBFIRST,
            .queue_size = 7,
        };
        ESP_ERROR_CHECK(spi_bus_add_device(VFD_HOST, &devcfg, &spidevice));
        vfd = new VFDDisplay(spidevice);
        vfd->test();

        ESP_LOGI(TAG, "Initialize OLED SPI bus");
        buscfg.sclk_io_num = PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSH8601Display()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Enable amoled power");
        gpio_set_direction(PIN_NUM_LCD_POWER, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_NUM_LCD_POWER, 1);
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
            PIN_NUM_LCD_CS,
            nullptr,
            nullptr);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }};

        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
            .bits_per_pixel = LCD_BIT_PER_PIXEL,
            .flags = {
                .reset_active_high = 0,
            },
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        esp_lcd_panel_invert_color(panel, false);
        // esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new CustomLcdDisplay(panel_io, panel, GPIO_NUM_NC, false,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Barometer"));
        thing_manager.AddThing(iot::CreateThing("Displayer"));
        // thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

    void InitializeAdc()
    {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc_handle));

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };

        // 创建并初始化校准句柄
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));

        // adc1_config_width(ADC_WIDTH_BIT_12);
        // adc1_config_channel_atten(BAT_DETECT_CH, ADC_ATTEN_DB_12);
        // esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);
    }
    void GetWakeupCause(void)
    {
        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
        switch (wakeup_cause)
        {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "Wakeup cause: Undefined");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wakeup cause: External source 0");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Wakeup cause: External source 1");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup cause: Timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            ESP_LOGI(TAG, "Wakeup cause: Touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            ESP_LOGI(TAG, "Wakeup cause: ULP");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            ESP_LOGI(TAG, "Wakeup cause: GPIO");
            break;
        case ESP_SLEEP_WAKEUP_UART:
            ESP_LOGI(TAG, "Wakeup cause: UART");
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            ESP_LOGI(TAG, "Wakeup cause: WiFi");
            break;
        case ESP_SLEEP_WAKEUP_COCPU:
            ESP_LOGI(TAG, "Wakeup cause: Co-processor");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup cause: Unknown");
            break;
        }
    }

public:
    DualScreenAIDisplay() : boot_button_(BOOT_BUTTON_GPIO),
                            touch_button_(TOUCH_BUTTON_GPIO),
                            volume_encoder_(VOLUME_ENCODER1_GPIO, VOLUME_ENCODER2_GPIO)
    // ,
    // system_reset_(RESET_NVS_BUTTON_GPIO, RESET_FACTORY_BUTTON_GPIO)
    {
        // Check if the reset button is pressed
        // system_reset_.CheckButtons();
        InitializeAdc();
        InitializeI2c();
        InitializeSpi();
        InitializeSH8601Display();
        InitializeButtons();
        InitializeEncoder();
        InitializeIot();
        GetWakeupCause();
    }

    virtual Led *GetLed() override
    {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual float GetBarometer() override
    {
        float pressure = 0.0f;
        if (ESP_OK == bmp280_read_pressure(bmp280, &pressure))
        {
            ESP_LOGI(TAG, "pressure:%f ", pressure);
            return pressure;
        }
        return 0;
    }

    virtual float GetTemperature() override
    {
        float temperature = 0.0f;
        if (ESP_OK == bmp280_read_temperature(bmp280, &temperature))
        {
            ESP_LOGI(TAG, "temperature:%f ", temperature);
            return temperature;
        }
        return 0;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        ***static NoAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                           AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    // virtual Sdcard *GetSdcard() override
    // {
    //     static Sdcard sd_card(PIN_NUM_SD_CMD, PIN_NUM_SD_CLK, PIN_NUM_SD_D0, PIN_NUM_SD_D1, PIN_NUM_SD_D2, PIN_NUM_SD_D3, PIN_NUM_SD_CDZ);
    //     return &sd_card;
    // }

#define VCHARGE 4050
#define V1 3800
#define V2 3500
#define V3 3300
#define V4 3100

    virtual bool GetBatteryLevel(int &level, bool &charging) override
    {
        static int last_level = 0;
        static bool last_charging = false;
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_value));
        int v1 = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_value, &v1));
        v1 *= 2;
        // ESP_LOGI(TAG, "adc_value: %d, v1: %d", adc_value, v1);
        if (v1 >= VCHARGE)
        {
            level = last_level;
            charging = true;
        }
        else if (v1 >= V1)
        {
            level = 100;
            charging = false;
        }
        else if (v1 >= V2)
        {
            level = 75;
            charging = false;
        }
        else if (v1 >= V3)
        {
            level = 50;
            charging = false;
        }
        else if (v1 >= V4)
        {
            level = 25;
            charging = false;
        }
        else
        {
            level = 0;
            charging = false;
        }

        if (level != last_level || charging != last_charging)
        {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        static struct tm time_user;
        rx8900_read_time(rx8900, &time_user);
        ((CustomLcdDisplay *)GetDisplay())->UpdateTime(&time_user);

        // char time_str[50];
        // strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_user);
        // ESP_LOGI(TAG, "The time is: %s", time_str);
        return true;
    }
};

DECLARE_BOARD(DualScreenAIDisplay);
