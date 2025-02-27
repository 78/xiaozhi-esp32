#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_ft5x06.h"
#include "font_awesome_symbols.h"
#include "audio_codecs/no_audio_codec.h"
#include <esp_sleep.h>
#include <vector>
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include <sstream>
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
#include "hna_16mm65t.h"
#include "ford_vfd.h"
#include "spectrumdisplay.h"

#define TAG "DualScreenAIDisplay"

LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_puhui_14_1);

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

class CustomLcdDisplay : public LcdDisplay, public Led,
#if FORD_VFD_EN
                         public FORD_VFD
#else
                         public HNA_16MM65T
#endif
{
private:
    uint8_t brightness_ = 0;
    lv_obj_t *time_label_ = nullptr;
    lv_style_t style_user;
    lv_style_t style_assistant;
    std::vector<lv_obj_t *> labelContainer; // 存储 label 指针的容器
    lv_anim_t anim[3];

#if FORD_VFD_EN
    lv_display_t *subdisplay;
    lv_obj_t *sub_status_label_;
#endif

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
                     esp_lcd_touch_handle_t tp_handle,
                     gpio_num_t backlight_pin,
                     bool backlight_output_invert,
                     int width,
                     int height,
                     int offset_x,
                     int offset_y,
                     bool mirror_x,
                     bool mirror_y,
                     bool swap_xy,
                     spi_device_handle_t spidevice = nullptr)
        : LcdDisplay(io_handle, panel_handle, tp_handle, backlight_pin, backlight_output_invert,
                     width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                     {
                         .text_font = &font_puhui_16_4,
                         .icon_font = &font_awesome_16_4,
                         .emoji_font = font_emoji_32_init(),
                     }),
#if FORD_VFD_EN
          FORD_VFD(spidevice)
#else
          HNA_16MM65T(spidevice)
#endif
    {
        DisplayLockGuard lock(this);
        // 由于屏幕是带圆角的，所以状态栏需要增加左右内边距
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.1, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.1, 0);

        InitializeBacklight();
        SetupUI();

#if FORD_VFD_EN
        InitializeSubScreen();
        SetupSubUI();
#endif
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
        SetSubBacklight(brightness);
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
        lv_obj_set_style_radius(content_, 0, 0);
        lv_obj_set_size(content_, LV_HOR_RES, LV_VER_RES - (18 + 2));
        lv_obj_set_flex_grow(content_, 1);

        /* Content */
        lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_all(content_, 0, 0);
        lv_obj_set_style_border_width(content_, 1, 0);
        lv_obj_add_flag(content_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_scroll_dir(content_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_ACTIVE);

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
#if FORD_VFD_EN
        SetSubContent(content);
#endif
        // std::stringstream ss;
        // ss << "role: " << role << ", content: " << content << std::endl;
        // std::string logMessage = ss.str();
        // auto sdcard = Board::GetInstance().GetSdcard();
        // sdcard->Write("/sdcard/log.txt", logMessage.c_str());
        // ESP_LOGI(TAG, "%s", logMessage.c_str());

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
        // ESP_LOGI(TAG, "Label Width: %ld-%ld", lv_obj_get_width(label), (LV_HOR_RES - 2));
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
        lv_obj_update_layout(content_);
    }

#if FORD_VFD_EN
    void SetSubBacklight(uint8_t brightness)
    {
        setbrightness(brightness);
    }

    static void sub_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
    {
        auto display = Board::GetInstance().GetDisplay();
#if false
        uint16_t *buf16 = (uint16_t *)px_map;
        int32_t x, y;
        for (y = area->y1; y <= area->y2; y++)
        {
            for (x = area->x1; x <= area->x2; x++)
            {
                display->DrawPoint(x, y, *buf16);
                buf16++;
            }
        }
#else
        int32_t x, y;
        uint8_t byte_index = 0;
        uint8_t bit_index = 0;

        for (y = area->y1; y <= area->y2; y++)
        {
            for (x = area->x1; x <= area->x2; x++)
            {
                uint8_t color = (px_map[byte_index] >> (7 - bit_index)) & 0x01;

                display->DrawPoint(x, y, color);

                bit_index++;
                if (bit_index == 8)
                {
                    bit_index = 0;
                    byte_index++;
                }
            }
        }
#endif
        lv_display_flush_ready(disp);
    }

    void InitializeSubScreen()
    {
        // 创建显示器对象
        subdisplay = lv_display_create(FORD_WIDTH, FORD_HEIGHT);
        if (subdisplay == NULL)
        {
            ESP_LOGI(TAG, "Failed to create subdisplay");
            return;
        }
        lv_display_set_flush_cb(subdisplay, sub_disp_flush);
        lv_display_set_color_format(subdisplay, LV_COLOR_FORMAT_I1);
        static uint16_t buf1[FORD_WIDTH * FORD_HEIGHT / 8];
        static uint16_t buf2[FORD_WIDTH * FORD_HEIGHT / 8];
        lv_display_set_buffers(subdisplay, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_FULL);
        LV_LOG_INFO("Subscreen initialized successfully");
    }

    void SetSubContent(const std::string &content)
    {
        lv_label_set_text(sub_status_label_, content.c_str());
    }

    void SetupSubUI()
    {
        DisplayLockGuard lock(this);

        ESP_LOGI(TAG, "SetupSubUI");
        auto screen = lv_disp_get_scr_act(subdisplay);

        lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
        lv_obj_set_style_text_color(screen, lv_color_white(), 0);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        lv_obj_set_style_pad_all(screen, 0, 0);
        lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *sub_container_ = lv_obj_create(screen);
        lv_obj_set_style_bg_color(sub_container_, lv_color_black(), 0);
        lv_obj_set_width(sub_container_, FORD_WIDTH);
        lv_obj_set_flex_flow(sub_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_border_width(sub_container_, 0, 0);
        lv_obj_set_style_pad_all(sub_container_, 0, 0);

        lv_obj_t *sub_status_bar_ = lv_obj_create(sub_container_);
        lv_obj_set_style_bg_color(sub_status_bar_, lv_color_black(), 0);
        lv_obj_set_width(sub_status_bar_, FORD_WIDTH);
        lv_obj_set_style_radius(sub_status_bar_, 0, 0);

        lv_obj_set_flex_flow(sub_status_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_border_width(sub_status_bar_, 0, 0);
        lv_obj_set_style_pad_all(sub_status_bar_, 0, 0);

        sub_status_label_ = lv_label_create(sub_status_bar_);
        lv_obj_set_style_border_width(sub_status_label_, 0, 0);
        lv_obj_set_style_bg_color(sub_status_label_, lv_color_black(), 0);
        lv_obj_set_style_pad_all(sub_status_label_, 0, 0);
        lv_obj_set_width(sub_status_label_, FORD_WIDTH);
        lv_label_set_long_mode(sub_status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(sub_status_label_, "正在初始化");
        lv_obj_set_style_text_align(sub_status_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_scrollbar_mode(sub_status_label_, LV_SCROLLBAR_MODE_OFF);
    }

    virtual void DrawPoint(int x, int y, uint8_t dot) override
    {
        draw_point(x, y, dot);
    }

    virtual void OnStateChanged() override
    {
        auto &app = Application::GetInstance();
        auto device_state = app.GetDeviceState();
        symbolhelper(DAB, false);
        symbolhelper(BT, false);
        symbolhelper(TA, false);
        symbolhelper(CD0, false);
        symbolhelper(CD1, false);
        symbolhelper(CD2, false);
        symbolhelper(CD3, false);
        switch (device_state)
        {
        case kDeviceStateStarting:
            symbolhelper(DAB, true);
            break;
        case kDeviceStateWifiConfiguring:
            symbolhelper(BT, true);
            break;
        case kDeviceStateIdle:
            setmode(FORD_FFT);
            return;
        case kDeviceStateConnecting:
            symbolhelper(TA, true);
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected())
            {
                symbolhelper(CD0, true);
                symbolhelper(CD1, true);
            }
            else
            {
                symbolhelper(CD0, true);
            }
            break;
        case kDeviceStateSpeaking:
            symbolhelper(IPOD, true);
            break;
        case kDeviceStateUpgrading:
            symbolhelper(UDISK, true);
            break;
        default:
            setmode(FORD_CONTENT);
            ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
            return;
        }
        setmode(FORD_CONTENT);
    }
#else
    void SetSubBacklight(uint8_t brightness)
    {
        pt6324_setbrightness(brightness);
    }

    virtual void OnStateChanged() override
    {
        auto &app = Application::GetInstance();
        auto device_state = app.GetDeviceState();
        symbolhelper(GIGA, false);
        symbolhelper(MONO, false);
        symbolhelper(STEREO, false);
        symbolhelper(REC_1, false);
        symbolhelper(REC_2, false);
        symbolhelper(USB1, false);
        dotshelper(DOT_MATRIX_FILL);
        switch (device_state)
        {
        case kDeviceStateStarting:
            symbolhelper(GIGA, true);
            break;
        case kDeviceStateWifiConfiguring:
            symbolhelper(MONO, true);
            break;
        case kDeviceStateIdle:
            break;
        case kDeviceStateConnecting:
            symbolhelper(STEREO, true);
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected())
            {
                symbolhelper(REC_1, true);
                symbolhelper(REC_2, true);
            }
            else
            {
                symbolhelper(REC_2, true);
            }
            break;
        case kDeviceStateSpeaking:
            dotshelper(DOT_MATRIX_NEXT);
            break;
        case kDeviceStateUpgrading:
            symbolhelper(USB1, true);
            break;
        default:
            ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
            return;
        }
    }

    virtual void Notification(const std::string &content, int timeout = 2000) override
    {
        noti_show(0, (char *)content.c_str(), 10, HNA_UP2DOWN, timeout);
    }

    virtual void SpectrumShow(float *buf, int size) override
    {
        spectrum_show(buf, size);
    }
#endif
};

class DualScreenAIDisplay : public WifiBoard
{
private:
    Button touch_button_;
    CustomLcdDisplay *display_ = NULL;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t bat_adc_cali_handle, dimm_adc_cali_handle;
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
        xTaskCreate([](void *arg)
                    {
            sntp_set_time_sync_notification_cb([](struct timeval *t){
                if (settimeofday(t, NULL) == -1) {
                    ESP_LOGE(TAG, "Failed to set system time");
                    return;
                }
                struct tm tm_info;
                localtime_r(&t->tv_sec, &tm_info);
                char time_str[50];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);

                ESP_LOGD(TAG, "The net time is: %s", time_str);
                auto ret = Board::GetInstance().CalibrateTime(&tm_info);
                if(!ret)
                    ESP_LOGI(TAG, "Calibration Time Failed");
                    else
                    {
                        CustomLcdDisplay *display = (CustomLcdDisplay*)Board::GetInstance().GetDisplay();
                        if(display != nullptr)
                        display->Notification("SYNC TM OK", 1000);
                    }
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
        // boot_button_.OnClick([this]()
        //                      {
        //     auto& app = Application::GetInstance();
        //      if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
        //         ResetWifiConfiguration();
        //     }
        //     app.ToggleChatState(); });

        // boot_button_.OnLongPress([this]
        //                          {
        //     ESP_LOGI(TAG, "System Sleeped");
        //     ((CustomLcdDisplay *)GetDisplay())->Sleep();
        //     gpio_set_level(PIN_NUM_LCD_POWER, 0);
        //     // esp_sleep_enable_ext0_wakeup(TOUCH_BUTTON_GPIO, 0);
        //     i2c_bus_delete(&i2c_bus);
        //     esp_deep_sleep_start(); });

        touch_button_.OnPressDown([this]()
                                  { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]()
                                { Application::GetInstance().StopListening(); });
    }

    void InitializeDisplay()
    {
        // Initialize the SPI bus configuration structure
        spi_bus_config_t buscfg = {0};
        spi_device_handle_t spi_device = nullptr;

#if SUB_DISPLAY_EN
        // Log the initialization process
        ESP_LOGI(TAG, "Initialize VFD SPI bus");

        // Set the clock and data pins for the SPI bus
        buscfg.sclk_io_num = PIN_NUM_VFD_PCLK;
        buscfg.data0_io_num = PIN_NUM_VFD_DATA0;
#if FORD_VFD_EN

        // Set the maximum transfer size in bytes
        buscfg.max_transfer_sz = 1024;

        // Initialize the SPI device interface configuration structure
        spi_device_interface_config_t devcfg = {
            .mode = 0,                      // Set the SPI mode to 3
            .clock_speed_hz = 400000,       // Set the clock speed to 1MHz
            .spics_io_num = PIN_NUM_VFD_CS, // Set the chip select pin
            .flags = 0,
            .queue_size = 7,
        };
#else

        // Set the maximum transfer size in bytes
        buscfg.max_transfer_sz = 256;

        // Initialize the SPI device interface configuration structure
        spi_device_interface_config_t devcfg = {
            .mode = 3,                      // Set the SPI mode to 3
            .clock_speed_hz = 1000000,      // Set the clock speed to 1MHz
            .spics_io_num = PIN_NUM_VFD_CS, // Set the chip select pin
            .flags = SPI_DEVICE_BIT_LSBFIRST,
            .queue_size = 7,
        };
#endif

        // Initialize the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_initialize(VFD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        // Add the PT6324 device to the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_add_device(VFD_HOST, &devcfg, &spi_device));
#endif
        ESP_LOGI(TAG, "Initialize OLED SPI bus");
        buscfg.sclk_io_num = PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
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

        esp_lcd_touch_handle_t tp = nullptr;
#if USE_TOUCH
        ESP_LOGI(TAG, "Initialize I2C bus");
        i2c_master_bus_handle_t i2c_bus_;
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)TOUCH_MASTER_NUM,
            .sda_io_num = TOUCH_SDA_NUM,
            .scl_io_num = TOUCH_SCL_NUM,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();

        tp_io_config.scl_speed_hz = 400 * 1000;
        // Attach the TOUCH to the I2C bus
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2((i2c_master_bus_t *)i2c_bus_, &tp_io_config, &tp_io_handle));

        const esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_HEIGHT - 1,
            .y_max = DISPLAY_WIDTH - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = TOUCH_INT_NUM,
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

        ESP_LOGI(TAG, "Initialize touch controller");
        (esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp)); // The first initial will be failed
        (esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
#endif

        display_ = new CustomLcdDisplay(panel_io, panel, tp, GPIO_NUM_NC, false,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, spi_device);

        if (PIN_NUM_VFD_EN != GPIO_NUM_NC)
        {
            ESP_LOGI(TAG, "Enable amoled power");
            gpio_set_direction(PIN_NUM_LCD_POWER, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_NUM_LCD_POWER, 1);
        }

        if (PIN_NUM_VFD_EN != GPIO_NUM_NC)
        {
            ESP_LOGI(TAG, "Enable VFD power");
            gpio_set_direction(PIN_NUM_VFD_EN, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_NUM_VFD_EN, 1);
        }
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
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

        adc_oneshot_chan_cfg_t bat_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &bat_config));

        adc_oneshot_chan_cfg_t dimm_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, DIMM_ADC_CHANNEL, &dimm_config));

        adc_cali_curve_fitting_config_t bat_cali_config = {
            .unit_id = ADC_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&bat_cali_config, &bat_adc_cali_handle));

        adc_cali_curve_fitting_config_t dimm_cali_config = {
            .unit_id = ADC_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&dimm_cali_config, &dimm_adc_cali_handle));
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
    DualScreenAIDisplay() : touch_button_(TOUCH_BUTTON_GPIO)
    {
        if (PIN_NUM_POWER_EN != GPIO_NUM_NC)
        {
            gpio_set_direction(PIN_NUM_POWER_EN, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_NUM_POWER_EN, 1);
        }

        vTaskDelay(pdMS_TO_TICKS(120));
        InitializeAdc();
        InitializeI2c();
        InitializeDisplay();
        InitializeButtons();
        InitializeIot();
        GetWakeupCause();
    }

    virtual Led *GetLed() override
    {
        // if (display_ != nullptr)
        return display_;
        // else
        // {
        //     static SingleLed led(BUILTIN_LED_GPIO);
        //     return &led;
        // }
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
    //     static Sdcard sd_card(PIN_NUM_SD_CS, PIN_NUM_SD_MOSI, PIN_NUM_SD_CLK, PIN_NUM_SD_MISO, SD_HOST);
    //     return &sd_card;
    // }

#define VCHARGE 4050
#define V1 3800
#define V2 3500
#define V3 3300
#define V4 3100

    int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
    {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    int32_t constrain(int32_t value, int32_t min_value, int32_t max_value)
    {
        if (value < min_value)
        {
            return min_value;
        }
        else if (value > max_value)
        {
            return max_value;
        }
        return value;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging) override
    {
        static int last_level = 0;
        static bool last_charging = false;
        int bat_adc_value;
        int bat_v = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &bat_adc_value));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(bat_adc_cali_handle, bat_adc_value, &bat_v));
        bat_v *= 2;

        // ESP_LOGI(TAG, "adc_value bat: %d, v: %d", bat_adc_value, bat_v);
        if (bat_v >= VCHARGE)
        {
            level = last_level;
            charging = true;
        }
        else if (bat_v >= V1)
        {
            level = 100;
            charging = false;
        }
        else if (bat_v >= V2)
        {
            level = 75;
            charging = false;
        }
        else if (bat_v >= V3)
        {
            level = 50;
            charging = false;
        }
        else if (bat_v >= V4)
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
            // ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        return true;
    }

    virtual bool CalibrateTime(struct tm *tm_info) override
    {
        if (rx8900_write_time(rx8900, tm_info) == ESP_FAIL)
            return false;
        return true;
    }

    virtual bool DimmingUpdate() override
    {
        static uint8_t last_bl = 0;
        int dimm_adc_value;
        int dimm_v = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, DIMM_ADC_CHANNEL, &dimm_adc_value));
        uint8_t bl = constrain(100 - map(dimm_adc_value, 300, 2400, 0, 100), 0, 100);

        if (last_bl != bl)
        {
            last_bl = bl;
            display_->SetBacklight(bl);
        }
    }

    virtual bool TimeUpdate() override
    {
        static struct tm time_user;
        if (rx8900_read_time(rx8900, &time_user) == ESP_FAIL)
        {
            time_t now;
            time(&now);
            time_user = *localtime(&now);
        }

        char time_str[7];
#if FORD_VFD_EN
        strftime(time_str, sizeof(time_str), "%H%M", &time_user);
        display_->number_show(5, time_str, 4);
        display_->time_blink();
        strftime(time_str, sizeof(time_str), "%m%d", &time_user);
        display_->symbolhelper(POINT1, true);
        display_->number_show(1, time_str, 4, FORD_DOWN2UP);
#else
        strftime(time_str, sizeof(time_str), "%H%M%S", &time_user);
        display_->content_show(4, time_str, 6);
        display_->time_blink();
        const char *weekDays[7] = {
            "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        display_->content_show(0, (char *)weekDays[time_user.tm_wday % 7], 3, HNA_DOWN2UP);
#endif
        return true;
    }
};

DECLARE_BOARD(DualScreenAIDisplay);
