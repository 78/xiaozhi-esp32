#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE         24000
#define AUDIO_OUTPUT_SAMPLE_RATE        24000

#define AUDIO_INPUT_REFERENCE           true

#define AUDIO_I2S_GPIO_MCLK             GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS               GPIO_NUM_10
#define AUDIO_I2S_GPIO_BCLK             GPIO_NUM_12
#define AUDIO_I2S_GPIO_DIN              GPIO_NUM_11
#define AUDIO_I2S_GPIO_DOUT             GPIO_NUM_9

#define AUDIO_CODEC_PA_PIN              GPIO_NUM_53
#define AUDIO_CODEC_I2C_SDA_PIN         GPIO_NUM_7
#define AUDIO_CODEC_I2C_SCL_PIN         GPIO_NUM_8
#define AUDIO_CODEC_ES8311_ADDR         ES8311_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO                GPIO_NUM_35

#define DISPLAY_WIDTH 1024
#define DISPLAY_HEIGHT 600

#define LCD_BIT_PER_PIXEL               (16)
#define PIN_NUM_LCD_RST                 GPIO_NUM_23

#define DELAY_TIME_MS                   (3000)
#define LCD_MIPI_DSI_LANE_NUM           (2)    // 2 data lanes

#define MIPI_DSI_PHY_PWR_LDO_CHAN       (3)
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

#define DISPLAY_SWAP_XY                 false
#define DISPLAY_MIRROR_X                false
#define DISPLAY_MIRROR_Y                false

#define DISPLAY_OFFSET_X                0
#define DISPLAY_OFFSET_Y                0

#define DISPLAY_BACKLIGHT_PIN           GPIO_NUM_20
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// SD Card configuration (disabled by default)
// Enable one of the following by setting to 1 and set pins accordingly.
// Note: SDMMC may conflict with ESP-Hosted SDIO. If using ESP-Hosted via SDIO,
// prefer SDSPI mode for SD card or disable hosted SDIO.

// SDMMC 1-bit/4-bit mode
#ifndef SDCARD_SDMMC_ENABLED
#define SDCARD_SDMMC_ENABLED            0
#endif
// SDMMC bus width: set to 1 or 4
#ifndef SDCARD_SDMMC_BUS_WIDTH
// Use 4-bit bus width when enabling SDMMC
#define SDCARD_SDMMC_BUS_WIDTH          4
#endif
// SDMMC pin assignments (set to actual pins when enabling SDMMC)
#ifndef SDCARD_SDMMC_CLK_PIN
#define SDCARD_SDMMC_CLK_PIN            GPIO_NUM_43  // BSP_SD_CLK
#endif
#ifndef SDCARD_SDMMC_CMD_PIN
#define SDCARD_SDMMC_CMD_PIN            GPIO_NUM_44  // BSP_SD_CMD
#endif
#ifndef SDCARD_SDMMC_D0_PIN
#define SDCARD_SDMMC_D0_PIN             GPIO_NUM_39  // BSP_SD_D0
#endif
#ifndef SDCARD_SDMMC_D1_PIN
#define SDCARD_SDMMC_D1_PIN             GPIO_NUM_40  // BSP_SD_D1
#endif
#ifndef SDCARD_SDMMC_D2_PIN
#define SDCARD_SDMMC_D2_PIN             GPIO_NUM_41  // BSP_SD_D2
#endif
#ifndef SDCARD_SDMMC_D3_PIN
#define SDCARD_SDMMC_D3_PIN             GPIO_NUM_42  // BSP_SD_D3
#endif

// SDSPI mode (uses SPI bus)
#ifndef SDCARD_SDSPI_ENABLED
#define SDCARD_SDSPI_ENABLED            1
#endif
#ifndef SDCARD_SPI_HOST
#define SDCARD_SPI_HOST                 SPI3_HOST
#endif
#ifndef SDCARD_SPI_MOSI
#define SDCARD_SPI_MOSI                 GPIO_NUM_44  // BSP_SD_SPI_MOSI
#endif
#ifndef SDCARD_SPI_MISO
#define SDCARD_SPI_MISO                 GPIO_NUM_39  // BSP_SD_SPI_MISO
#endif
#ifndef SDCARD_SPI_SCLK
#define SDCARD_SPI_SCLK                 GPIO_NUM_43  // BSP_SD_SPI_CLK
#endif
#ifndef SDCARD_SPI_CS
#define SDCARD_SPI_CS                   GPIO_NUM_42  // BSP_SD_SPI_CS
#endif

#ifndef SDCARD_MOUNT_POINT
#define SDCARD_MOUNT_POINT              "/sdcard"
#endif

#endif // _BOARD_CONFIG_H_
