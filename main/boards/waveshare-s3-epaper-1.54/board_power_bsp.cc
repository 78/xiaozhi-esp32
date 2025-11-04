#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "board_power_bsp.h"
#include "driver/gpio.h"

void  board_power_bsp::Pwoer_led_Task(void *arg) {
    gpio_config_t gpio_conf = {};                                                            
        gpio_conf.intr_type = GPIO_INTR_DISABLE;                                             
        gpio_conf.mode = GPIO_MODE_OUTPUT;                                                   
        gpio_conf.pin_bit_mask = (0x1ULL << GPIO_NUM_3);
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;                                      
        gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;                                           
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
    for(;;) {
        gpio_set_level(GPIO_NUM_3,0);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(GPIO_NUM_3,1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

board_power_bsp::board_power_bsp(uint8_t _epd_power_pin,uint8_t _audio_power_pin,uint8_t _vbat_power_pin) :
    epd_power_pin(_epd_power_pin),
    audio_power_pin(_audio_power_pin),
    vbat_power_pin(_vbat_power_pin) {
    gpio_config_t gpio_conf = {};                                                            
        gpio_conf.intr_type = GPIO_INTR_DISABLE;                                             
        gpio_conf.mode = GPIO_MODE_OUTPUT;                                                   
        gpio_conf.pin_bit_mask = (0x1ULL << epd_power_pin) | (0x1ULL << audio_power_pin) | (0x1ULL << vbat_power_pin);
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;                                      
        gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;                                           
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
    xTaskCreatePinnedToCore(Pwoer_led_Task, "Pwoer_led_Task", 3 * 1024, NULL , 2, NULL,0); 
}

board_power_bsp::~board_power_bsp() {

}

void board_power_bsp::POWEER_EPD_ON() {
    gpio_set_level((gpio_num_t)epd_power_pin,0);
}

void board_power_bsp::POWEER_EPD_OFF() {
    gpio_set_level((gpio_num_t)epd_power_pin,1);
}

void board_power_bsp::POWEER_Audio_ON() {
    gpio_set_level((gpio_num_t)audio_power_pin,0);
}

void board_power_bsp::POWEER_Audio_OFF() {
    gpio_set_level((gpio_num_t)audio_power_pin,1);
}

void board_power_bsp::VBAT_POWER_ON() {
    gpio_set_level((gpio_num_t)vbat_power_pin,1);
}

void board_power_bsp::VBAT_POWER_OFF() {
    gpio_set_level((gpio_num_t)vbat_power_pin,0);
}