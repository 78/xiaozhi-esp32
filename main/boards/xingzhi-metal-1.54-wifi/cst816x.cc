#include "cst816x.h"
#include "board.h"
#include "application.h"
#include "display.h"
#include "assets/lang_config.h"
// #include "audio_codec.h"
#include "wifi_board.h"
#include <wifi_station.h>
#include "power_save_timer.h"
#include "codecs/es8311_audio_codec.h"
#include <algorithm>  // 用于std::max/std::min

#define TAG "Cst816x"

const Cst816x::TouchThresholdConfig& Cst816x::getThresholdConfig(int x, int y) {
    for (const auto& config : TOUCH_THRESHOLD_TABLE) {
        if (config.x == x && config.y == y) {
            return config;
        }
    }
    return DEFAULT_THRESHOLD;
}

Cst816x::Cst816x(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
    uint8_t chip_id = ReadReg(0xA7);
    ESP_LOGI(TAG, "Get CST816x chip ID: 0x%02X", chip_id);
    read_buffer_ = new uint8_t[6];
}

Cst816x::~Cst816x() {
    if (read_buffer_ != nullptr) {
        delete[] read_buffer_;
        read_buffer_ = nullptr;
    }
}

int64_t Cst816x::getCurrentTimeUs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);  
    return (int64_t)tv.tv_sec * 1000000L + tv.tv_usec;
}

void Cst816x::UpdateTouchPoint() {
    ReadRegs(0x02, read_buffer_, 6);
    tp_.num = read_buffer_[0] & 0x0F;
    tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
    tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
    memset(read_buffer_, 0, 6);
}

void Cst816x::resetTouchCounters() {
    is_touching_ = false;
    touch_start_time_ = 0;
    last_release_time_ = 0;
    click_count_ = 0;
    long_press_started_ = false;

    is_volume_long_pressing_ = false;
    volume_long_press_dir_ = 0;
    last_volume_adjust_time_ = 0;
}

void Cst816x::touchpad_daemon(void* arg) {
    Cst816x* cst816x = static_cast<Cst816x*>(arg);  
    auto& board = Board::GetInstance();             
    auto codec = board.GetAudioCodec();             
    auto display = board.GetDisplay();             

    while (1) {
        cst816x->UpdateTouchPoint();
        auto& tp = cst816x->GetTouchPoint(); 
        int64_t current_time = cst816x->getCurrentTimeUs(); 

        const auto& config = cst816x->getThresholdConfig(tp.x, tp.y);
        if (tp.num > 0) {
            ESP_LOGD(TAG, "Touch at (%d,%d) → SingleThresh:%lldms, DoubleWindow:%lldms, LongThresh:%lldms",
                tp.x, tp.y,
                config.single_click_thresh_us / 1000,
                config.double_click_window_us / 1000,
                config.long_press_thresh_us / 1000);
        }

        TouchEvent current_event;  
        bool event_detected = false;

        if (tp.num > 0 && !cst816x->is_touching_) {
            cst816x->is_touching_ = true;
            cst816x->touch_start_time_ = current_time;  
            cst816x->long_press_started_ = false;
        }
        else if (tp.num > 0 && cst816x->is_touching_) {
            if (!cst816x->long_press_started_ && 
                (current_time - cst816x->touch_start_time_ >= config.long_press_thresh_us)) {
                current_event = {TouchEventType::LONG_PRESS_START, tp.x, tp.y};
                event_detected = true;
                cst816x->long_press_started_ = true;
            }
        }
        else if (tp.num == 0 && cst816x->is_touching_) {
            cst816x->is_touching_ = false;
            int64_t touch_duration = current_time - cst816x->touch_start_time_;
            cst816x->last_release_time_ = current_time;
            if (cst816x->long_press_started_) {
                current_event = {TouchEventType::LONG_PRESS_END, tp.x, tp.y};
                event_detected = true;
            }
            else if (touch_duration <= config.single_click_thresh_us) {
                cst816x->click_count_++;
            }
        }
        else if (tp.num == 0 && !cst816x->is_touching_) {
            if (cst816x->click_count_ > 0 && 
                (current_time - cst816x->last_release_time_ >= config.double_click_window_us)) {
                if (cst816x->click_count_ == 2) {
                    current_event = {TouchEventType::DOUBLE_CLICK, tp.x, tp.y};
                    event_detected = true;
                }
                else if (cst816x->click_count_ == 1) {
                    current_event = {TouchEventType::SINGLE_CLICK, tp.x, tp.y};
                    event_detected = true;
                }
                cst816x->click_count_ = 0; 
            }
        }

        if (event_detected) {
            if (current_event.y == 600 && (current_event.x == 20 || current_event.x == 40 || current_event.x == 60)) {
                switch (current_event.type) {
                    case TouchEventType::SINGLE_CLICK:
                        if (current_event.x == 40) {
                            board.SetPowerSaveMode(false);
                            auto& app = Application::GetInstance();
                            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                                auto& wifi_board = static_cast<WifiBoard&>(Board::GetInstance());
                                wifi_board.ResetWifiConfiguration();
                            }
                            app.ToggleChatState();  
                        } else if (current_event.x == 20) {     // 20,600 单击：音量+
                            int current_vol = codec->output_volume();
                            int new_vol = current_vol + 10;
                            new_vol = (new_vol >= ES8311_VOL_MAX) ? ES8311_VOL_MAX : new_vol;
                            ESP_LOGI(TAG, "current_vol, new_vol(%d, %d)", current_vol, new_vol);
                            codec->EnableOutput(true);
                            codec->SetOutputVolume(new_vol);
                            display->ShowNotification(Lang::Strings::VOLUME + std::to_string(new_vol));
                        } else if (current_event.x == 60) {     // 60,600 单击：音量-
                            int current_vol = codec->output_volume();
                            int new_vol = current_vol - 10;
                            new_vol = (new_vol <= ES8311_VOL_MIN) ? ES8311_VOL_MIN : new_vol;
                            ESP_LOGI(TAG, "current_vol, new_vol(%d, %d)", current_vol, new_vol);
                            codec->EnableOutput(true);
                            codec->SetOutputVolume(new_vol);
                            display->ShowNotification(Lang::Strings::VOLUME + std::to_string(new_vol));
                        }
                        break;

                    case TouchEventType::DOUBLE_CLICK:
                        ESP_LOGI(TAG, "Double click detected at (%d, %d)", current_event.x, current_event.y);
                        break;

                    case TouchEventType::LONG_PRESS_START:
                        ESP_LOGI(TAG, "Long press started at (%d, %d) → Start volume adjust", current_event.x, current_event.y);
                        if (current_event.x == 20) {
                            cst816x->is_volume_long_pressing_ = true;
                            cst816x->volume_long_press_dir_ = 1;
                            cst816x->last_volume_adjust_time_ = current_time;
                        } else if (current_event.x == 60) {
                            cst816x->is_volume_long_pressing_ = true;
                            cst816x->volume_long_press_dir_ = -1; 
                            cst816x->last_volume_adjust_time_ = current_time;
                        }
                        break;

                    case TouchEventType::LONG_PRESS_END:
                        ESP_LOGI(TAG, "Long press ended at (%d, %d) → Stop volume adjust", current_event.x, current_event.y);
                        if (current_event.x == 20 || current_event.x == 60) {
                            cst816x->is_volume_long_pressing_ = false;
                            cst816x->volume_long_press_dir_ = 0;
                            cst816x->last_volume_adjust_time_ = 0;
                        }
                        break;
                }
            }
        }

        if (cst816x->is_volume_long_pressing_) {
            int64_t now = cst816x->getCurrentTimeUs();
            if (now - cst816x->last_volume_adjust_time_ >= cst816x->VOL_ADJ_INTERVAL_US) {
                int current_vol = codec->output_volume();
                int new_vol = current_vol + (cst816x->volume_long_press_dir_ * cst816x->VOL_ADJ_STEP);
                
                new_vol = std::max(ES8311_VOL_MIN, std::min(ES8311_VOL_MAX, new_vol));
                
                if (new_vol != current_vol) {
                    codec->EnableOutput(true);
                    codec->SetOutputVolume(new_vol);
                    display->ShowNotification(Lang::Strings::VOLUME + std::to_string(new_vol));
                    cst816x->last_volume_adjust_time_ = now; 
                } else {
                    cst816x->is_volume_long_pressing_ = false;
                    cst816x->volume_long_press_dir_ = 0;
                    ESP_LOGI(TAG, "Volume reached limit (%d), stop adjusting", new_vol);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void Cst816x::InitCst816d() {
    ESP_LOGI(TAG, "Init CST816x touch driver");
    xTaskCreate(touchpad_daemon, "touch_daemon", 2048, this, 1, NULL);
}