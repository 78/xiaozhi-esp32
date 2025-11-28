#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include <cstring>
#include "esp_log.h"
#include <cstdint>
#include "driver/spi_master.h"
static spi_device_handle_t spi;
static const char *TAG = "EPD_DEMO";

void init_gpio(uint64_t gpio_num, gpio_mode_t gpio_mode)
{
        gpio_config_t gpio_config_cfg = {};
        gpio_config_cfg.pin_bit_mask = (1ULL<<gpio_num);
        gpio_config_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
        gpio_config_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_config_cfg.mode = gpio_mode;
        gpio_config_cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&gpio_config_cfg);
}


void EPD_spi_init(void)
{
    esp_err_t ret;
    spi_bus_config_t buscfg={};
        buscfg.mosi_io_num=SPI_PIN_NUM_MOSI;
        buscfg.miso_io_num=-1;
        buscfg.sclk_io_num=SPI_PIN_NUM_CLK;
        buscfg.quadwp_io_num=-1;
        buscfg.quadhd_io_num=-1;
       // buscfg.max_transfer_sz = EPD_ARRAY; 

    spi_device_interface_config_t devcfg={};
        devcfg.clock_speed_hz=10*1000*1000;          //Clock out at 10 MHz
        devcfg.mode=0;                                //SPI mode 0
        devcfg.spics_io_num=EPD_PIN_NUM_CS;               //CS pin
        devcfg.queue_size=7;     
                             //We want to be able to queue 7 transactions at a time
    //Initialize the SPI bus
    ret=spi_bus_initialize(EPD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK( ret );
    ESP_LOGW(TAG, "INIT: %d", ret);
    //assert(ret==ESP_OK);
    //Attach the EPD to the SPI bus
    ret=spi_bus_add_device(EPD_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK( ret );
    ESP_LOGW(TAG, "ADD: %d", ret);
    //assert(ret==ESP_OK);
}

//SPI write byte
void SPI_Write(uint8_t value)
{   
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&value;               //The data is the command itself
    ret=spi_device_transmit(spi, &t);  //Transmit!
   // ESP_ERROR_CHECK( ret );
   // ESP_LOGW(TAG, "spi_write: %d", value);
   // vTaskDelay(pdMS_TO_TICKS(1000));
    //assert(ret==ESP_OK);            //Should have had no issues.4
    if (ret != ESP_OK) {
    ESP_LOGE("SPI", "Transmit failed: %s", esp_err_to_name(ret));
}
}

//SPI write command
void EPD_W21_WriteCMD(uint8_t command)
{
	EPD_W21_CS_0;
	EPD_W21_DC_0;  // D/C#   0:command  1:data  
	SPI_Write(command);
	EPD_W21_CS_1;
}
//SPI write data
void EPD_W21_WriteDATA(uint8_t datas)
{
	EPD_W21_CS_0;
	EPD_W21_DC_1;  // D/C#   0:command  1:data
	SPI_Write(datas);
	EPD_W21_CS_1;
}
