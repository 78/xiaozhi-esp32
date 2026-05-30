#include "dooi_display.h"
#include <esp_log.h>
#include "app_ui.h"
#include "assets/lang_config.h"

#include "app_ui_logic.h"

#define TAG "DooiDisplay"

extern "C" {
    void wakeup_ack_set_topic(const char* topic);
    void wakeup_ack_add_notification(const char* notification);
}

DooiDisplay::DooiDisplay() {
    ESP_LOGI(TAG, "DooiDisplay initialized");
}

DooiDisplay::~DooiDisplay() {
}

bool DooiDisplay::Lock(int timeout_ms) {
    return true;
}

void DooiDisplay::Unlock() {
}

void DooiDisplay::SetStatus(const char* status) {
    ESP_LOGW(TAG, "SetStatus: %s", status);
    if (status && strlen(status) > 0) 
    {
        app_event_t evt = {.type = APP_EVT_NONE};

        if (strcmp(status, Lang::Strings::LISTENING) == 0) {
            evt.type = APP_EVT_STATE_LISTENING;
        } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
            evt.type = APP_EVT_STATE_STANDBY;
        } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
            evt.type = APP_EVT_STATE_SPEAKING;
        } else if (strcmp(status, Lang::Strings::CONNECTING) == 0) {
            evt.type = APP_EVT_STATE_CONNECTING;
        }  else if(strcmp(status, Lang::Strings::LOADING_PROTOCOL) == 0){
            evt.type = APP_EVT_STATE_LOADING_PROTOCOL;
        }else if(strcmp(status, Lang::Strings::ACTIVATION) == 0){
            evt.type = APP_EVT_STATE_ACTIVATION;
        }else if(strcmp(status, Lang::Strings::CHECKING_NEW_VERSION) == 0){
            evt.type = APP_EVT_STATE_CHECKING_NEW_VERSION;
        }else if (strcmp(status, Lang::Strings::ERROR) == 0) {
            evt.type = APP_EVT_STATE_ERROR;
        }

        app_ui_logic_post(&evt, false);
    }
}

void DooiDisplay::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void DooiDisplay::ShowNotification(const char* notification, int duration_ms) {
    ESP_LOGW(TAG, "ShowNotification: %s", notification);

    if(strcmp(notification, Lang::Strings::SCANNING_WIFI) == 0)
    {
        app_event_t evt = {.type = APP_EVT_NONE};
        evt.type = APP_EVT_STATE_CONNECT_WIFI;
        app_ui_logic_post(&evt, false);
    }

    if(app_ui_logic_get_state() != APP_EVT_STATE_LOADING_PROTOCOL)
    {
        wakeup_ack_set_topic("SYS MESSAGE");
        wakeup_ack_add_notification(notification);
        screen_set_current(SCREEN_WAKEUP_ACK);
    }
}

void DooiDisplay::UpdateStatusBar(bool update_all) {
}

void DooiDisplay::SetEmotion(const char* emotion) {
    ESP_LOGW(TAG, "SetEmotion: %s", emotion);

    ui_emoji_set_expression(emotion);

}

void DooiDisplay::SetChatMessage(const char* role, const char* content) {
    if(strcmp(role, "user") == 0)
    {
        wakeup_ack_set_topic("USER MESSAGE");
        wakeup_ack_add_notification(content);
        screen_set_current(SCREEN_WAKEUP_ACK);

    }
    if(app_ui_logic_get_state() == APP_EVT_STATE_ACTIVATION)
    {
        wakeup_ack_set_topic("SYS MESSAGE");
        wakeup_ack_add_notification(content);
        screen_set_current(SCREEN_WAKEUP_ACK);
    }
    ESP_LOGW(TAG, "Role:%s", role);
    ESP_LOGW(TAG, "     %s", content);
}

void DooiDisplay::SetTheme(Theme* theme) {
    ESP_LOGI(TAG, "Theme changed to: %s", theme->name().c_str());
}

void DooiDisplay::SetPowerSaveMode(bool on) {
    ESP_LOGW(TAG, "SetPowerSaveMode: %d", on);
}


void DooiDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    ESP_LOGI(TAG, "SetPreviewImage called with image size: %d bytes", image->image_dsc()->data_size);
}