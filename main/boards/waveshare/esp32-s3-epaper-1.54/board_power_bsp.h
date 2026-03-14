#ifndef __BOARD_POWER_BSP_H__
#define __BOARD_POWER_BSP_H__

class BoardPowerBsp {
private:
    const int epdPowerPin_;
    const int audioPowerPin_;
    const int vbatPowerPin_;

    static void PowerLedTask(void *arg);

public:
    BoardPowerBsp(int epdPowerPin, int audioPowerPin, int vbatPowerPin);
    ~BoardPowerBsp();
    void PowerEpdOn();
    void PowerEpdOff();
    void PowerAudioOn();
    void PowerAudioOff();
    void VbatPowerOn();
    void VbatPowerOff();
};

#endif