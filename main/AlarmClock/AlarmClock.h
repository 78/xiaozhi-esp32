#ifndef ALARMCLOCK_H
#define ALARMCLOCK_H

#include <string>
#include <vector>
#include <esp_log.h>
#include <esp_timer.h>
#include "time.h"
#include <mutex>
#include "settings.h"
#include <atomic>
#if CONFIG_USE_ALARM
struct Alarm {
    std::string name;
    int time;
};

class AlarmManager {
public:
    AlarmManager();
    ~AlarmManager();

    // 设置闹钟
    // 设置闹钟, 使用相对时间, 从现在开始多少秒以后, name是实际显示的闹钟名字
    // 设置好新闹钟以后判断一下当前最新的闹钟, 如果有比之前更早的闹钟重新设置定时器
    // 实际给AI操控的接口
    void SetAlarm(int seconde_from_now, std::string alarm_name);
    // 获取闹钟列表状态,返回一个描述字符串, 用于AI获取状态
    std::string GetAlarmsStatus();
    // 清除过时的闹钟, 从nvs以及列表里面清除数据
    void ClearOverdueAlarm(time_t now);
    // 获取从现在开始第一个响的闹钟
    Alarm *GetProximateAlarm(time_t now);
    // 闹钟响了的处理函数, 清除过时的闹钟, 设置闹钟的标志位, 显示闹钟的提醒
    // 之后可以改为发送tts信号
    void OnAlarm();
    // 闹钟是不是响了的标志位
    bool IsRing(){ return ring_flag; };
    // 清除闹钟标志位
    void ClearRing(){ESP_LOGI("Alarm", "clear");ring_flag = false;};
    // 当前的闹钟, 返回一个可以发送的对话json数据, 在OnAlarm里面设置
    std::string get_now_alarm_name(){return now_alarm_name;};
private:
    std::vector<Alarm> alarms_; // 闹钟列表
    std::mutex mutex_; // 互斥锁
    esp_timer_handle_t timer_; // 定时器

    std::atomic<bool> ring_flag{false}; 
    std::atomic<bool> running_flag{false}; // 时钟是不是在跑
};
#endif
#endif