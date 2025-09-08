#include "wifi_board.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "codecs/box_audio_codec.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include "esp_lcd_sh8601.h"
#include "display/lcd_display.h"
#include "esp_io_expander_tca9554.h"
#include "mcp_server.h"
#include "lvgl.h"

#define TAG "waveshare_c6_amoled_1_43"

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = 
{
    {0x11, (uint8_t []){0x00}, 0, 80},   
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x53, (uint8_t []){0x20}, 1, 1},
    {0x63, (uint8_t []){0xFF}, 1, 1},
    {0x51, (uint8_t []){0x00}, 1, 1},
    {0x29, (uint8_t []){0x00}, 0, 10},
    {0x51, (uint8_t []){0xFF}, 1, 0},
};

class CustomLcdDisplay : public SpiLcdDisplay {
public:
    static void MyDrawEventCb(lv_event_t *e) {
        lv_area_t *area = (lv_area_t *)lv_event_get_param(e);   
        uint16_t x1 = area->x1;
        uint16_t x2 = area->x2; 
        uint16_t y1 = area->y1;
        uint16_t y2 = area->y2; 
        // round the start of coordinate down to the nearest 2M number
        area->x1 = (x1 >> 1) << 1;
        area->y1 = (y1 >> 1) << 1;
        // round the end of coordinate up to the nearest 2N+1 number
        area->x2 = ((x2 >> 1) << 1) + 1;
        area->y2 = ((y2 >> 1) << 1) + 1;
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
        DisplayLockGuard lock(this);
        lv_display_add_event_cb(display_, MyDrawEventCb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

class CustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button pwr_button_;
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_io_expander_handle_t io_expander = NULL;
    CustomLcdDisplay* display_;
    i2c_master_dev_handle_t disp_touch_dev_handle = NULL;
    lv_indev_t *touch_indev = NULL;    //touch
    uint8_t pwr_flag = 0;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
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
    }

    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS, &io_expander);
        if(ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_7 | IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_OUTPUT);
        ESP_ERROR_CHECK(ret);
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_7 | IO_EXPANDER_PIN_NUM_6, 1);
        ESP_ERROR_CHECK(ret);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {            
            .data0_io_num = LCD_D0,             
            .data1_io_num = LCD_D1, 
            .sclk_io_num = LCD_PCLK,            
            .data2_io_num = LCD_D2,             
            .data3_io_num = LCD_D3,             
            .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t),
        };
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        const esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = LCD_CS,          
            .dc_gpio_num = -1,          
            .spi_mode = 0,              
            .pclk_hz = 40 * 1000 * 1000,
            .trans_queue_depth = 4,     
            .on_color_trans_done = NULL,  
            .user_ctx = NULL,         
            .lcd_cmd_bits = 32,         
            .lcd_param_bits = 8,        
            .flags = {                  
                .quad_mode = true,      
            },                          
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle));
        sh8601_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,             // Uncomment these line if use custom initialization commands
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]), // sizeof(axs15231b_lcd_init_cmd_t),
            .flags = 
            {
                .use_qspi_interface = 1,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
            .bits_per_pixel = 16,                           // Implemented by LCD command `3Ah` (16/18)
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
        esp_lcd_panel_set_gap(panel_handle,0x06,0x00);
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        display_ = new CustomLcdDisplay(io_handle, panel_handle,
        EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() { //接入锂电池时,可长按PWR开机/关机
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
        });

        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });

        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        pwr_button_.OnLongPress([this]() {
            if(pwr_flag == 1)
            {
                pwr_flag = 0;
                esp_err_t ret;
                ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_6, 0);
                ESP_ERROR_CHECK(ret);
            }
        });

        pwr_button_.OnPressUp([this]() {
            if(pwr_flag == 0)
            {
                pwr_flag = 1;
            }
        });
    }

    void InitializeTouch() {
        i2c_device_config_t dev_cfg = 
        {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = I2C_Touch_ADDRESS,
            .scl_speed_hz = 300000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &disp_touch_dev_handle));

        touch_indev = lv_indev_create();
        lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch_indev, TouchInputReadCallback);
        lv_indev_set_user_data(touch_indev, disp_touch_dev_handle);
    }

    static void TouchInputReadCallback(lv_indev_t * indev, lv_indev_data_t *indevData)
    {
        i2c_master_dev_handle_t i2c_dev = (i2c_master_dev_handle_t)lv_indev_get_user_data(indev);
        uint8_t cmd = 0x02;
        uint8_t buf[5] = {0};
        uint16_t tp_x,tp_y;
        i2c_master_transmit_receive(i2c_dev,&cmd,1,buf,5,1000);
        if(buf[0])
        {
            tp_x = (((uint16_t)buf[1] & 0x0f)<<8) | (uint16_t)buf[2];
            tp_y = (((uint16_t)buf[3] & 0x0f)<<8) | (uint16_t)buf[4];
            if(tp_x > EXAMPLE_LCD_H_RES)
            {tp_x = EXAMPLE_LCD_H_RES;}
            if(tp_y > EXAMPLE_LCD_V_RES)
            {tp_y = EXAMPLE_LCD_V_RES;}
            indevData->point.x = tp_x;
            indevData->point.y = tp_y;
            //ESP_LOGI("tp","(%ld,%ld)",indevData->point.x,indevData->point.y);
            indevData->state = LV_INDEV_STATE_PRESSED;
        }
        else
        {
            indevData->state = LV_INDEV_STATE_RELEASED;
        }
    }

    void InitializeTools()
    {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.disp.setbacklight", "设置屏幕亮度", PropertyList({
            Property("level", kPropertyTypeInteger, 0, 255)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int level = properties["level"].value<int>();
            ESP_LOGI("setbacklight","%d",level);
            SetDispbacklight(level);
            return true;
        });
    }

    void SetDispbacklight(uint8_t backlight) {
        uint32_t lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= 0x02 << 24;
        uint8_t param = backlight;
        esp_lcd_panel_io_tx_param(io_handle, lcd_cmd, &param,1);
    }

public:
    CustomBoard() :
        boot_button_(BOOT_BUTTON_GPIO),pwr_button_(PWR_BUTTON_GPIO) {
        InitializeI2c();
        InitializeTca9554();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
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
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

};

DECLARE_BOARD(CustomBoard);