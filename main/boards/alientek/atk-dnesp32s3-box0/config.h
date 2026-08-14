#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

enum XiaozhiStatus {
    kDevice_null,
    kDevice_join_Sleep,
    kDevice_Exit_Sleep,
    kDevice_Distributionnetwork,
    kDevice_Exit_Distributionnetwork,
};

enum LcdStatus {
    kDevicelcdbacklightOn,
    kDevicelcdbacklightOff,
};

enum WakeStatus {
    kDeviceAwakened,
    kDeviceWaitWake,
    kDeviceSleeped,
};

enum PowerSupply {
    kDeviceTypecSupply,
    kDeviceBatterySupply,
};

enum PowerSleep {
    kDeviceNoSleep,
    kDeviceDeepSleep,
    kDeviceNeutralSleep,
};

#define SYS_POW_PIN GPIO_NUM_2
#define CHG_CTRL_PIN GPIO_NUM_47
#define CODEC_PWR_PIN GPIO_NUM_14
#define CHRG_PIN GPIO_NUM_48

#define BAT_VSEN_PIN GPIO_NUM_1

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS GPIO_NUM_10
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_9

#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_11
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_12
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR

#define AUDIO_SPK_GPIO_PIN GPIO_NUM_21

#define R_BUTTON_GPIO GPIO_NUM_0
#define M_BUTTON_GPIO GPIO_NUM_4
#define L_BUTTON_GPIO GPIO_NUM_3

#define BUILTIN_LED_GPIO GPIO_NUM_13

#define LCD_SCLK_PIN GPIO_NUM_39
#define LCD_MOSI_PIN GPIO_NUM_40
#define LCD_MISO_PIN GPIO_NUM_NC
#define LCD_DC_PIN GPIO_NUM_38
#define LCD_CS_PIN GPIO_NUM_41
#define LCD_RST_PIN GPIO_NUM_NC

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_42
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#endif // _BOARD_CONFIG_H_

