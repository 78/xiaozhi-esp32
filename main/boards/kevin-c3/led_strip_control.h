#ifndef LED_STRIP_CONTROL_H
#define LED_STRIP_CONTROL_H

#include "iot/thing.h"
#include "led/circular_strip.h"

using namespace iot;

class LedStripControl : public Thing {
private:
    CircularStrip* led_strip_;
    int brightness_level_;  // 亮度等级 (0-8)

    int LevelToBrightness(int level) const;  // 将等级转换为实际亮度值
    StripColor RGBToColor(int red, int green, int blue);

public:
    explicit LedStripControl(CircularStrip* led_strip);
}; 

#endif // LED_STRIP_CONTROL_H
