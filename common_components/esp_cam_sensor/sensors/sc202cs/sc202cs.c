/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "sc202cs_settings.h"
#include "sc202cs.h"

/*
 * SC202CS camera sensor gain control.
 * Note1: The analog gain only has coarse gain, and no fine gain, so in the adjustment of analog gain.
 * Digital gain needs to replace analog fine gain for smooth transition, so as to avoid AGC oscillation.
 * Note2: the analog gain of SC202CS will be affected by temperature, it is recommended to increase Dgain first and then Again.
 */
typedef struct {
    uint8_t dgain_fine; // digital gain fine
    uint8_t dgain_coarse; // digital gain coarse
    uint8_t analog_gain;
} sc202cs_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t gain_index; // current gain index

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} sc202cs_para_t;

struct sc202cs_cam {
    sc202cs_para_t sc202cs_para;
};

#define SC202CS_IO_MUX_LOCK(mux)
#define SC202CS_IO_MUX_UNLOCK(mux)
#define SC202CS_ENABLE_OUT_XCLK(pin,clk)
#define SC202CS_DISABLE_OUT_XCLK(pin)

#define SC202CS_FETCH_EXP_H(val)     (((val) >> 12) & 0xF)
#define SC202CS_FETCH_EXP_M(val)     (((val) >> 4) & 0xFF)
#define SC202CS_FETCH_EXP_L(val)     (((val) & 0xF) << 4)

#define SC202CS_PID         0xeb52
#define SC202CS_SENSOR_NAME "SC202CS"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC202CS_SUPPORT_NUM CONFIG_CAMERA_SC202CS_MAX_SUPPORT

static const uint32_t s_limited_abs_gain = CONFIG_CAMERA_SC202CS_ABSOLUTE_GAIN_LIMIT;
static size_t s_limited_abs_gain_index;
static const char *TAG = "sc202cs";

#if CONFIG_CAMERA_SC202CS_ANA_GAIN_PRIORITY
// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final abs_gain is multiplied by 1000.)
static const uint32_t sc202cs_abs_gain_val_map[] = {
    1000,
    1031,
    1063,
    1094,
    1125,
    1156,
    1188,
    1219,
    1250,
    1281,
    1313,
    1344,
    1375,
    1406,
    1438,
    1469,
    1500,
    1531,
    1563,
    1594,
    1625,
    1656,
    1688,
    1719,
    1750,
    1781,
    1813,
    1844,
    1875,
    1906,
    1938,
    1969,
    // 2X
    2000,
    2062,
    2126,
    2188,
    2250,
    2312,
    2376,
    2438,
    2500,
    2562,
    2626,
    2688,
    2750,
    2812,
    2876,
    2938,
    3000,
    3062,
    3126,
    3188,
    3250,
    3312,
    3376,
    3438,
    3500,
    3562,
    3626,
    3688,
    3750,
    3812,
    3876,
    3938,
    // 4X
    4000,
    4124,
    4252,
    4376,
    4500,
    4624,
    4752,
    4876,
    5000,
    5124,
    5252,
    5376,
    5500,
    5624,
    5752,
    5876,
    6000,
    6124,
    6252,
    6376,
    6500,
    6624,
    6752,
    6876,
    7000,
    7124,
    7252,
    7376,
    7500,
    7624,
    7752,
    7876,
    // 8X
    8000,
    8248,
    8504,
    8752,
    9000,
    9248,
    9504,
    9752,
    10000,
    10248,
    10504,
    10752,
    11000,
    11248,
    11504,
    11752,
    12000,
    12248,
    12504,
    12752,
    13000,
    13248,
    13504,
    13752,
    14000,
    14248,
    14504,
    14752,
    15000,
    15248,
    15504,
    15752,
    // 16X
    16000,
    16496,
    17008,
    17504,
    18000,
    18496,
    19008,
    19504,
    20000,
    20496,
    21008,
    21504,
    22000,
    22496,
    23008,
    23504,
    24000,
    24496,
    25008,
    25504,
    26000,
    26496,
    27008,
    27504,
    28000,
    28496,
    29008,
    29504,
    30000,
    30496,
    31008,
    31504,
    // 32X
    32000,
    33008,
    34000,
    35008,
    36000,
    37008,
    38000,
    39008,
    40000,
    41008,
    42000,
    43008,
    44000,
    45008,
    46000,
    47008,
    48000,
    49008,
    50000,
    51008,
    52000,
    53008,
    54000,
    55008,
    56000,
    57008,
    58000,
    59008,
    60000,
    61008,
    62000,
    63008,
};

// SC202CS Gain map format: [DIG_FINE, DIG_COARSE, ANG]
static const sc202cs_gain_t sc202cs_gain_map[] = {
    {0x80, 0x00, 0x00},
    {0x84, 0x00, 0x00},
    {0x88, 0x00, 0x00},
    {0x8c, 0x00, 0x00},
    {0x90, 0x00, 0x00},
    {0x94, 0x00, 0x00},
    {0x98, 0x00, 0x00},
    {0x9c, 0x00, 0x00},
    {0xa0, 0x00, 0x00},
    {0xa4, 0x00, 0x00},
    {0xa8, 0x00, 0x00},
    {0xac, 0x00, 0x00},
    {0xb0, 0x00, 0x00},
    {0xb4, 0x00, 0x00},
    {0xb8, 0x00, 0x00},
    {0xbc, 0x00, 0x00},
    {0xc0, 0x00, 0x00},
    {0xc4, 0x00, 0x00},
    {0xc8, 0x00, 0x00},
    {0xcc, 0x00, 0x00},
    {0xd0, 0x00, 0x00},
    {0xd4, 0x00, 0x00},
    {0xd8, 0x00, 0x00},
    {0xdc, 0x00, 0x00},
    {0xe0, 0x00, 0x00},
    {0xe4, 0x00, 0x00},
    {0xe8, 0x00, 0x00},
    {0xec, 0x00, 0x00},
    {0xf0, 0x00, 0x00},
    {0xf4, 0x00, 0x00},
    {0xf8, 0x00, 0x00},
    {0xfc, 0x00, 0x00},
    // 2X
    {0x80, 0x00, 0x01},
    {0x84, 0x00, 0x01},
    {0x88, 0x00, 0x01},
    {0x8c, 0x00, 0x01},
    {0x90, 0x00, 0x01},
    {0x94, 0x00, 0x01},
    {0x98, 0x00, 0x01},
    {0x9c, 0x00, 0x01},
    {0xa0, 0x00, 0x01},
    {0xa4, 0x00, 0x01},
    {0xa8, 0x00, 0x01},
    {0xac, 0x00, 0x01},
    {0xb0, 0x00, 0x01},
    {0xb4, 0x00, 0x01},
    {0xb8, 0x00, 0x01},
    {0xbc, 0x00, 0x01},
    {0xc0, 0x00, 0x01},
    {0xc4, 0x00, 0x01},
    {0xc8, 0x00, 0x01},
    {0xcc, 0x00, 0x01},
    {0xd0, 0x00, 0x01},
    {0xd4, 0x00, 0x01},
    {0xd8, 0x00, 0x01},
    {0xdc, 0x00, 0x01},
    {0xe0, 0x00, 0x01},
    {0xe4, 0x00, 0x01},
    {0xe8, 0x00, 0x01},
    {0xec, 0x00, 0x01},
    {0xf0, 0x00, 0x01},
    {0xf4, 0x00, 0x01},
    {0xf8, 0x00, 0x01},
    {0xfc, 0x00, 0x01},
    // 4X
    {0x80, 0x00, 0x03},
    {0x84, 0x00, 0x03},
    {0x88, 0x00, 0x03},
    {0x8c, 0x00, 0x03},
    {0x90, 0x00, 0x03},
    {0x94, 0x00, 0x03},
    {0x98, 0x00, 0x03},
    {0x9c, 0x00, 0x03},
    {0xa0, 0x00, 0x03},
    {0xa4, 0x00, 0x03},
    {0xa8, 0x00, 0x03},
    {0xac, 0x00, 0x03},
    {0xb0, 0x00, 0x03},
    {0xb4, 0x00, 0x03},
    {0xb8, 0x00, 0x03},
    {0xbc, 0x00, 0x03},
    {0xc0, 0x00, 0x03},
    {0xc4, 0x00, 0x03},
    {0xc8, 0x00, 0x03},
    {0xcc, 0x00, 0x03},
    {0xd0, 0x00, 0x03},
    {0xd4, 0x00, 0x03},
    {0xd8, 0x00, 0x03},
    {0xdc, 0x00, 0x03},
    {0xe0, 0x00, 0x03},
    {0xe4, 0x00, 0x03},
    {0xe8, 0x00, 0x03},
    {0xec, 0x00, 0x03},
    {0xf0, 0x00, 0x03},
    {0xf4, 0x00, 0x03},
    {0xf8, 0x00, 0x03},
    {0xfc, 0x00, 0x03},
    // 8X
    {0x80, 0x00, 0x07},
    {0x84, 0x00, 0x07},
    {0x88, 0x00, 0x07},
    {0x8c, 0x00, 0x07},
    {0x90, 0x00, 0x07},
    {0x94, 0x00, 0x07},
    {0x98, 0x00, 0x07},
    {0x9c, 0x00, 0x07},
    {0xa0, 0x00, 0x07},
    {0xa4, 0x00, 0x07},
    {0xa8, 0x00, 0x07},
    {0xac, 0x00, 0x07},
    {0xb0, 0x00, 0x07},
    {0xb4, 0x00, 0x07},
    {0xb8, 0x00, 0x07},
    {0xbc, 0x00, 0x07},
    {0xc0, 0x00, 0x07},
    {0xc4, 0x00, 0x07},
    {0xc8, 0x00, 0x07},
    {0xcc, 0x00, 0x07},
    {0xd0, 0x00, 0x07},
    {0xd4, 0x00, 0x07},
    {0xd8, 0x00, 0x07},
    {0xdc, 0x00, 0x07},
    {0xe0, 0x00, 0x07},
    {0xe4, 0x00, 0x07},
    {0xe8, 0x00, 0x07},
    {0xec, 0x00, 0x07},
    {0xf0, 0x00, 0x07},
    {0xf4, 0x00, 0x07},
    {0xf8, 0x00, 0x07},
    {0xfc, 0x00, 0x07},
    // 16X
    {0x80, 0x00, 0x0f},
    {0x84, 0x00, 0x0f},
    {0x88, 0x00, 0x0f},
    {0x8c, 0x00, 0x0f},
    {0x90, 0x00, 0x0f},
    {0x94, 0x00, 0x0f},
    {0x98, 0x00, 0x0f},
    {0x9c, 0x00, 0x0f},
    {0xa0, 0x00, 0x0f},
    {0xa4, 0x00, 0x0f},
    {0xa8, 0x00, 0x0f},
    {0xac, 0x00, 0x0f},
    {0xb0, 0x00, 0x0f},
    {0xb4, 0x00, 0x0f},
    {0xb8, 0x00, 0x0f},
    {0xbc, 0x00, 0x0f},
    {0xc0, 0x00, 0x0f},
    {0xc4, 0x00, 0x0f},
    {0xc8, 0x00, 0x0f},
    {0xcc, 0x00, 0x0f},
    {0xd0, 0x00, 0x0f},
    {0xd4, 0x00, 0x0f},
    {0xd8, 0x00, 0x0f},
    {0xdc, 0x00, 0x0f},
    {0xe0, 0x00, 0x0f},
    {0xe4, 0x00, 0x0f},
    {0xe8, 0x00, 0x0f},
    {0xec, 0x00, 0x0f},
    {0xf0, 0x00, 0x0f},
    {0xf4, 0x00, 0x0f},
    {0xf8, 0x00, 0x0f},
    {0xfc, 0x00, 0x0f},
    //32x
    {0x80, 0x01, 0x0f},
    {0x84, 0x01, 0x0f},
    {0x88, 0x01, 0x0f},
    {0x8c, 0x01, 0x0f},
    {0x90, 0x01, 0x0f},
    {0x94, 0x01, 0x0f},
    {0x98, 0x01, 0x0f},
    {0x9c, 0x01, 0x0f},
    {0xa0, 0x01, 0x0f},
    {0xa4, 0x01, 0x0f},
    {0xa8, 0x01, 0x0f},
    {0xac, 0x01, 0x0f},
    {0xb0, 0x01, 0x0f},
    {0xb4, 0x01, 0x0f},
    {0xb8, 0x01, 0x0f},
    {0xbc, 0x01, 0x0f},
    {0xc0, 0x01, 0x0f},
    {0xc4, 0x01, 0x0f},
    {0xc8, 0x01, 0x0f},
    {0xcc, 0x01, 0x0f},
    {0xd0, 0x01, 0x0f},
    {0xd4, 0x01, 0x0f},
    {0xd8, 0x01, 0x0f},
    {0xdc, 0x01, 0x0f},
    {0xe0, 0x01, 0x0f},
    {0xe4, 0x01, 0x0f},
    {0xe8, 0x01, 0x0f},
    {0xec, 0x01, 0x0f},
    {0xf0, 0x01, 0x0f},
    {0xf4, 0x01, 0x0f},
    {0xf8, 0x01, 0x0f},
    {0xfc, 0x01, 0x0f},
};
#elif CONFIG_CAMERA_SC202CS_DIG_GAIN_PRIORITY
// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final abs_gain is multiplied by 1000.)
static const uint32_t sc202cs_abs_gain_val_map[] = {
    1000,
    1031,
    1063,
    1094,
    1125,
    1156,
    1188,
    1219,
    1250,
    1281,
    1313,
    1344,
    1375,
    1406,
    1438,
    1469,
    1500,
    1531,
    1563,
    1594,
    1625,
    1656,
    1688,
    1719,
    1750,
    1781,
    1813,
    1844,
    1875,
    1906,
    1938,
    1969,
    // 2X
    2000,
    2063,
    2125,
    2188,
    2250,
    2313,
    2375,
    2438,
    2500,
    2563,
    2625,
    2688,
    2750,
    2813,
    2875,
    2938,
    3000,
    3063,
    3125,
    3188,
    3250,
    3313,
    3375,
    3438,
    3500,
    3563,
    3625,
    3688,
    3750,
    3813,
    3875,
    3938,
    // 4X
    4000,
    4126,
    4250,
    4376,
    4500,
    4626,
    4750,
    4876,
    5000,
    5126,
    5250,
    5376,
    5500,
    5626,
    5750,
    5876,
    6000,
    6126,
    6250,
    6376,
    6500,
    6626,
    6750,
    6876,
    7000,
    7126,
    7250,
    7376,
    7500,
    7626,
    7750,
    7876,
    // 8X
    8000,
    8252,
    8500,
    8752,
    9000,
    9252,
    9500,
    9752,
    10000,
    10252,
    10500,
    10752,
    11000,
    11252,
    11500,
    11752,
    12000,
    12252,
    12500,
    12752,
    13000,
    13252,
    13500,
    13752,
    14000,
    14252,
    14500,
    14752,
    15000,
    15252,
    15500,
    15752,
    // 16X
    16000,
    16504,
    17000,
    17504,
    18000,
    18504,
    19000,
    19504,
    20000,
    20504,
    21000,
    21504,
    22000,
    22504,
    23000,
    23504,
    24000,
    24504,
    25000,
    25504,
    26000,
    26504,
    27000,
    27504,
    28000,
    28504,
    29000,
    29504,
    30000,
    30504,
    31000,
    31504,
    // 32X
    32000,
    33008,
    34000,
    35008,
    36000,
    37008,
    38000,
    39008,
    40000,
    41008,
    42000,
    43008,
    44000,
    45008,
    46000,
    47008,
    48000,
    49008,
    50000,
    51008,
    52000,
    53008,
    54000,
    55008,
    56000,
    57008,
    58000,
    59008,
    60000,
    61008,
    62000,
    63008,
};

// SC202CS Gain map format: [DIG_FINE, DIG_COARSE, ANG]
static const sc202cs_gain_t sc202cs_gain_map[] = {
    {0x80, 0x00, 0x00},
    {0x84, 0x00, 0x00},
    {0x88, 0x00, 0x00},
    {0x8c, 0x00, 0x00},
    {0x90, 0x00, 0x00},
    {0x94, 0x00, 0x00},
    {0x98, 0x00, 0x00},
    {0x9c, 0x00, 0x00},
    {0xa0, 0x00, 0x00},
    {0xa4, 0x00, 0x00},
    {0xa8, 0x00, 0x00},
    {0xac, 0x00, 0x00},
    {0xb0, 0x00, 0x00},
    {0xb4, 0x00, 0x00},
    {0xb8, 0x00, 0x00},
    {0xbc, 0x00, 0x00},
    {0xc0, 0x00, 0x00},
    {0xc4, 0x00, 0x00},
    {0xc8, 0x00, 0x00},
    {0xcc, 0x00, 0x00},
    {0xd0, 0x00, 0x00},
    {0xd4, 0x00, 0x00},
    {0xd8, 0x00, 0x00},
    {0xdc, 0x00, 0x00},
    {0xe0, 0x00, 0x00},
    {0xe4, 0x00, 0x00},
    {0xe8, 0x00, 0x00},
    {0xec, 0x00, 0x00},
    {0xf0, 0x00, 0x00},
    {0xf4, 0x00, 0x00},
    {0xf8, 0x00, 0x00},
    {0xfc, 0x00, 0x00},
    // 2X
    {0x80, 0x01, 0x00},
    {0x84, 0x01, 0x00},
    {0x88, 0x01, 0x00},
    {0x8c, 0x01, 0x00},
    {0x90, 0x01, 0x00},
    {0x94, 0x01, 0x00},
    {0x98, 0x01, 0x00},
    {0x9c, 0x01, 0x00},
    {0xa0, 0x01, 0x00},
    {0xa4, 0x01, 0x00},
    {0xa8, 0x01, 0x00},
    {0xac, 0x01, 0x00},
    {0xb0, 0x01, 0x00},
    {0xb4, 0x01, 0x00},
    {0xb8, 0x01, 0x00},
    {0xbc, 0x01, 0x00},
    {0xc0, 0x01, 0x00},
    {0xc4, 0x01, 0x00},
    {0xc8, 0x01, 0x00},
    {0xcc, 0x01, 0x00},
    {0xd0, 0x01, 0x00},
    {0xd4, 0x01, 0x00},
    {0xd8, 0x01, 0x00},
    {0xdc, 0x01, 0x00},
    {0xe0, 0x01, 0x00},
    {0xe4, 0x01, 0x00},
    {0xe8, 0x01, 0x00},
    {0xec, 0x01, 0x00},
    {0xf0, 0x01, 0x00},
    {0xf4, 0x01, 0x00},
    {0xf8, 0x01, 0x00},
    {0xfc, 0x01, 0x00},
    // 4X
    {0x80, 0x01, 0x01},
    {0x84, 0x01, 0x01},
    {0x88, 0x01, 0x01},
    {0x8c, 0x01, 0x01},
    {0x90, 0x01, 0x01},
    {0x94, 0x01, 0x01},
    {0x98, 0x01, 0x01},
    {0x9c, 0x01, 0x01},
    {0xa0, 0x01, 0x01},
    {0xa4, 0x01, 0x01},
    {0xa8, 0x01, 0x01},
    {0xac, 0x01, 0x01},
    {0xb0, 0x01, 0x01},
    {0xb4, 0x01, 0x01},
    {0xb8, 0x01, 0x01},
    {0xbc, 0x01, 0x01},
    {0xc0, 0x01, 0x01},
    {0xc4, 0x01, 0x01},
    {0xc8, 0x01, 0x01},
    {0xcc, 0x01, 0x01},
    {0xd0, 0x01, 0x01},
    {0xd4, 0x01, 0x01},
    {0xd8, 0x01, 0x01},
    {0xdc, 0x01, 0x01},
    {0xe0, 0x01, 0x01},
    {0xe4, 0x01, 0x01},
    {0xe8, 0x01, 0x01},
    {0xec, 0x01, 0x01},
    {0xf0, 0x01, 0x01},
    {0xf4, 0x01, 0x01},
    {0xf8, 0x01, 0x01},
    {0xfc, 0x01, 0x01},
    // 8X
    {0x80, 0x01, 0x03},
    {0x84, 0x01, 0x03},
    {0x88, 0x01, 0x03},
    {0x8c, 0x01, 0x03},
    {0x90, 0x01, 0x03},
    {0x94, 0x01, 0x03},
    {0x98, 0x01, 0x03},
    {0x9c, 0x01, 0x03},
    {0xa0, 0x01, 0x03},
    {0xa4, 0x01, 0x03},
    {0xa8, 0x01, 0x03},
    {0xac, 0x01, 0x03},
    {0xb0, 0x01, 0x03},
    {0xb4, 0x01, 0x03},
    {0xb8, 0x01, 0x03},
    {0xbc, 0x01, 0x03},
    {0xc0, 0x01, 0x03},
    {0xc4, 0x01, 0x03},
    {0xc8, 0x01, 0x03},
    {0xcc, 0x01, 0x03},
    {0xd0, 0x01, 0x03},
    {0xd4, 0x01, 0x03},
    {0xd8, 0x01, 0x03},
    {0xdc, 0x01, 0x03},
    {0xe0, 0x01, 0x03},
    {0xe4, 0x01, 0x03},
    {0xe8, 0x01, 0x03},
    {0xec, 0x01, 0x03},
    {0xf0, 0x01, 0x03},
    {0xf4, 0x01, 0x03},
    {0xf8, 0x01, 0x03},
    {0xfc, 0x01, 0x03},
    // 16X
    {0x80, 0x01, 0x07},
    {0x84, 0x01, 0x07},
    {0x88, 0x01, 0x07},
    {0x8c, 0x01, 0x07},
    {0x90, 0x01, 0x07},
    {0x94, 0x01, 0x07},
    {0x98, 0x01, 0x07},
    {0x9c, 0x01, 0x07},
    {0xa0, 0x01, 0x07},
    {0xa4, 0x01, 0x07},
    {0xa8, 0x01, 0x07},
    {0xac, 0x01, 0x07},
    {0xb0, 0x01, 0x07},
    {0xb4, 0x01, 0x07},
    {0xb8, 0x01, 0x07},
    {0xbc, 0x01, 0x07},
    {0xc0, 0x01, 0x07},
    {0xc4, 0x01, 0x07},
    {0xc8, 0x01, 0x07},
    {0xcc, 0x01, 0x07},
    {0xd0, 0x01, 0x07},
    {0xd4, 0x01, 0x07},
    {0xd8, 0x01, 0x07},
    {0xdc, 0x01, 0x07},
    {0xe0, 0x01, 0x07},
    {0xe4, 0x01, 0x07},
    {0xe8, 0x01, 0x07},
    {0xec, 0x01, 0x07},
    {0xf0, 0x01, 0x07},
    {0xf4, 0x01, 0x07},
    {0xf8, 0x01, 0x07},
    {0xfc, 0x01, 0x07},
    //32x
    {0x80, 0x01, 0x0f},
    {0x84, 0x01, 0x0f},
    {0x88, 0x01, 0x0f},
    {0x8c, 0x01, 0x0f},
    {0x90, 0x01, 0x0f},
    {0x94, 0x01, 0x0f},
    {0x98, 0x01, 0x0f},
    {0x9c, 0x01, 0x0f},
    {0xa0, 0x01, 0x0f},
    {0xa4, 0x01, 0x0f},
    {0xa8, 0x01, 0x0f},
    {0xac, 0x01, 0x0f},
    {0xb0, 0x01, 0x0f},
    {0xb4, 0x01, 0x0f},
    {0xb8, 0x01, 0x0f},
    {0xbc, 0x01, 0x0f},
    {0xc0, 0x01, 0x0f},
    {0xc4, 0x01, 0x0f},
    {0xc8, 0x01, 0x0f},
    {0xcc, 0x01, 0x0f},
    {0xd0, 0x01, 0x0f},
    {0xd4, 0x01, 0x0f},
    {0xd8, 0x01, 0x0f},
    {0xdc, 0x01, 0x0f},
    {0xe0, 0x01, 0x0f},
    {0xe4, 0x01, 0x0f},
    {0xe8, 0x01, 0x0f},
    {0xec, 0x01, 0x0f},
    {0xf0, 0x01, 0x0f},
    {0xf4, 0x01, 0x0f},
    {0xf8, 0x01, 0x0f},
    {0xfc, 0x01, 0x0f},
};
#endif // end CONFIG_ANA_GAIN_PRIORITY

static const esp_cam_sensor_isp_info_t sc202cs_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 72000000,
            .vts = 1250,
            .hts = 1920,
            .gain_def = 0, // gain index, depend on {0x3e06, 0x3e07, 0x3e09}, since these registers are not set in format reg_list, the default values ​​are used here.
            .exp_def = 0x4dc, // depend on {0x3e00, 0x3e01, 0x3e02}, see format_reg_list to get the default value.
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 72000000,
            .vts = 1250,
            .hts = 1920,
            .gain_def = 0, // gain index, depend on {0x3e06, 0x3e07, 0x3e09}, since these registers are not set in format reg_list, the default values ​​are used here.
            .exp_def = 0x4dc, // depend on {0x3e00, 0x3e01, 0x3e02}, see format_reg_list to get the default value.
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 72000000,
            .vts = 1250,
            .hts = 1920,
            .gain_def = 0, // gain index, depend on {0x3e06, 0x3e07, 0x3e09}, since these registers are not set in format reg_list, the default values ​​are used here.
            .exp_def = 0x4dc, // depend on {0x3e00, 0x3e01, 0x3e02}, see format_reg_list to get the default value.
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 72000000,
            .vts = 1250,
            .hts = 1920,
            .gain_def = 0, // gain index, depend on {0x3e06, 0x3e07, 0x3e09}, since these registers are not set in format reg_list, the default values ​​are used here.
            .exp_def = 0x4dc, // depend on {0x3e00, 0x3e01, 0x3e02}, see format_reg_list to get the default value.
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t sc202cs_format_info[] = {
    {
        .name = "MIPI_1lane_24Minput_RAW8_1280x720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_1lane_raw8_1280x720_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_raw8_1280x720_30fps),
        .fps = 30,
        .isp_info = &sc202cs_isp_info[0],
        .mipi_info = {
            .mipi_clk = 576000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW8_1600x1200_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1600,
        .height = 1200,
        .regs = init_reglist_MIPI_1lane_raw8_1600x1200_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_raw8_1600x1200_30fps),
        .fps = 30,
        .isp_info = &sc202cs_isp_info[1],
        .mipi_info = {
            .mipi_clk = 576000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW10_1600x1200_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1600,
        .height = 1200,
        .regs = init_reglist_MIPI_1lane_raw10_1600x1200_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_raw10_1600x1200_30fps),
        .fps = 30,
        .isp_info = &sc202cs_isp_info[2],
        .mipi_info = {
            .mipi_clk = 720000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW10_1600x900_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1600,
        .height = 900,
        .regs = init_reglist_MIPI_1lane_raw10_1600x900_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_raw10_1600x900_30fps),
        .fps = 30,
        .isp_info = &sc202cs_isp_info[3],
        .mipi_info = {
            .mipi_clk = 720000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
};

static esp_err_t sc202cs_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc202cs_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sc202cs_write_array(esp_sccb_io_handle_t sccb_handle, sc202cs_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != SC202CS_REG_END) {
        if (regarray[i].reg != SC202CS_REG_DELAY) {
            ret = sc202cs_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    return ret;
}

static esp_err_t sc202cs_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc202cs_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = sc202cs_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sc202cs_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return sc202cs_set_reg_bits(dev->sccb_handle, 0x4501, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t sc202cs_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc202cs_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc202cs_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc202cs_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sc202cs_read(dev->sccb_handle, SC202CS_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sc202cs_read(dev->sccb_handle, SC202CS_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sc202cs_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = sc202cs_write(dev->sccb_handle, SC202CS_REG_SLEEP_MODE, enable ? 0x01 : 0x00);

    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sc202cs_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return sc202cs_set_reg_bits(dev->sccb_handle, 0x3221, 1, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc202cs_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return sc202cs_set_reg_bits(dev->sccb_handle, 0x3221, 5, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc202cs_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0x08;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - 6; // max = VTS-6 = height+vblank-6, so when update vblank, exposure_max must be updated
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = s_limited_abs_gain_index;
        qdesc->enumeration.elements = sc202cs_abs_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // default gain index
        break;
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t sc202cs_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct sc202cs_cam *cam_sc202cs = (struct sc202cs_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_sc202cs->sc202cs_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_sc202cs->sc202cs_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t sc202cs_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    uint32_t u32_val = *(uint32_t *)arg;
    struct sc202cs_cam *cam_sc202cs = (struct sc202cs_cam *)dev->priv;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        ESP_LOGD(TAG, "set exposure 0x%" PRIx32, u32_val);
        /* 4 least significant bits of expsoure are fractional part */
        ret = sc202cs_write(dev->sccb_handle,
                            SC202CS_REG_SHUTTER_TIME_H,
                            SC202CS_FETCH_EXP_H(u32_val));
        ret |= sc202cs_write(dev->sccb_handle,
                             SC202CS_REG_SHUTTER_TIME_M,
                             SC202CS_FETCH_EXP_M(u32_val));
        ret |= sc202cs_write(dev->sccb_handle,
                             SC202CS_REG_SHUTTER_TIME_L,
                             SC202CS_FETCH_EXP_L(u32_val));
        if (ret == ESP_OK) {
            cam_sc202cs->sc202cs_para.exposure_val = u32_val;
        }
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        ESP_LOGD(TAG, "dgain_fine %" PRIx8 ", dgain_coarse %" PRIx8 ", again_coarse %" PRIx8, sc202cs_gain_map[u32_val].dgain_fine, sc202cs_gain_map[u32_val].dgain_coarse, sc202cs_gain_map[u32_val].analog_gain);
        ret = sc202cs_write(dev->sccb_handle,
                            SC202CS_REG_DIG_FINE_GAIN,
                            sc202cs_gain_map[u32_val].dgain_fine);
        ret |= sc202cs_write(dev->sccb_handle,
                             SC202CS_REG_DIG_COARSE_GAIN,
                             sc202cs_gain_map[u32_val].dgain_coarse);
        ret |= sc202cs_write(dev->sccb_handle,
                             SC202CS_REG_ANG_GAIN,
                             sc202cs_gain_map[u32_val].analog_gain);
        if (ret == ESP_OK) {
            cam_sc202cs->sc202cs_para.gain_index = u32_val;
        }
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = sc202cs_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = sc202cs_set_mirror(dev, *value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t sc202cs_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(sc202cs_format_info);
    formats->format_array = &sc202cs_format_info[0];
    return ESP_OK;
}

static esp_err_t sc202cs_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t sc202cs_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct sc202cs_cam *cam_sc202cs = (struct sc202cs_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &sc202cs_format_info[CONFIG_CAMERA_SC202CS_MIPI_IF_FORMAT_INDEX_DAFAULT];
    }

    ret = sc202cs_write_array(dev->sccb_handle, (sc202cs_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_sc202cs->sc202cs_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_sc202cs->sc202cs_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;

    return ret;
}

static esp_err_t sc202cs_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);

    esp_err_t ret = ESP_FAIL;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(esp_cam_sensor_format_t));
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t sc202cs_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC202CS_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc202cs_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc202cs_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc202cs_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc202cs_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sc202cs_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc202cs_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc202cs_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SC202CS_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sc202cs_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC202CS_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sc202cs_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC202CS_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sc202cs_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc202cs (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t sc202cs_ops = {
    .query_para_desc = sc202cs_query_para_desc,
    .get_para_value = sc202cs_get_para_value,
    .set_para_value = sc202cs_set_para_value,
    .query_support_formats = sc202cs_query_support_formats,
    .query_support_capability = sc202cs_query_support_capability,
    .set_format = sc202cs_set_format,
    .get_format = sc202cs_get_format,
    .priv_ioctl = sc202cs_priv_ioctl,
    .del = sc202cs_delete
};

esp_cam_sensor_device_t *sc202cs_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct sc202cs_cam *cam_sc202cs;
    s_limited_abs_gain_index = ARRAY_SIZE(sc202cs_abs_gain_val_map);
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_sc202cs = heap_caps_calloc(1, sizeof(struct sc202cs_cam), MALLOC_CAP_DEFAULT);
    if (!cam_sc202cs) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)SC202CS_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sc202cs_ops;
    dev->priv = cam_sc202cs;
    dev->cur_format = &sc202cs_format_info[CONFIG_CAMERA_SC202CS_MIPI_IF_FORMAT_INDEX_DAFAULT];
    for (size_t i = 0; i < ARRAY_SIZE(sc202cs_abs_gain_val_map); i++) {
        if (sc202cs_abs_gain_val_map[i] > s_limited_abs_gain) {
            s_limited_abs_gain_index = i - 1;
            break;
        }
    }

    // Configure sensor power, clock, and SCCB port
    if (sc202cs_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc202cs_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC202CS_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC202CS, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc202cs_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC202CS_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc202cs_detect, ESP_CAM_SENSOR_MIPI_CSI, SC202CS_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc202cs_detect(config);
}
#endif
