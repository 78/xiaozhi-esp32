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
                    ESP_LOGI(TAG, "Alarm %s at %lld is overdue", it->name.c_str(), (long long)it->time);
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
            ESP_LOGI(TAG, "Alarm %s add agein at %lld", alarm.name.c_str(), (long long)alarm.time);
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


void AlarmManager::SetAlarm(int second_from_now, std::string alarm_name){
    std::lock_guard<std::mutex> lock(mutex_);
    if(second_from_now <= 0){
        ESP_LOGE(TAG, "Invalid alarm time");
        return;
    }

    Settings settings_("alarm_clock", true); // 闹钟设置
    time_t now = time(NULL);
    time_t new_time = now + second_from_now;
    
    // 检查是否已存在同名闹钟
    bool found_existing = false;
    for(auto& alarm : alarms_){
        if(alarm.name == alarm_name){
            ESP_LOGI(TAG, "Found existing alarm with name: %s, updating time from %lld to %lld", 
                     alarm_name.c_str(), (long long)alarm.time, (long long)new_time);
            
            // 更新现有闹钟的时间
            time_t old_time = alarm.time;
            alarm.time = new_time;
            
            // 更新存储中的时间
            for(int i = 0; i < 10; i++){
                if(settings_.GetString("alarm_" + std::to_string(i)) == alarm_name && 
                   settings_.GetInt("alarm_time_" + std::to_string(i)) == old_time){
                    settings_.SetInt("alarm_time_" + std::to_string(i), new_time);
                    ESP_LOGI(TAG, "Updated stored alarm time for %s", alarm_name.c_str());
                    break;
                }
            }
            found_existing = true;
            break;
        }
    }
    
    // 如果没有找到同名闹钟，创建新闹钟
    if(!found_existing){
        if(alarms_.size() >= 10){
            ESP_LOGE(TAG, "Too many alarms");
            return;
        }
        
        Alarm alarm; // 一个新的闹钟
        alarm.name = alarm_name;
        alarm.time = new_time;
        alarms_.push_back(alarm);
        
        // 从设置里面找到第一个空闲的闹钟, 记录新的闹钟
        for(int i = 0; i < 10; i++){
            if(settings_.GetString("alarm_" + std::to_string(i)) == ""){
                settings_.SetString("alarm_" + std::to_string(i), alarm_name);
                settings_.SetInt("alarm_time_" + std::to_string(i), alarm.time);
                ESP_LOGI(TAG, "Created new alarm: %s at %lld", alarm_name.c_str(), (long long)alarm.time);
                break;
            }
        }
    }

    Alarm *alarm_first = GetProximateAlarm(now);
    if(alarm_first != nullptr){
        ESP_LOGI(TAG, "Next alarm: %s at %lld", alarm_first->name.c_str(), (long long)alarm_first->time);
        if(running_flag == true){
            esp_timer_stop(timer_);
        }

        int seconds_from_now = alarm_first->time - now;
        ESP_LOGI(TAG, "Setting timer for %d seconds", seconds_from_now);
        esp_timer_start_once(timer_, seconds_from_now * 1000000); // 当前一定有时钟, 所以不需要清除标志
        running_flag = true;
    }
}

void AlarmManager::CancelAlarm(std::string alarm_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    Settings settings_("alarm_clock", true); // 闹钟设置
    bool found = false;
    
    ESP_LOGI(TAG, "开始取消闹钟: %s", alarm_name.c_str());
    
    // 从内存中删除指定名称的闹钟
    for(auto it = alarms_.begin(); it != alarms_.end();) {
        if(it->name == alarm_name) {
            // 从存储中删除相应的闹钟设置
            for (int i = 0; i < 10; i++) {
                if(settings_.GetString("alarm_" + std::to_string(i)) == alarm_name) {
                    settings_.SetString("alarm_" + std::to_string(i), "");
                    settings_.SetInt("alarm_time_" + std::to_string(i), 0);
                    ESP_LOGI(TAG, "从设置中移除闹钟 %s", alarm_name.c_str());
                }
            }
            ESP_LOGI(TAG, "从内存中移除闹钟: %s (时间: %lld)", alarm_name.c_str(), (long long)it->time);
            it = alarms_.erase(it); // 删除闹钟并更新迭代器
            found = true;
        } else {
            ++it;
        }
    }
    
    if (!found) {
        ESP_LOGW(TAG, "未找到名为 %s 的闹钟", alarm_name.c_str());
        return;
    }
    
    // 输出所有剩余闹钟的信息，用于调试
    ESP_LOGI(TAG, "剩余闹钟列表:");
    for(auto& alarm : alarms_) {
        ESP_LOGI(TAG, "  - %s (时间: %lld)", alarm.name.c_str(), (long long)alarm.time);
    }
    
    // 重置定时器，使其指向下一个闹钟
    if(running_flag) {
        esp_timer_stop(timer_);
        ESP_LOGI(TAG, "停止当前定时器");
    }
    
    time_t now = time(NULL);
    Alarm *alarm_first = GetProximateAlarm(now);
    
    if(alarm_first != nullptr) {
        int seconds_from_now = alarm_first->time - now;
        ESP_LOGI(TAG, "重置定时器指向下一个闹钟: %s，将在 %d 秒后触发", 
                 alarm_first->name.c_str(), seconds_from_now);
        esp_timer_start_once(timer_, seconds_from_now * 1000000);
        running_flag = true;
    } else {
        ESP_LOGI(TAG, "取消后没有更多闹钟了");
        running_flag = false;
    }
    
    ESP_LOGI(TAG, "闹钟 %s 已成功取消", alarm_name.c_str());
}

#include "application.h"
void AlarmManager::OnAlarm(){
    ESP_LOGI(TAG, "=----闹钟触发----=");
    ring_flag = true;
    
    // 闹钟触发后，CustomBoard 的监听器会自动检测并处理超级省电模式唤醒
    
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    // 找到当前触发的闹钟
    Alarm *alarm_first = nullptr;
    for(auto& alarm : alarms_){
        if(alarm.time <= time(NULL)){
            alarm_first = &alarm;
            break;
        }
    }
    
    if (alarm_first) {
        char message_buf_send[256];
        sprintf(message_buf_send, "{\"type\":\"listen\",\"state\":\"detect\",\"text\":\"闹钟-#%s\",\"source\":\"text\"}", alarm_first->name.c_str());
        now_alarm_name = message_buf_send;
        display->SetStatus(alarm_first->name.c_str());  // 显示闹钟名字
        
        ESP_LOGI(TAG, "闹钟 '%s' 触发", alarm_first->name.c_str());
    }
    
    // 清理过期闹钟并设置下一个闹钟
    time_t now = time(NULL);
    ClearOverdueAlarm(now);

    Alarm *current_alarm_ = GetProximateAlarm(now);
    if(current_alarm_ != nullptr){
        int new_timer_time = current_alarm_->time - now;
        ESP_LOGI(TAG, "设置下一个闹钟在 %d 秒后", new_timer_time);
        esp_timer_start_once(timer_, new_timer_time * 1000000);
    } else {
        running_flag = false; // 没有闹钟了
        ESP_LOGI(TAG, "没有更多闹钟了");
    }
}

std::string AlarmManager::GetAlarmsStatus(){
    std::lock_guard<std::mutex> lock(mutex_);
    std::string status;
    for(auto& alarm : alarms_){
        if(!status.empty()){
            status += "; ";
        }
        status += alarm.name + " at " + std::to_string(alarm.time);
    }
    return status;
}

#endif