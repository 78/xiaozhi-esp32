#ifndef _DISPLAY_EPD_W21_SPI_
#define _DISPLAY_EPD_W21_SPI_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdint.h>
void init_gpio(uint64_t gpio_num, gpio_mode_t gpio_mode);
void EPD_spi_init(void);
void SPI_Write(uint8_t value);
void EPD_W21_WriteCMD(uint8_t command);
void EPD_W21_WriteDATA(uint8_t datas);

#ifdef __cplusplus
}
#endif



#define EPD_HOST    SPI2_HOST


    #define SPI_PIN_NUM_MOSI GPIO_NUM_23     //EPS32 DEMO
    #define SPI_PIN_NUM_CLK  GPIO_NUM_18     //ESP32 DEMO
    #define EPD_PIN_NUM_CS   GPIO_NUM_27     //ESP32 DEMO
    #define EPD_PIN_NUM_DC   GPIO_NUM_14     //ESP32 DEMO
    #define EPD_PIN_NUM_RST  GPIO_NUM_12     //ESP32 DEMO
    #define EPD_PIN_NUM_BUSY GPIO_NUM_13     //ESP32 DEMO
    #define GT30_PIN_NUM_CS   GPIO_NUM_15     //ESP32 DEMO
    #define SPI_NUM_MISO   GPIO_NUM_19     //ESP32 DEMO


    // #define SPI_PIN_NUM_MOSI GPIO_NUM_14     //EPS32S3 DEMO
    // #define SPI_PIN_NUM_CLK  GPIO_NUM_17     //EPS32S3 DEMO
    // #define EPD_PIN_NUM_CS   GPIO_NUM_45     //EPS32S3 DEMO
    // #define EPD_PIN_NUM_DC   GPIO_NUM_46     //EPS32S3 DEMO
    // #define EPD_PIN_NUM_RST  GPIO_NUM_47     //EPS32S3 DEMO
    // #define EPD_PIN_NUM_BUSY GPIO_NUM_48     //EPS32S3 DEMO






#define isEPD_W21_BUSY gpio_get_level(EPD_PIN_NUM_BUSY)
#define EPD_W21_RST_0 gpio_set_level(EPD_PIN_NUM_RST, 0)
#define EPD_W21_RST_1 gpio_set_level(EPD_PIN_NUM_RST, 1)
#define EPD_W21_DC_0  gpio_set_level(EPD_PIN_NUM_DC, 0)
#define EPD_W21_DC_1  gpio_set_level(EPD_PIN_NUM_DC, 1)
#define EPD_W21_CS_0 gpio_set_level(EPD_PIN_NUM_CS, 0)
#define EPD_W21_CS_1 gpio_set_level(EPD_PIN_NUM_CS, 1)
#define GT30_W21_CS_0 gpio_set_level(GT30_PIN_NUM_CS, 0)
#define GT30_W21_CS_1 gpio_set_level(GT30_PIN_NUM_CS, 1)




void SPI_Write(unsigned char value);
void EPD_W21_WriteDATA(unsigned char datas);
void EPD_W21_WriteCMD(unsigned char command);


#endif 
