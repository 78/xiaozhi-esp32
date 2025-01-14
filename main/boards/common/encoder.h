#ifndef ENCODER_H_
#define ENCODER_H_

#include <driver/pulse_cnt.h>
// #include <iot_encoder.h>
#include <functional>

class Encoder {
public:
    Encoder(int gpio_pcnt1, int gpio_pcnt2, int _low_limit = -1000, int _high_limit = 1000);
    ~Encoder();

    void OnPcntReach(std::function<void(int)> callback);
private:
    int gpio_pcnt1_;
    int gpio_pcnt2_;
    pcnt_unit_handle_t pcnt_unit_ = NULL;
    
    std::function<void(int)> on_pcnt_reach_;
};

#endif // ENCODER_H_
