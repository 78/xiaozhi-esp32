#include "iot/thing.h"
#include "AlarmClock.h"
#include "application.h"
#include <esp_log.h>
#if CONFIG_USE_ALARM
#define TAG "AlarmIot"
// extern AlarmManager* alarm_m_;
namespace iot {

// 这里仅定义 AlarmIot 的属性和方法，不包含具体的实现
class AlarmIot : public Thing {
public:
    AlarmIot() : Thing("Alarm", "一个闹钟, 可以定时提醒") {
        // 定义设备的属性
        properties_.AddStringProperty("Alarm_List", "当前闹钟的描述", [this]() -> std::string {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                return std::string("AlarmManager is nullptr");
            }
            ESP_LOGI(TAG, "Alarm_List %s", app.alarm_m_->GetAlarmsStatus().c_str());
            return app.alarm_m_->GetAlarmsStatus();
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetAlarm", "设置一个闹钟", ParameterList({
            Parameter("second_from_now", "闹钟多少秒以后响", kValueTypeNumber, true),
            Parameter("alarm_name", "闹钟的描述(名字)", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                ESP_LOGE(TAG, "AlarmManager is nullptr");
                return;
            }
            ESP_LOGI(TAG, "SetAlarm");
            int second_from_now = parameters["second_from_now"].number();
            std::string alarm_name = parameters["alarm_name"].string();
            ESP_LOGI(TAG, "SetAlarm with name: '%s', seconds: %d", alarm_name.c_str(), second_from_now);
            app.alarm_m_->SetAlarm(second_from_now, alarm_name);
        });

        // 添加取消闹钟方法
        methods_.AddMethod("CancelAlarm", "取消一个闹钟", ParameterList({
            Parameter("alarm_name", "要取消的闹钟名称", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                ESP_LOGE(TAG, "AlarmManager is nullptr");
                return;
            }
            ESP_LOGI(TAG, "CancelAlarm");
            std::string alarm_name = parameters["alarm_name"].string();
            ESP_LOGI(TAG, "CancelAlarm with name: '%s'", alarm_name.c_str());
            app.alarm_m_->CancelAlarm(alarm_name);
            ESP_LOGI(TAG, "CancelAlarm command sent for alarm: %s", alarm_name.c_str());
        });
    }
};

} // namespace iot

DECLARE_THING(AlarmIot);
#endif

