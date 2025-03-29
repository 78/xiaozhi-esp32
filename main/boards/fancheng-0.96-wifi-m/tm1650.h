#ifndef _TM1650_H_
#define _TM1650_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Display parameters
#define TM1650_BRIGHT1      0x11    // Brightness level 1 (lowest)
#define TM1650_BRIGHT2      0x21    // Brightness level 2
#define TM1650_BRIGHT3      0x31    // Brightness level 3
#define TM1650_BRIGHT4      0x41    // Brightness level 4
#define TM1650_BRIGHT5      0x51    // Brightness level 5
#define TM1650_BRIGHT6      0x61    // Brightness level 6
#define TM1650_BRIGHT7      0x71    // Brightness level 7
#define TM1650_BRIGHT8      0x01    // Brightness level 8 (highest)
#define TM1650_DSP_OFF      0x00    // Display off

#ifdef __cplusplus
extern "C" {
#endif
// Function prototypes
void TM1650_init(void);
void TM1650_cfg_display(uint8_t param);
void TM1650_clear(void);
void TM1650_print(uint8_t dig, uint8_t seg_data);
void TM1650_print_number(uint16_t number, bool leading_zeros);
void TM1650_display_time(uint8_t hours, uint8_t minutes);
// Digit segment patterns for 0-9
//extern const uint8_t TM1650_DIGIT_TABLE[10];
//extern const uint8_t TM1650_DIGIT_DP_TABLE[10];
#ifdef __cplusplus
}
#endif


#endif // _TM1650_H_ 