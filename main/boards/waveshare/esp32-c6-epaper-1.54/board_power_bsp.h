#ifndef __BOARD_POWER_BSP_H__
#define __BOARD_POWER_BSP_H__

#include <stdint.h>
#include "esp_io_expander.h"

class BoardPowerBsp {
private:
    esp_io_expander_handle_t io_expander_;
    const uint32_t epdPowerPin_;
    const uint32_t audioPowerPin_;
    const uint32_t vbatPowerPin_;
    const uint32_t paCtrlPin_;
    const uint32_t ledPin_;

    static void PowerLedTask(void* arg);

public:
    BoardPowerBsp(esp_io_expander_handle_t io_expander, uint32_t epdPowerPin,
                  uint32_t audioPowerPin, uint32_t vbatPowerPin, uint32_t paCtrlPin,
                  uint32_t ledPin);
    ~BoardPowerBsp();
    void PowerEpdOn();
    void PowerEpdOff();
    void PowerAudioOn();
    void PowerAudioOff();
    void VbatPowerOn();
    void VbatPowerOff();
};

#endif
