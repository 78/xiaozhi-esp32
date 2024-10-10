/**
 * @file STMPE610.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "stmpe610.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "tp_spi.h"
#include <stddef.h>

/*********************
 *      DEFINES
 *********************/
#define TAG        "STMPE610"


/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void write_8bit_reg(uint8_t reg, uint8_t val);
static uint16_t read_16bit_reg(uint8_t reg);
static uint8_t read_8bit_reg(uint8_t reg);
static void read_data(int16_t *x, int16_t *y, uint8_t *z);
static bool buffer_empty();
static void adjust_data(int16_t * x, int16_t * y);


/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize the STMPE610
 */
void stmpe610_init(void)
{
	uint8_t u8;
	uint16_t u16;
	
	ESP_LOGI(TAG, "Initialization.");

	// Get the initial SPI configuration
	//u8 = read_8bit_reg(STMPE_SPI_CFG);
	//ESP_LOGI(TAG, "SPI_CFG = 0x%x", u8);
	
    // Attempt a software reset
	write_8bit_reg(STMPE_SYS_CTRL1, STMPE_SYS_CTRL1_RESET);
	vTaskDelay(10 / portTICK_RATE_MS);
	
	// Reset the SPI configuration, making sure auto-increment is set
	u8 = read_8bit_reg(STMPE_SPI_CFG);
	write_8bit_reg(STMPE_SPI_CFG, u8 | STMPE_SPI_CFG_AA);
	u8 = read_8bit_reg(STMPE_SPI_CFG);
	ESP_LOGI(TAG, "SPI_CFG = 0x%x", u8);
	
	// Verify SPI communication
	u16 = read_16bit_reg(STMPE_CHIP_ID);
	if (u16 != 0x811) {
		ESP_LOGE(TAG, "Incorrect version: 0x%x", u16);
	}

	write_8bit_reg(STMPE_SYS_CTRL2, 0x00); // Disable clocks
	write_8bit_reg(STMPE_TSC_CTRL, 0);     // Disable to allow writing
	
	write_8bit_reg(STMPE_TSC_CTRL,
				   STEMP_TSC_CTRL_TRACK_0 | 
                   STMPE_TSC_CTRL_XYZ |
                   STMPE_TSC_CTRL_EN);
	
	write_8bit_reg(STMPE_TSC_CFG, STMPE_TSC_CFG_4SAMPLE |
				   STMPE_TSC_CFG_DELAY_1MS |
                   STMPE_TSC_CFG_SETTLE_1MS);
                   
	write_8bit_reg(STMPE_TSC_FRACTION_Z, 0x7);
	write_8bit_reg(STMPE_TSC_I_DRIVE, STMPE_TSC_I_DRIVE_50MA);
	
	write_8bit_reg(STMPE_SYS_CTRL2, 0x04); // GPIO clock off, TSC clock on, ADC clock on
	
	write_8bit_reg(STMPE_ADC_CTRL1, STMPE_ADC_CTRL1_12BIT | STMPE_ADC_CTRL1_80CLK);
	write_8bit_reg(STMPE_ADC_CTRL2, STMPE_ADC_CTRL2_3_25MHZ);
	
	write_8bit_reg(STMPE_GPIO_ALT_FUNCT, 0x00);  // Disable GPIO

	write_8bit_reg(STMPE_FIFO_TH, 1);                      // Set FIFO threshold
	write_8bit_reg(STMPE_FIFO_STA, STMPE_FIFO_STA_RESET);  // Assert FIFO reset
	write_8bit_reg(STMPE_FIFO_STA, 0);                     // Deassert FIFO reset
	
	write_8bit_reg(STMPE_INT_EN, 0x00);  // No interrupts
	write_8bit_reg(STMPE_INT_STA, 0xFF); // reset all ints
}

/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 * @return false: because no more data to be read
 */
bool stmpe610_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    static int16_t last_x = 0;
    static int16_t last_y = 0;
    bool valid = true;
	int c = 0;
    int16_t x = 0;
    int16_t y = 0;
    uint8_t z;

    if ((read_8bit_reg(STMPE_TSC_CTRL) & STMPE_TSC_TOUCHED) == STMPE_TSC_TOUCHED) {
		// Making sure that we read all data and return the latest point
		while (!buffer_empty()) {
			read_data(&x, &y, &z);
			c++;
		}
		
		if (c > 0) {
			//ESP_LOGI(TAG, "%d: %d %d %d", c, x, y, z);
		
			adjust_data(&x, &y);
    		last_x = x;
    		last_y = y;
    		//ESP_LOGI(TAG, "  ==> %d %d", x, y);
    	}
    	
    	z = read_8bit_reg(STMPE_INT_STA);  // Clear interrupts
    	z = read_8bit_reg(STMPE_FIFO_STA);
    	if ((z & STMPE_FIFO_STA_OFLOW) == STMPE_FIFO_STA_OFLOW) {
    		// Clear the FIFO if we discover an overflow
    		write_8bit_reg(STMPE_FIFO_STA, STMPE_FIFO_STA_RESET);
			write_8bit_reg(STMPE_FIFO_STA, 0); // unreset
			ESP_LOGE(TAG, "Fifo overflow");
		}
    }
    
    if (c == 0) {
        x = last_x;
        y = last_y;
        valid = false;
    }

    data->point.x = (int16_t) x;
    data->point.y = (int16_t) y;
    data->state = valid == false ? LV_INDEV_STATE_REL : LV_INDEV_STATE_PR;

    return false;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/
static void write_8bit_reg(uint8_t reg, uint8_t val)
{
	uint8_t data_send[2];
	
	data_send[0] = reg;
	data_send[1] = val;
	tp_spi_write_reg(data_send, 2);
}


static uint16_t read_16bit_reg(uint8_t reg)
{
	uint8_t data_recv[2];
	
	tp_spi_read_reg(0x80 | reg, data_recv, 2);
	
	return data_recv[0] << 8 | data_recv[1];
}


static uint8_t read_8bit_reg(uint8_t reg)
{	
	uint8_t data_recv;
	
	tp_spi_read_reg(0x80 | reg, &data_recv, 1);
	
	return data_recv;
}


static void read_data(int16_t *x, int16_t *y, uint8_t *z)
{
	*x = read_16bit_reg(STMPE_TSC_DATA_X);
	*y = read_16bit_reg(STMPE_TSC_DATA_Y);
	*z = read_8bit_reg(STMPE_TSC_DATA_Z);
}


static bool buffer_empty()
{
	return ((read_8bit_reg(STMPE_FIFO_STA) & STMPE_FIFO_STA_EMPTY) == STMPE_FIFO_STA_EMPTY);
}


static void adjust_data(int16_t * x, int16_t * y)
{
#if STMPE610_XY_SWAP != 0
    int16_t swap_tmp;
    swap_tmp = *x;
    *x = *y;
    *y = swap_tmp;
#endif

    if((*x) > STMPE610_X_MIN)(*x) -= STMPE610_X_MIN;
    else(*x) = 0;

    if((*y) > STMPE610_Y_MIN)(*y) -= STMPE610_Y_MIN;
    else(*y) = 0;

    (*x) = (uint32_t)((uint32_t)(*x) * LV_HOR_RES) /
           (STMPE610_X_MAX - STMPE610_X_MIN);

    (*y) = (uint32_t)((uint32_t)(*y) * LV_VER_RES) /
           (STMPE610_Y_MAX - STMPE610_Y_MIN);

#if STMPE610_X_INV != 0
    (*x) =  LV_HOR_RES - (*x);
#endif

#if STMPE610_Y_INV != 0
    (*y) =  LV_VER_RES - (*y);
#endif

}

