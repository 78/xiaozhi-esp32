#ifndef __BOARD_POWER_BSP_H__
#define __BOARD_POWER_BSP_H__


class board_power_bsp
{
private:
    const uint8_t epd_power_pin;
    const uint8_t audio_power_pin;
    const uint8_t vbat_power_pin;

    static void Pwoer_led_Task(void *arg);

public:
    board_power_bsp(uint8_t _epd_power_pin,uint8_t _audio_power_pin,uint8_t _vbat_power_pin);
    ~board_power_bsp();

    void POWEER_EPD_ON();
    void POWEER_EPD_OFF();
    void POWEER_Audio_ON();
    void POWEER_Audio_OFF();
    void VBAT_POWER_ON();
    void VBAT_POWER_OFF();
};

#endif