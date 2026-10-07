#ifndef EPD_DISPLAY_2_9_H
#define EPD_DISPLAY_2_9_H

#include "epd_display.h"

// 2.9-inch panel using the C221 25C waveforms.
class EpdDisplay_2_9 : public EpdDisplay {
public:
    using EpdDisplay::EpdDisplay;

protected:
    Waveform GetFullWaveform() const override;
    Waveform GetPartialWaveform() const override;

private:
    static const uint8_t WF_Full[];
    static const uint8_t WF_Partial[];
};

#endif  // EPD_DISPLAY_2_9_H