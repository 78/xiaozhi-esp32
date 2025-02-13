#ifndef _NO_DISPLAY_H_
#define _NO_DISPLAY_H_

#include "display.h"

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
    virtual void SetBacklight(uint8_t brightness) override {
        // 空实现，因为这是一个无显示设备
    }
public:
    NoDisplay();
    ~NoDisplay();
};

#endif
