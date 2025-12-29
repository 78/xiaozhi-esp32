#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/lcd_display.h"
#include "button.h"
#include "config.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lvgl_port.h>

#include "cpp_bus_driver_library.h"
#include "hi8561_driver.h"
#include "rm69a10_driver.h"

#define TAG "LilygoTDisplayP4Board"

void TouchTask(void *arg);

#if defined CONFIG_SCREEN_TYPE_HI8561
class CustomBacklight : public Backlight {
private:
    std::unique_ptr<Cpp_Bus_Driver::Tool> tool_;

public:
    CustomBacklight(std::unique_ptr<Cpp_Bus_Driver::Tool> tool) : tool_(std::move(tool)) {}

    void SetBrightnessImpl(uint8_t brightness) override{
        tool_->set_pwm_duty(brightness);
    };
};
#elif defined CONFIG_SCREEN_TYPE_RM69A10
class CustomBacklight : public Backlight {
private:
    esp_lcd_panel_handle_t mipi_dpi_panel_;

public:
    CustomBacklight(esp_lcd_panel_handle_t panel) : mipi_dpi_panel_(panel) {}

    void SetBrightnessImpl(uint8_t brightness) override{
        set_rm69a10_brightness(mipi_dpi_panel_,static_cast<uint8_t>(static_cast<float>(brightness) * 2.55));
    };
};

#else
#error "unknown macro definition, please select the correct macro definition."
#endif

class LilygoTDisplayP4Board : public WifiBoard {
public:
    i2c_master_bus_handle_t audio_codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay *display_;
    esp_lcd_panel_handle_t mipi_dpi_panel_;
    esp_timer_handle_t touchpad_timer_;
    uint8_t brightness_;

    std::shared_ptr<Cpp_Bus_Driver::Hardware_Iic_1> xl9535_iic_bus_ = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9535_SDA, XL9535_SCL, I2C_NUM_1);
    std::unique_ptr<Cpp_Bus_Driver::Xl95x5> xl9535_ = std::make_unique<Cpp_Bus_Driver::Xl95x5>(xl9535_iic_bus_, XL9535_IIC_ADDRESS, DEFAULT_CPP_BUS_DRIVER_VALUE);

#if defined CONFIG_SCREEN_TYPE_HI8561
    std::shared_ptr<Cpp_Bus_Driver::Hardware_Iic_1> hi8561_t_iic_bus_ = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(HI8561_TOUCH_SDA, HI8561_TOUCH_SCL, I2C_NUM_1);
    std::unique_ptr<Cpp_Bus_Driver::Hi8561_Touch> hi8561_t_ = std::make_unique<Cpp_Bus_Driver::Hi8561_Touch>(hi8561_t_iic_bus_, HI8561_TOUCH_IIC_ADDRESS, DEFAULT_CPP_BUS_DRIVER_VALUE);

    std::unique_ptr<Cpp_Bus_Driver::Tool> hi8561_backlight_ = std::make_unique<Cpp_Bus_Driver::Tool>();
#elif defined CONFIG_SCREEN_TYPE_RM69A10
    std::shared_ptr<Cpp_Bus_Driver::Hardware_Iic_1> gt9895_iic_bus_ = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(GT9895_TOUCH_SDA, GT9895_TOUCH_SCL, I2C_NUM_1);
    std::unique_ptr<Cpp_Bus_Driver::Gt9895> gt9895_ = std::make_unique<Cpp_Bus_Driver::Gt9895>(gt9895_iic_bus_, GT9895_IIC_ADDRESS, GT9895_X_SCALE_FACTOR, GT9895_Y_SCALE_FACTOR,
                                                       DEFAULT_CPP_BUS_DRIVER_VALUE);
#else
#error "unknown macro definition, please select the correct macro definition."
#endif

    std::unique_ptr<Cpp_Bus_Driver::Tool> esp32p4_ = std::make_unique<Cpp_Bus_Driver::Tool>();

    bool Init_Ldo_Channel_Power(uint8_t chan_id, uint32_t voltage_mv){
        esp_ldo_channel_handle_t ldo_channel_handle = NULL;
        esp_ldo_channel_config_t ldo_channel_config ={
                .chan_id = static_cast<int>(chan_id),
                .voltage_mv = static_cast<int>(voltage_mv),
            };
        if (esp_ldo_acquire_channel(&ldo_channel_config, &ldo_channel_handle) != ESP_OK){
            printf("esp_ldo_acquire_channel %d fail\n", chan_id);
            return false;
        }

        return true;
    }

    void InitializeCodecI2c() {
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &audio_codec_i2c_bus_));
    }

    void InitializeXl9535() {
        xl9535_->begin(500000);
        xl9535_->pin_mode(XL9535_ESP32P4_VCCA_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        xl9535_->pin_mode(XL9535_5_0_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        xl9535_->pin_mode(XL9535_3_3_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        // 开关3.3v电压时候必须先将GPS断电
        xl9535_->pin_mode(XL9535_GPS_WAKE_UP, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        xl9535_->pin_write(XL9535_GPS_WAKE_UP, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        // 开关3.3v电压时候必须先将ESP32C6断电
        xl9535_->pin_mode(XL9535_ESP32C6_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        xl9535_->pin_write(XL9535_ESP32C6_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);

        xl9535_->pin_write(XL9535_ESP32P4_VCCA_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);

        xl9535_->pin_write(XL9535_5_0_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_5_0_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_5_0_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));

        xl9535_->pin_write(XL9535_3_3_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_3_3_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_3_3_V_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        vTaskDelay(pdMS_TO_TICKS(10));

        // ESP32C6复位
        xl9535_->pin_write(XL9535_ESP32C6_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
        xl9535_->pin_write(XL9535_ESP32C6_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        vTaskDelay(pdMS_TO_TICKS(100));
        xl9535_->pin_write(XL9535_ESP32C6_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    void InitializeLCD() {
        xl9535_->pin_mode(XL9535_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        xl9535_->pin_write(XL9535_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));

        Init_Ldo_Channel_Power(3, 1800);
        
        esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;

        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = SCREEN_DATA_LANE_NUM,
            .lane_bit_rate_mbps = SCREEN_LANE_BIT_RATE_MBPS,
        };
        esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

        ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
        // we use DBI interface to send LCD commands and parameters
        esp_lcd_dbi_io_config_t dbi_io_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits = 8,   // according to the LCD spec
            .lcd_param_bits = 8, // according to the LCD spec
        };
        esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_io_config, &mipi_dbi_io);

        esp_lcd_dpi_panel_config_t dpi_config = {
            .virtual_channel = 0,
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = SCREEN_MIPI_DSI_DPI_CLK_MHZ,
            .pixel_format = SCREEN_COLOR_RGB_PIXEL_FORMAT,
            .num_fbs = 0,
            .video_timing = {
                .h_size = SCREEN_WIDTH,
                .v_size = SCREEN_HEIGHT,
                .hsync_pulse_width = SCREEN_MIPI_DSI_HSYNC,
                .hsync_back_porch = SCREEN_MIPI_DSI_HBP,
                .hsync_front_porch = SCREEN_MIPI_DSI_HFP,
                .vsync_pulse_width = SCREEN_MIPI_DSI_VSYNC,
                .vsync_back_porch = SCREEN_MIPI_DSI_VBP,
                .vsync_front_porch = SCREEN_MIPI_DSI_VFP,
            },
            .flags = {
                .use_dma2d = true, // use DMA2D to copy draw buffer into frame buffer
            }
        };

#if defined CONFIG_SCREEN_TYPE_HI8561
        hi8561_vendor_config_t vendor_config = {
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
            },
        };
        esp_lcd_panel_dev_config_t dev_config = {
            .reset_gpio_num = -1,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = SCREEN_BITS_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        esp_lcd_new_panel_hi8561(mipi_dbi_io, &dev_config, &mipi_dpi_panel_);
    #elif defined CONFIG_SCREEN_TYPE_RM69A10
        rm69a10_vendor_config_t vendor_config = {
            .mipi_config = {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
            },
        };
        esp_lcd_panel_dev_config_t dev_config = {
            .reset_gpio_num = -1,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = SCREEN_BITS_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        esp_lcd_new_panel_rm69a10(mipi_dbi_io, &dev_config, &mipi_dpi_panel_);
#else
#error "unknown macro definition, please select the correct macro definition."
#endif

        esp_lcd_panel_init(mipi_dpi_panel_);

#if defined CONFIG_SCREEN_TYPE_HI8561
        hi8561_backlight_->create_pwm(HI8561_SCREEN_BL, ledc_channel_t::LEDC_CHANNEL_0, 2000);
#elif defined CONFIG_SCREEN_TYPE_RM69A10
#else
#error "unknown macro definition, please select the correct macro definition."
#endif


        display_ = new MipiLcdDisplay(mipi_dbi_io, mipi_dpi_panel_, SCREEN_WIDTH, SCREEN_HEIGHT,
                                       SCREEN_OFFSET_X, SCREEN_OFFSET_Y, SCREEN_MIRROR_X, SCREEN_MIRROR_Y, SCREEN_SWAP_XY);
    }

    void InitializeTouch(){
        xl9535_->pin_mode(XL9535_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        xl9535_->pin_write(XL9535_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9535_->pin_write(XL9535_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));

#if defined CONFIG_SCREEN_TYPE_HI8561
        hi8561_t_iic_bus_->set_bus_handle(xl9535_iic_bus_->get_bus_handle());
        hi8561_t_->begin();
#elif defined CONFIG_SCREEN_TYPE_RM69A10
        gt9895_iic_bus_->set_bus_handle(xl9535_iic_bus_->get_bus_handle());
        gt9895_->begin();
#else
#error "unknown macro definition, please select the correct macro definition."
#endif

        xTaskCreate(TouchTask, "tp", 2 * 1024, this, 5, NULL);
    }

    void AppToggleChatState(void){
        auto& app = Application::GetInstance();
        // During startup (before connected), pressing BOOT button enters Wi-Fi config mode without reboot
        if (app.GetDeviceState() == kDeviceStateStarting) {
            EnterWifiConfigMode();
            return;
        }
        app.ToggleChatState(); 
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
        AppToggleChatState();
        });
    }

    LilygoTDisplayP4Board() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeXl9535();
        InitializeLCD();
        InitializeTouch();
        InitializeButtons();
        GetBacklight()->SetBrightness(100);
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(audio_codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

#if defined CONFIG_SCREEN_TYPE_HI8561
    virtual Backlight* GetBacklight() override {
        static CustomBacklight backlight(std::move(hi8561_backlight_));
        return &backlight;
    }
#elif defined CONFIG_SCREEN_TYPE_RM69A10
    virtual Backlight* GetBacklight() override {
        static CustomBacklight backlight(std::move(mipi_dpi_panel_));
        return &backlight;
    }
#else
#error "unknown macro definition, please select the correct macro definition."
#endif

};

void TouchTask(void *arg) {
    LilygoTDisplayP4Board* board = (LilygoTDisplayP4Board*)arg;

    static bool touch_flag = false;
    static bool touch_lock_flag = true;
    static size_t first_touch_time = 0;
    static bool waiting_for_second_tap = false;

    while (1){
#if defined CONFIG_SCREEN_TYPE_HI8561
        if (board->hi8561_t_->get_finger_count() > 0){
            if(touch_flag == false){
                touch_lock_flag = false;
            }
            touch_flag = true;
        } else {
            touch_flag = false;
        }
#elif defined CONFIG_SCREEN_TYPE_RM69A10
        if (board->gt9895_->get_finger_count() > 0){
            if(touch_flag == false){
                touch_lock_flag = false;
            }
            touch_flag = true;
        } else {
            touch_flag = false;
        }
#else
#error "unknown macro definition, please select the correct macro definition."
#endif

        if(touch_lock_flag == false){
            size_t current_time = board->esp32p4_->get_system_time_ms();
            
            if (!waiting_for_second_tap) {
                // 第一次点击
                first_touch_time = current_time;
                waiting_for_second_tap = true;
                printf("first touch detected, waiting for second...\n");
            } else {
                // 第二次点击，检查时间间隔
                // 500ms内完成双击
                if ((current_time - first_touch_time) <= 500) {
                    printf("double touch trigger\n");

                    board->AppToggleChatState();
                    
                    // 重置状态
                    waiting_for_second_tap = false;
                    first_touch_time = 0;
                } else {
                    // 超时，重新开始
                    first_touch_time = current_time;
                    printf("first touch timeout, restart...\n");
                }
            }
            
            touch_lock_flag = true;
        }
        
        // 处理双击超时
        if (waiting_for_second_tap) {
            size_t current_time = board->esp32p4_->get_system_time_ms();

            if ((current_time - first_touch_time) > 500) {
                waiting_for_second_tap = false;
                first_touch_time = 0;
                printf("double touch timeout\n");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

DECLARE_BOARD(LilygoTDisplayP4Board);
