#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H

#include "display.h"

class LedDisplay : public Display {
private:

public:
    LedDisplay();
    ~LedDisplay();
    virtual void SetEmotion(const char* emotion) override;
};

#endif // LED_DISPLAY_H
