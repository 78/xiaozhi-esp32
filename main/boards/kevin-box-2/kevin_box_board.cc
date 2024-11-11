#include "boards/ml307_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/ssd1306_display.h"
#include "application.h"
#include "button.h"
#include "led.h"
#include "config.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include "boards/wifi_board.h"

static const char *TAG = "KevinBoxBoard";

class KevinBoxBoard : public Ml307Board
{
private:
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    i2c_master_bus_handle_t display_i2c_bus_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t axp2101_handle_;
    uint8_t _data_buffer[2];

    void MountStorage()
    {
        // Mount the storage partition
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/storage",
            .partition_label = "storage",
            .max_files = 5,
            .format_if_mount_failed = true,
        };
        esp_vfs_spiffs_register(&conf);
    }
    void MountSdcard()
    {
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
            .format_if_mount_failed = true,
#else
            .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
            .max_files = 5,
            .allocation_unit_size = 16 * 1024};
        sdmmc_card_t *card;
        const char mount_point[] = "/sdcard";
        ESP_LOGI(TAG, "Initializing SD card");
        ESP_LOGI(TAG, "Using SDMMC peripheral");
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = 40000000;

        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = 1;

        slot_config.clk = GPIO_NUM_11;
        slot_config.cmd = GPIO_NUM_10;
        slot_config.d0 = GPIO_NUM_12;
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        ESP_LOGI(TAG, "Mounting filesystem");
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

        if (ret != ESP_OK)
        {
            if (ret == ESP_FAIL)
            {
                ESP_LOGE(TAG, "Failed to mount filesystem. "
                              "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                              "Make sure SD card lines have pull-up resistors in place.",
                         esp_err_to_name(ret));
            }
            return;
        }
        ESP_LOGI(TAG, "Filesystem mounted");

        // Card has been initialized, print its properties
        sdmmc_card_print_info(stdout, card);
    }

    void Enable4GModule()
    {
        // Make GPIO15 HIGH to enable the 4G module
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 4),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&ml307_enable_config);
        gpio_set_level(GPIO_NUM_4, 1);
        // gpio_set_level(GPIO_NUM_18, 1);
    }

    void Axp2101_writrReg(uint8_t reg, uint8_t data)
    {
        _data_buffer[0] = reg;
        _data_buffer[1] = data;
        ESP_ERROR_CHECK(i2c_master_transmit(axp2101_handle_, _data_buffer, 2, 100));
    }
    void Axp2101_readReg(uint8_t reg, uint8_t readSize)
    {
        /* Store data into buffer */
        ESP_ERROR_CHECK(i2c_master_transmit_receive(axp2101_handle_, &reg, 1, _data_buffer, readSize, 100));
    }
    /* Charging status */
    inline bool isCharging()
    {
        /* PMU status 2 */
        Axp2101_readReg(0x01, 1);
        if ((_data_buffer[0] & 0b01100000) == 0b00100000)
        {
            return true;
        }
        return false;
    }

    inline bool isChargeDone()
    {
        /* PMU status 2 */
        Axp2101_readReg(0x01, 1);
        if ((_data_buffer[0] & 0b00000111) == 0b00000100)
        {
            return true;
        }
        return false;
    }

    /* Bettery status */
    inline uint8_t batteryLevel()
    {
        /* Battery percentage data */
        Axp2101_readReg(0xA4, 1);
        return _data_buffer[0];
    }

    /* Power control */
    inline void powerOff()
    {
        /* PMU common configuration */
        Axp2101_readReg(0x10, 1);
        /* Soft power off */
        Axp2101_writrReg(0x10, (_data_buffer[0] | 0b00000001));
    }

    void InitializeAxp2101()
    {
        i2c_device_config_t axp2101_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AXP2101_I2C_ADDR,
            .scl_speed_hz = 100000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(codec_i2c_bus_, &axp2101_cfg, &axp2101_handle_));
        assert(axp2101_handle_ != NULL);
        Axp2101_writrReg(0x93, 0x1c); // 配置aldo2输出为3.3v

        Axp2101_readReg(0x90, 1);                 // XPOWERS_AXP2101_LDO_ONOFF_CTRL0
        _data_buffer[0] = _data_buffer[0] | 0x02; // set bit 1 (ALDO2)
        Axp2101_writrReg(0x90, _data_buffer[0]);  // and power channels now enabled

        Axp2101_writrReg(0x64, 0x03); // CV charger voltage setting to 42V
        Axp2101_readReg(0x62, 1);
        ESP_LOGI(TAG, "axp2101 read 0x62 get: 0x%X", _data_buffer[0]);
        Axp2101_writrReg(0x61, 0x05); // set Main battery precharge current to 125mA
        Axp2101_writrReg(0x62, 0x10); // set Main battery charger current to 900mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
        Axp2101_writrReg(0x63, 0x15); // set Main battery term charge current to 125mA
        Axp2101_readReg(0x62, 1);
        ESP_LOGI(TAG, "axp2101 read 0x62 get: 0x%X", _data_buffer[0]);

        Axp2101_readReg(0x18, 1);
        ESP_LOGI(TAG, "axp2101 read 0x18 get: 0x%X", _data_buffer[0]);
        _data_buffer[0] = _data_buffer[0] & 0b11100000;
        _data_buffer[0] = _data_buffer[0] | 0b00001110;
        Axp2101_writrReg(0x18, _data_buffer[0]);
        Axp2101_readReg(0x18, 1);
        ESP_LOGI(TAG, "axp2101 read 0x18 get: 0x%X", _data_buffer[0]);

        Axp2101_writrReg(0x14, 0x00); // set minimum system voltage to 4.1V (default 4.7V), for poor USB cables
        Axp2101_writrReg(0x15, 0x00); // set input voltage limit to 3.88v, for poor USB cables
        Axp2101_writrReg(0x16, 0x05); // set input voltage limit to 3.88v, for poor USB cables

        Axp2101_writrReg(0x24, 0x01); // set Vsys for PWROFF threshold to 3.2V (default - 2.6V and kill battery)
        Axp2101_writrReg(0x50, 0x14); // set TS pin to EXTERNAL input (not temperature)
    }

    void InitializeDisplayI2c()
    {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeCodecI2c()
    {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
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
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             { Application::GetInstance().ToggleChatState(); });

        volume_up_button_.OnClick([this]()
                                  {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("Volume\n" + std::to_string(volume)); });

        volume_up_button_.OnLongPress([this]()
                                      {
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(100);
            GetDisplay()->ShowNotification("Volume\n100"); });

        volume_down_button_.OnClick([this]()
                                    {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("Volume\n" + std::to_string(volume)); });

        volume_down_button_.OnLongPress([this]()
                                        {
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(0);
            GetDisplay()->ShowNotification("Volume\n0"); });
    }

public:
    KevinBoxBoard() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
                      boot_button_(BOOT_BUTTON_GPIO),
                      volume_up_button_(VOLUME_UP_BUTTON_GPIO),
                      volume_down_button_(VOLUME_DOWN_BUTTON_GPIO)
    {
    }

    virtual void Initialize() override
    {
        ESP_LOGI(TAG, "Initializing KevinBoxBoard");
        InitializeDisplayI2c();
        InitializeCodecI2c();
        InitializeAxp2101();
        // InitializeADC();
        MountStorage();
        MountSdcard();
        Enable4GModule();

        // gpio_config_t charging_io = {
        //     .pin_bit_mask = (1ULL << 2),
        //     .mode = GPIO_MODE_INPUT,
        //     .pull_up_en = GPIO_PULLUP_ENABLE,
        //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
        //     .intr_type = GPIO_INTR_DISABLE,
        // };
        // gpio_config(&charging_io);

        InitializeButtons();
        // WifiBoard::Initialize();

        Ml307Board::Initialize();
    }

    virtual Led *GetBuiltinLed() override
    {
        static Led led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                         AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                         AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        return &display;
    }

    virtual bool GetBatteryVoltage(int &voltage, bool &charging) override
    {
        // ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(adc1_handle_, adc1_cali_handle_, ADC_CHANNEL_0, &voltage));
        voltage = batteryLevel();
        charging = isCharging();
        ESP_LOGI(TAG, "Battery voltage: %d, Charging: %d", voltage, charging);
        return true;
    }
};

DECLARE_BOARD(KevinBoxBoard);