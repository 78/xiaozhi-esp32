#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include "encoder.h"
#include <esp_log.h>

#define TAG "Encoder"

/**
 * @brief Encoder 类的构造函数，用于初始化编码器。
 *
 * @param gpio_pcnt1 编码器第一个信号引脚连接的 GPIO 编号。
 * @param gpio_pcnt2 编码器第二个信号引脚连接的 GPIO 编号。
 * @param _low_limit 脉冲计数的下限。
 * @param _high_limit 脉冲计数的上限。
 */
Encoder::Encoder(gpio_num_t gpio_pcnt1, gpio_num_t gpio_pcnt2, int _low_limit, int _high_limit)
    : gpio_pcnt1_(gpio_pcnt1), gpio_pcnt2_(gpio_pcnt2)
{
    // 检查 GPIO 引脚编号是否有效
    if (gpio_pcnt1 == -1 || gpio_pcnt2 == -1)
    {
        return;
    }

    // 安装脉冲计数单元
    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .low_limit = _low_limit,
        .high_limit = _high_limit,
    };
    // 创建脉冲计数单元
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit_));

    // 设置毛刺滤波器
    ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    // 为脉冲计数单元设置毛刺滤波器
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_, &filter_config));

    // 安装脉冲计数通道
    ESP_LOGI(TAG, "install pcnt channels");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = gpio_pcnt1,
        .level_gpio_num = gpio_pcnt2,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    // 创建通道 A
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_, &chan_a_config, &pcnt_chan_a));

    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = gpio_pcnt2,
        .level_gpio_num = gpio_pcnt1,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    // 创建通道 B
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_, &chan_b_config, &pcnt_chan_b));

    // 设置脉冲计数通道的边沿和电平动作
    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    // 设置通道 A 的边沿动作
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    // 设置通道 A 的电平动作
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    // 设置通道 B 的边沿动作
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    // 设置通道 B 的电平动作
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // 添加监视点并注册回调
    ESP_LOGI(TAG, "add watch points and register callbacks");
    // 添加下限监视点
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit_, _low_limit));
    // 添加上限监视点
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit_, _high_limit));
}

/**
 * @brief Encoder 类的析构函数，用于释放编码器资源。
 */
Encoder::~Encoder()
{
    if (pcnt_unit_ != NULL)
    {
        // 删除脉冲计数单元
        ESP_ERROR_CHECK(pcnt_del_unit(pcnt_unit_));
        pcnt_unit_ = NULL;
    }
}

/**
 * @brief 注册一个回调函数，当脉冲计数值达到上下限或发生变化时触发。
 *
 * @param callback 一个接受 int 类型参数的函数对象，用于处理计数值变化事件。
 */
void Encoder::OnPcntReach(std::function<void(int)> callback)
{
    if (pcnt_unit_ == nullptr)
    {
        return;
    }

    // 保存回调函数
    on_pcnt_reach_ = callback;

    // 定义脉冲计数事件回调结构体
    pcnt_event_callbacks_t cbs = {
        .on_reach = [](pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) -> bool
        {
            // 将用户上下文转换为 Encoder 对象指针
            Encoder *encoder = static_cast<Encoder *>(user_ctx);
            if (encoder->on_pcnt_reach_)
            {
                // 调用回调函数
                encoder->on_pcnt_reach_(edata->watch_point_value);
            }
            return true;
        }};
    // 为脉冲计数单元注册事件回调
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit_, &cbs, this));

    // 启用脉冲计数单元
    ESP_LOGI(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_));
    // 清除脉冲计数单元的计数值
    ESP_LOGI(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_));
    // 启动脉冲计数单元
    ESP_LOGI(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_));

    // 创建一个任务来定期检查脉冲计数值
    xTaskCreate(
        [](void *arg)
        {
            // 将任务参数转换为 Encoder 对象指针
            Encoder *encoder = static_cast<Encoder *>(arg);
            static int lastvalue = 0;
            int pulse_count = 0;
            while (encoder->pcnt_unit_ != NULL)
            {
                // 获取当前脉冲计数值
                ESP_ERROR_CHECK(pcnt_unit_get_count(encoder->pcnt_unit_, &pulse_count));
                if (lastvalue != pulse_count)
                {
                    lastvalue = pulse_count;
                    if (encoder->on_pcnt_reach_)
                    {
                        // 调用回调函数
                        encoder->on_pcnt_reach_(pulse_count);
                    }
                }
                // 任务延时 100 毫秒
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            // 删除当前任务
            vTaskDelete(NULL);
        },
        "encoder",
        4096 - 1024,
        this,
        3,
        nullptr);
}