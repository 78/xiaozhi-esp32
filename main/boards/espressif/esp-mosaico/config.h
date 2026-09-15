#ifndef ESP_MOSAICO_CONFIG_H
#define ESP_MOSAICO_CONFIG_H

#include <driver/gpio.h>
#include <driver/spi_master.h>

// ESP-Mosaico has an ES8311 codec at 7-bit I2C address 0x19. esp_codec_dev
// uses the 8-bit address form for this device.
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_CODEC_ES8311_ADDR (0x19U << 1)

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_54
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_37
#define AUDIO_I2S_GPIO_WS GPIO_NUM_49
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_52
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_40
#define AUDIO_CODEC_PA_PIN GPIO_NUM_45

// The primary I2C bus is revision-dependent; the board class selects these
// after reading eFuse USER_DATA.
#define I2C_SDA_V1_0 GPIO_NUM_0
#define I2C_SCL_V1_0 GPIO_NUM_1
#define I2C_SDA_V1_2 GPIO_NUM_56
#define I2C_SCL_V1_2 GPIO_NUM_3

// BQ27220 fuel gauge on the primary, revision-dependent I2C bus.
#define BATTERY_BQ27220_I2C_ADDR 0x55
#define BATTERY_I2C_SPEED_HZ (400 * 1000)

// CO5300 480x480 QSPI display. v1.2 swaps SCLK and RESET.
#define DISPLAY_QSPI_HOST SPI2_HOST
#define DISPLAY_QSPI_CS GPIO_NUM_50
#define DISPLAY_QSPI_SCLK_V1_0 GPIO_NUM_44
#define DISPLAY_QSPI_RST_V1_0 GPIO_NUM_42
#define DISPLAY_QSPI_SCLK_V1_2 GPIO_NUM_42
#define DISPLAY_QSPI_RST_V1_2 GPIO_NUM_44
#define DISPLAY_QSPI_D0 GPIO_NUM_36
#define DISPLAY_QSPI_D1 GPIO_NUM_51
#define DISPLAY_QSPI_D2 GPIO_NUM_35
#define DISPLAY_QSPI_D3 GPIO_NUM_9

#define DISPLAY_WIDTH 480
#define DISPLAY_HEIGHT 480
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

// Keep top status icons clear of the display's rounded corners.
#define DISPLAY_TOP_BAR_HORIZONTAL_INSET (DISPLAY_WIDTH / 10)

#define TOUCH_INT_PIN GPIO_NUM_6

// Application AI button; the ROM BOOT button is GPIO61.
#define AI_BUTTON_GPIO GPIO_NUM_7

// Board power rails. VCC_PW is active low. CODEC_PW only exists on v1.0.
#define POWER_VCC_3V3_GPIO GPIO_NUM_60
#define POWER_CODEC_3V3_V1_0_GPIO GPIO_NUM_56
#define POWER_SHUTDOWN_GPIO GPIO_NUM_57

// The status LED is active low and only present on v1.0.
#define STATUS_LED_V1_0_GPIO GPIO_NUM_3

// The optional CameraBoard is supported in the left expansion slot only. Its
// EEPROM is at 0x50 when GPIO14 is held low; the right slot must remain at
// 0x51 while the left descriptor is read.
#define CAMERA_EEPROM_I2C_ADDR 0x50
#define CAMERA_EEPROM_ADDR_SELECT_LEFT_GPIO GPIO_NUM_14
#define CAMERA_EEPROM_ADDR_SELECT_RIGHT_GPIO GPIO_NUM_39

// CameraBoard OV3640 DVP interface. GPIO33 is shared with USB Serial/JTAG,
// so camera builds use the UART console.
#define CAMERA_PIN_D0 GPIO_NUM_16
#define CAMERA_PIN_D1 GPIO_NUM_15
#define CAMERA_PIN_D2 GPIO_NUM_33
#define CAMERA_PIN_D3 GPIO_NUM_4
#define CAMERA_PIN_D4 GPIO_NUM_14
#define CAMERA_PIN_D5 GPIO_NUM_12
#define CAMERA_PIN_D6 GPIO_NUM_18
#define CAMERA_PIN_D7 GPIO_NUM_13
#define CAMERA_PIN_VSYNC GPIO_NUM_55
#define CAMERA_PIN_HREF GPIO_NUM_19
#define CAMERA_PIN_PCLK GPIO_NUM_17
#define CAMERA_PIN_XCLK GPIO_NUM_NC
#define CAMERA_PIN_RESET GPIO_NUM_53
#define CAMERA_PIN_PWDN GPIO_NUM_48
#define CAMERA_FLASH_GPIO GPIO_NUM_34
#define CAMERA_SCCB_FREQ_HZ (100 * 1000)

// On v1.0 this is shared with the mainboard I2C0 bus. v1.1/v1.2 require a
// dedicated bus because their mainboard I2C is routed to GPIO56/GPIO3.
#define CAMERA_I2C_PORT_V1_2 I2C_NUM_1
#define CAMERA_I2C_SDA GPIO_NUM_0
#define CAMERA_I2C_SCL GPIO_NUM_1

#endif  // ESP_MOSAICO_CONFIG_H
