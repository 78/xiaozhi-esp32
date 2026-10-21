#ifndef XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_CONFIG_H_
#define XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_CONFIG_H_

#include <driver/gpio.h>
#include <driver/i2c_types.h>

// Audio control bus (I2C1) and devices.
#define AUDIO_I2C_PORT I2C_NUM_1
#define AUDIO_I2C_SDA_PIN GPIO_NUM_20
#define AUDIO_I2C_SCL_PIN GPIO_NUM_21
#define PCA9535_I2C_ADDRESS 0x20
#define ES8311_I2C_ADDRESS 0x18
#define ES7210_I2C_ADDRESS 0x40
// ES7210 analog gain for the microphone slots, from the factory BSP
// (CODEC_DEFAULT_ADC_VOLUME = 37.5 dB). With the default 0 dB the mics are
// ~40 dB down and the wake word fires on noise while ASR hears nothing.
#define AUDIO_MIC_GAIN 37.5f
#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// ES8311 playback bus (I2S TX).
#define AUDIO_TX_MCLK_PIN GPIO_NUM_33
#define AUDIO_TX_BCLK_PIN GPIO_NUM_32
#define AUDIO_TX_WS_PIN GPIO_NUM_31
#define AUDIO_TX_DOUT_PIN GPIO_NUM_30

// ES7210 capture bus (I2S RX/TDM).
#define AUDIO_RX_MCLK_PIN GPIO_NUM_29
#define AUDIO_RX_BCLK_PIN GPIO_NUM_28
#define AUDIO_RX_LRCK_PIN GPIO_NUM_27
#define AUDIO_RX_DIN_PIN GPIO_NUM_26

// PCA9535 (address 0x20) output bits, from the Seeed D1001 BSP pin table.
// The expander drives power and panel control signals; see
// reterminal_d1001_expander.{h,cc} for the wrappers that own these bits.
// The values are pin bit masks, not pin numbers: esp_io_expander_set_level()
// and esp_io_expander_set_dir() both take a mask.
#define EXPANDER_BIT_LCD_PWR_EN (1ULL << 0)        // EXP_PIN_NUM_0: display power enable
#define EXPANDER_BIT_LCD_RST (1ULL << 2)           // EXP_PIN_NUM_2: LCD reset
#define EXPANDER_BIT_LCD_BACKLIGHT_EN (1ULL << 7)  // EXP_PIN_NUM_7: backlight power enable
#define EXPANDER_BIT_PWR_HOLD (1ULL << 8)          // EXP_PIN_NUM_8: system power hold (vdd_3v3)
#define EXPANDER_BIT_PA_ENABLE (1ULL << 11)        // EXP_PIN_NUM_11: NS4150B power amp enable
#define EXPANDER_BIT_BAT_READ_EN (1ULL << 6)       // EXP_PIN_NUM_6: battery voltage divider gate
#define EXPANDER_BIT_BAT_CHG_EN (1ULL << 10)       // EXP_PIN_NUM_10: charger enable, ACTIVE LOW (CHG_ENBn)
#define EXPANDER_BIT_LTE_PWR_EN (1ULL << 15)      // EXP_PIN_NUM_15: mPCIe/LTE power
#define EXPANDER_BIT_TOUCH_RST (1ULL << 12)        // EXP_PIN_NUM_12: GSL3670 touch reset
#define EXPANDER_BIT_CAM_EN (1ULL << 1)            // EXP_PIN_NUM_1: camera rail enable
#define EXPANDER_BIT_CAM_PWDN (1ULL << 3)          // EXP_PIN_NUM_3: camera power down
#define EXPANDER_BIT_CAM_RST (1ULL << 9)           // EXP_PIN_NUM_9: camera reset

// GSL3670 touch controller bus (I2C0) and interrupt.
#define TOUCH_I2C_PORT I2C_NUM_0
#define TOUCH_I2C_SDA_PIN GPIO_NUM_37
#define TOUCH_I2C_SCL_PIN GPIO_NUM_38
#define TOUCH_INTERRUPT_PIN GPIO_NUM_16
#define TOUCH_I2C_ADDRESS 0x40
#define TOUCH_I2C_FREQ_HZ 400000

// Onboard MIPI CSI-2 camera module. The SC2356 sensor answers on the same
// I2C0 bus as the touch controller, so the camera reuses that bus handle
// instead of letting esp_video create a second master on the same pins.
#define CAMERA_SCCB_ADDRESS 0x36
#define CAMERA_SCCB_FREQ_HZ TOUCH_I2C_FREQ_HZ
// The Seeed BSP polls the controller and leaves GPIO16 unused.
#define TOUCH_SWAP_XY 0
#define TOUCH_MIRROR_X 1
#define TOUCH_MIRROR_Y 1

#define BOOT_BUTTON_GPIO GPIO_NUM_3

// JD9365DA-H3 panel and MIPI-DSI PHY facts.
#define DISPLAY_WIDTH 800
#define DISPLAY_HEIGHT 1280
#define DISPLAY_MIPI_DSI_LANE_NUM 2
#define DISPLAY_MIPI_DSI_LANE_BITRATE_MBPS 1000
#define DISPLAY_MIPI_DSI_PHY_LDO_CHANNEL 3
#define DISPLAY_MIPI_DSI_PHY_LDO_VOLTAGE_MV 2500

// DPI timings, from JD9365_8_800_1280_PANEL_60HZ_DPI_CONFIG in the Seeed BSP.
#define DISPLAY_DPI_CLOCK_MHZ 60
#define DISPLAY_HSYNC_PULSE_WIDTH 20
#define DISPLAY_HSYNC_BACK_PORCH 20
#define DISPLAY_HSYNC_FRONT_PORCH 40
#define DISPLAY_VSYNC_PULSE_WIDTH 4
#define DISPLAY_VSYNC_BACK_PORCH 30
#define DISPLAY_VSYNC_FRONT_PORCH 30

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

// Backlight PWM, driven directly by the P4 (the expander only gates the rail).
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_14
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// ESP32-C6 ESP-Hosted SDIO connection.
#define ESP32C6_SDIO_CMD_PIN GPIO_NUM_6
#define ESP32C6_SDIO_CLK_PIN GPIO_NUM_11
#define ESP32C6_SDIO_D0_PIN GPIO_NUM_7
#define ESP32C6_SDIO_D1_PIN GPIO_NUM_8
#define ESP32C6_SDIO_D2_PIN GPIO_NUM_9
#define ESP32C6_SDIO_D3_PIN GPIO_NUM_10
#define ESP32C6_RESET_PIN GPIO_NUM_13

#endif  // XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_CONFIG_H_