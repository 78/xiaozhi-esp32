#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"  //需要用us延迟函数

// Define GPIO pins for TM1650
#define TM1650_SCL_PIN      42
#define TM1650_SDA_PIN      41

// Define GPIO control macros
#define TM1650_SCL(x)       gpio_set_level(TM1650_SCL_PIN, (x))
#define TM1650_SDA(x)       gpio_set_level(TM1650_SDA_PIN, (x))
#define TM1650_SDA_READ     gpio_get_level(TM1650_SDA_PIN)

// Digit positions
#define TM1650_DIG1     0
#define TM1650_DIG2     1
#define TM1650_DIG3     2
#define TM1650_DIG4     3

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

// Segment patterns for 0-9 digits (common anode 7-segment display)
const uint8_t TM1650_DIGIT_TABLE[16] = {
    0x3f,    /* 0  -> 0b00111111 */
    0x06,    /* 1  -> 0b00000110 */
    0x5b,    /* 2  -> 0b01011011 */
    0x4f,    /* 3  -> 0b01001111 */
    0x66,    /* 4  -> 0b01100110 */
    0x6d,    /* 5  -> 0b01101101 */
    0x7d,    /* 6  -> 0b01111101 */
    0x07,    /* 7  -> 0b00000111 */
    0x7f,    /* 8  -> 0b01111111 */
    0x6f,    /* 9  -> 0b01101111 */
    0x77,    /* A  -> 0b01110111 */
    0x7c,    /* b  -> 0b01111100 */
    0x39,    /* C  -> 0b00111001 */
    0x5e,    /* d  -> 0b01011110 */
    0x79,    /* E  -> 0b01111001 */
    0x71     /* F  -> 0b01110001 */
};

// Segment patterns with decimal point
const uint8_t TM1650_DIGIT_DP_TABLE[10] = {0xbf, 0x86, 0xdb, 0xcf, 0xe6, 0xed, 0xfd, 0x87, 0xff, 0xef};

// Function declarations
static void TM1650_IIC_delay(void);
static void TM1650_IIC_start(void);
static void TM1650_IIC_stop(void);
static void TM1650_IIC_send_byte(uint8_t bytedata);
static void TM1650_IIC_wait_ack(void);
void TM1650_init(void);
void TM1650_cfg_display(uint8_t param);
void TM1650_clear(void);
void TM1650_print(uint8_t dig, uint8_t seg_data);
void TM1650_print_number(uint16_t number, bool leading_zeros);
void TM1650_print_cycle(void);
void TM1650_display_time(uint8_t hours, uint8_t minutes);

/**
 * @brief       IIC delay function, used to control IIC read/write speed
 * @param       None
 * @retval      None
 */
static void TM1650_IIC_delay(void)
{
    esp_rom_delay_us(5);    // 2us delay, read/write speed within 250Khz
}

/**
 * @brief       Generate IIC start signal
 * @param       None
 * @retval      None
 */
static void TM1650_IIC_start(void)
{
    TM1650_SDA(1);
    TM1650_SCL(1);          // START signal: when SCL is high, SDA changes from high to low
    TM1650_IIC_delay();
    TM1650_SDA(0);
    TM1650_IIC_delay();
    
    TM1650_SCL(0);          // Clamp the I2C bus, ready to send or receive data
    TM1650_IIC_delay();
}

/**
 * @brief       Generate IIC stop signal
 * @param       None
 * @retval      None
 */
static void TM1650_IIC_stop(void)
{
    TM1650_SDA(0);
    TM1650_IIC_delay();
    TM1650_SCL(1);          // STOP signal: when SCL is high, SDA changes from low to high
    TM1650_IIC_delay();
    TM1650_SDA(1);
    TM1650_IIC_delay();
}

/**
 * @brief       Send a byte via IIC
 * @param       bytedata: data to send
 * @retval      None
 */
static void TM1650_IIC_send_byte(uint8_t bytedata)
{
    for (uint8_t i = 0; i < 8; i++) {
        TM1650_SDA((bytedata & 0x80) >> 7);    // Send MSB first
        TM1650_IIC_delay();
        
        // Generate a clock signal
        TM1650_SCL(1);
        TM1650_IIC_delay();
        TM1650_SCL(0);
        TM1650_IIC_delay();
        
        bytedata <<= 1;                        // Left shift 1 bit for next send
    }
    TM1650_SDA(1);                             // Release SDA line after sending
    TM1650_IIC_delay();
}

/**
 * @brief       Wait for ACK signal
 * @param       None
 * @retval      None
 */
static void TM1650_IIC_wait_ack(void)
{
    TM1650_SDA(1);          // Release SDA line (external device can pull SDA low)
    TM1650_IIC_delay();
    
    // Generate a clock signal
    TM1650_SCL(1);          // SCL=1, slave can return ACK
    TM1650_IIC_delay();
    
    // Wait for ACK but with timeout (not using while loop like in reference)
    uint8_t waittime = 0;
    while (TM1650_SDA_READ) {
        waittime++;
        if (waittime > 100) {
            break;  // Timeout
        }
        TM1650_IIC_delay();
    }
    
    TM1650_SCL(0);          // SCL=0, end ACK check
    TM1650_IIC_delay();
}

/**
 * @brief       Initialize TM1650 and GPIO pins
 * @param       None
 * @retval      None
 */
void TM1650_init(void)
{
 

        gpio_config_t config1 = {
            .pin_bit_mask = (1ULL << TM1650_SCL_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ONLY,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config1));

        gpio_config_t config2 = {
            .pin_bit_mask = (1ULL << TM1650_SDA_PIN),
            .mode = GPIO_MODE_INPUT_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ONLY,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config2));
 



    
    // Set initial pin states
    TM1650_SDA(1);
    TM1650_SCL(1);
    
    // Configure display with maximum brightness
    TM1650_cfg_display(TM1650_BRIGHT8);
    
    // Clear the display
    TM1650_clear();
}

/**
 * @brief       Configure display brightness
 * @param       param: TM1650_DSP_OFF (turn off display);
 *                     TM1650_BRIGHT1-8 (brightness levels 1-8, turn on display)
 * @retval      None
 */
void TM1650_cfg_display(uint8_t param)
{
    TM1650_IIC_start();
    TM1650_IIC_send_byte(0x48);  // TM1650 command address for display control
    TM1650_IIC_wait_ack();
    TM1650_IIC_send_byte(param); // Brightness parameter
    TM1650_IIC_wait_ack();
    TM1650_IIC_stop();
}

/**
 * @brief       Clear display (set all segments to 0)
 * @param       None
 * @retval      None
 */
void TM1650_clear(void)
{
    for (uint8_t dig = TM1650_DIG1; dig <= TM1650_DIG4; dig++) {
        TM1650_print(dig, 0);
    }
}

/**
 * @brief       Write segment data to a specific digit position
 * @param       dig: Digit position (0-3)
 * @param       seg_data: Segment data to write
 * @retval      None
 * @note        DIG1 address: 0x68; DIG2: 0x6A; DIG3: 0x6C; DIG4: 0x6E
 */
void TM1650_print(uint8_t dig, uint8_t seg_data)
{
    TM1650_IIC_start();
    TM1650_IIC_send_byte(dig * 2 + 0x68); // Select digit (address starts at 0x68)
    TM1650_IIC_wait_ack();
    TM1650_IIC_send_byte(seg_data);       // Segment data
    TM1650_IIC_wait_ack();
    TM1650_IIC_stop();
}

/**
 * @brief       Display a number on the 4-digit display
 * @param       number: Number to display (0-9999)
 * @param       leading_zeros: Whether to display leading zeros
 * @retval      None
 */

void TM1650_print_number(uint16_t number, bool leading_zeros)
{
    uint8_t digit;
    uint8_t leading = leading_zeros ? 0 : 1;
    
    // Handle thousands digit
    digit = number / 1000;
    if (digit > 0 || leading == 0) {
        TM1650_print(TM1650_DIG1, TM1650_DIGIT_TABLE[digit]);
        leading = 0;
    } else {
        TM1650_print(TM1650_DIG1, 0);  // blank
    }
    
    // Handle hundreds digit
    number %= 1000;
    digit = number / 100;
    if (digit > 0 || leading == 0) {
        TM1650_print(TM1650_DIG2, TM1650_DIGIT_TABLE[digit]);
        leading = 0;
    } else {
        TM1650_print(TM1650_DIG2, 0);  // blank
    }
    
    // Handle tens digit
    number %= 100;
    digit = number / 10;
    if (digit > 0 || leading == 0) {
        TM1650_print(TM1650_DIG3, TM1650_DIGIT_TABLE[digit]);
        leading = 0;
    } else {
        TM1650_print(TM1650_DIG3, 0);  // blank
    }
    
    // Handle units digit (always shown)
    digit = number % 10;
    TM1650_print(TM1650_DIG4, TM1650_DIGIT_TABLE[digit]);
}

/**
 * @brief       Display time in format H.MM (hours with decimal point, minutes)
 * @param       hours: Hours to display (0-23)
 * @param       minutes: Minutes to display (0-59)
 * @retval      None
 * @note        If hours < 10, the tens digit will be blank
 */
void TM1650_display_time(uint8_t hours, uint8_t minutes)
{
    uint8_t hour_tens, hour_units, min_tens, min_units;
    
    // Extract digits
    hour_tens = hours / 10;
    hour_units = hours % 10;
    min_tens = minutes / 10;
    min_units = minutes % 10;
    
    // Display hour tens digit (or blank if zero)
    if (hour_tens > 0) {
        TM1650_print(TM1650_DIG1, TM1650_DIGIT_TABLE[hour_tens]);
    } else {
        TM1650_print(TM1650_DIG1, 0); // blank
    }
    
    // Display hour units digit with decimal point
    TM1650_print(TM1650_DIG2, TM1650_DIGIT_DP_TABLE[hour_units]);
    
    // Display minute digits
    TM1650_print(TM1650_DIG3, TM1650_DIGIT_TABLE[min_tens]);
    TM1650_print(TM1650_DIG4, TM1650_DIGIT_TABLE[min_units]);
}

/**
 * @brief       Cycle through displaying all digits in the digit table
 * @param       None
 * @retval      None
 */
void TM1650_print_cycle(void)
{
    uint8_t i;
    uint8_t dig = 0;
    uint8_t data_length = sizeof(TM1650_DIGIT_TABLE) / sizeof(TM1650_DIGIT_TABLE[0]);
    
    for (i = 0; i < data_length; i++) {
        TM1650_print(dig, TM1650_DIGIT_TABLE[i]);
        if (dig < 3) {
            dig++;
        } else {
            dig = 0;
            vTaskDelay(500 / portTICK_PERIOD_MS);  // Use FreeRTOS delay instead of delay_ms
        }
    }
} 