
#include "AlarmClock.h"
#include "assets/lang_config.h"
#include "board.h"
#include "display.h"
#define TAG "AlarmManager"

#if CONFIG_USE_ALARM
Alarm * AlarmManager::GetProximateAlarm(time_t now){
    Alarm *current_alarm_ = nullptr;
    for(auto& alarm : alarms_){
        if(alarm.time > now && (current_alarm_ == nullptr || alarm.time < current_alarm_->time)){
            current_alarm_ = &alarm; // 获取当前时间以后第一个发生的时钟句柄
        }
    }
    return current_alarm_;
}
/// @brief 删除超过时间的所有时钟
/// @param now 
void AlarmManager::ClearOverdueAlarm(time_t now){
    std::lock_guard<std::mutex> lock(mutex_);
    Settings settings_("alarm_clock", true); // 闹钟设置(硬盘存储)
    for(auto it = alarms_.begin(); it != alarms_.end();){
        if(it->time <= now){
            for (int i = 0; i < 10; i++){
                if(settings_.GetString("alarm_" + std::to_string(i)) == it->name && settings_.GetInt("alarm_time_" + std::to_string(i)) == it->time){
                    settings_.SetString("alarm_" + std::to_string(i), "");
                    settings_.SetInt("alarm_time_" + std::to_string(i), 0);
                    ESP_LOGI(TAG, "Alarm %s at %d is overdue", it->name.c_str(), it->time);
                }
            }
            it = alarms_.erase(it); // 删除过期的闹钟, 此时it指向下一个元素
        }else{
            it++;
        }
    }
}

AlarmManager::AlarmManager(){
    ESP_LOGI(TAG, "AlarmManager init");
    ring_flag = false;
    running_flag = false;


    Settings settings_("alarm_clock", true); // 闹钟设置
    // 从Setting里面读取闹钟列表
    for(int i = 0; i < 10; i++){
        std::string alarm_name = settings_.GetString("alarm_" + std::to_string(i));
        if(alarm_name != ""){
            Alarm alarm;
            alarm.name = alarm_name;
            alarm.time = settings_.GetInt("alarm_time_" + std::to_string(i));
            alarms_.push_back(alarm);
            ESP_LOGI(TAG, "Alarm %s add agein at %d", alarm.name.c_str(), alarm.time);
        }
    }

    // 建立一个时钟
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            AlarmManager* alarm_manager = (AlarmManager*)arg;
            alarm_manager->OnAlarm(); // 闹钟响了
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alarm_timer"
    };
    esp_timer_create(&timer_args, &timer_);
    time_t now = time(NULL);
    // 获取最近的闹钟, 同时清除过期的闹钟
    printf("now: %lld\n", now);

    ClearOverdueAlarm(now);

    Alarm *current_alarm_ = GetProximateAlarm(now);
    // 启动闹钟
    if(current_alarm_ != nullptr){
        int new_timer_time = current_alarm_->time - now;
        ESP_LOGI(TAG, "begin a alarm at %d", new_timer_time);
        esp_timer_start_once(timer_, new_timer_time * 1000000);
        running_flag = true;
    }
}


AlarmManager::~AlarmManager(){
    if(timer_ != nullptr){
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
}


void AlarmManager::SetAlarm(int seconde_from_now, std::string alarm_name){
    std::lock_guard<std::mutex> lock(mutex_);
    if(alarms_.size() >= 10){
        ESP_LOGE(TAG, "Too many alarms");
        return;
    }
    if(seconde_from_now <= 0){
        ESP_LOGE(TAG, "Invalid alarm time");
        return;
    }

    Settings settings_("alarm_clock", true); // 闹钟设置
    Alarm alarm; // 一个新的闹钟
    alarm.name = alarm_name;
    time_t now = time(NULL);
    alarm.time = now + seconde_from_now;
    alarms_.push_back(alarm);
    // 从设置里面找到第一个空闲的闹钟, 记录新的闹钟
    for(int i = 0; i < 10; i++){
        if(settings_.GetString("alarm_" + std::to_string(i)) == ""){
            settings_.SetString("alarm_" + std::to_string(i), alarm_name);
            settings_.SetInt("alarm_time_" + std::to_string(i), alarm.time);
            break;
        }
    }

    Alarm *alarm_first = GetProximateAlarm(now);
    ESP_LOGI(TAG, "Alarm %s set at %d, now first %d", alarm.name.c_str(), alarm.time, alarm_first->time);
    if(running_flag == true){
        esp_timer_stop(timer_);
    }

    seconde_from_now = alarm_first->time - now;
    ESP_LOGI(TAG, "begin a alarm at %d", seconde_from_now);
    esp_timer_start_once(timer_, seconde_from_now * 1000000); // 当前一定有时钟, 所以不需要清除标志
}

void AlarmManager::OnAlarm(){
    ESP_LOGI(TAG, "=----ring----=");
    ring_flag = true;
    auto display = Board::GetInstance().GetDisplay();
    // 遍历闹钟
    Alarm *alarm_first = nullptr;
    for(auto& alarm : alarms_){
        if(alarm.time <= time(NULL)){
            alarm_first = &alarm;
            break;
        }
    }

    display->SetStatus(alarm_first->name.c_str());  // 显示闹钟名字
    // // 闹钟响了
    time_t now = time(NULL);
    // 处理一下相同时间的闹钟
    ClearOverdueAlarm(now);

    Alarm *current_alarm_ = GetProximateAlarm(now);
    if(current_alarm_ != nullptr){
        int new_timer_time = current_alarm_->time - now;
        ESP_LOGI(TAG, "begin a alarm at %d", new_timer_time);
        esp_timer_start_once(timer_, new_timer_time * 1000000);
    }else{
        running_flag = false; // 没有闹钟了
        ESP_LOGI(TAG, "no alarm now");
    }

}

std::string AlarmManager::GetAlarmsStatus(){
    std::lock_guard<std::mutex> lock(mutex_);
    std::string status;
    for(auto& alarm : alarms_){
        status += alarm.name + " at " + std::to_string(alarm.time) + "\n";
    }
    return status;
}

#endif