/**
 * @file Encoder.h
 * @brief 该头文件定义了一个用于处理编码器信号的 Encoder 类。
 *
 * 该类利用脉冲计数单元（PCNT）来统计编码器的脉冲信号，并允许用户注册回调函数，
 * 当计数值达到设定的限制时触发回调。
 *
 * @author 施华锋
 * @date 2025-2-18
 */

#ifndef ENCODER_H_
#define ENCODER_H_

// 引入脉冲计数单元驱动库，用于处理编码器的脉冲计数
#include <driver/pulse_cnt.h>
// 引入 GPIO 驱动库，用于配置编码器连接的 GPIO 引脚
#include <driver/gpio.h>
// 引入标准库中的 function 头文件，用于处理回调函数
#include <functional>

/**
 * @class Encoder
 * @brief 该类封装了编码器的操作，利用脉冲计数单元（PCNT）统计编码器脉冲。
 *
 * 该类允许用户注册一个回调函数，当脉冲计数值达到设定的上下限时，会触发该回调函数。
 */
class Encoder
{
public:
    /**
     * @brief 构造函数，初始化编码器对象。
     *
     * @param gpio_pcnt1 连接到编码器第一个信号引脚的 GPIO 编号。
     * @param gpio_pcnt2 连接到编码器第二个信号引脚的 GPIO 编号。
     * @param _low_limit 脉冲计数的下限，默认为 -1000。
     * @param _high_limit 脉冲计数的上限，默认为 1000。
     */
    Encoder(gpio_num_t gpio_pcnt1, gpio_num_t gpio_pcnt2, int _low_limit = -1000, int _high_limit = 1000);

    /**
     * @brief 析构函数，负责释放编码器对象占用的资源。
     */
    ~Encoder();

    /**
     * @brief 注册一个回调函数，当脉冲计数值达到上下限时触发。
     *
     * @param callback 一个接受 int 类型参数的函数对象，用于处理计数值达到限制的事件。
     */
    void OnPcntReach(std::function<void(int)> callback);

private:
    // 编码器第一个信号引脚连接的 GPIO 编号
    gpio_num_t gpio_pcnt1_;
    // 编码器第二个信号引脚连接的 GPIO 编号
    gpio_num_t gpio_pcnt2_;
    // 脉冲计数单元句柄，用于操作脉冲计数单元
    pcnt_unit_handle_t pcnt_unit_ = NULL;
    // 存储用户注册的回调函数，当计数值达到限制时调用
    std::function<void(int)> on_pcnt_reach_;
};

#endif // ENCODER_H_