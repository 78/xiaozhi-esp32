#pragma once

#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <optional>
#include <cmath>
#include "wifi_configuration_ap.h"
#include "application.h"

#define SAMPLE_RATE 6400
#define MARK_FREQ 1800
#define SPACE_FREQ 1500
#define BITRATE 100
#define WINSIZE 64

namespace afsk
{
    void loop_provisioning(Application *app, WifiConfigurationAp *wifi_ap);

    /**
     * 单个关注频率的Goertzel算法实现
     */
    class TraceGoertzel
    {
    private:
        float freq_;           // 目标频率(归一化后, 即 f / fs)
        size_t n_;             // 窗口大小
        float k_;              // 频率bin
        float w_;              // 角频率
        float cw_;             // cos(w)
        float sw_;             // sin(w)
        float c_;              // 2 * cos(w)
        std::deque<float> zs_; // 用于存储S[-1]和S[-2]的环形缓冲区

    public:
        /**
         * 构造函数
         * @param freq 归一化频率 (f / fs)
         * @param n 窗口大小
         */
        TraceGoertzel(float freq, size_t n);

        /**
         * 重置状态
         */
        void reset();

        /**
         * 更新一个采样点
         * @param x 输入采样点
         */
        void update(float x);

        /**
         * 计算当前振幅
         * @return 振幅值
         */
        float amplitude() const;
    };

    /**
     * 关注Mark/Space频率对的结构体
     */
    class PairGoertzel
    {
    private:
        std::deque<float> in_buffer_;          // 输入缓冲区<采样点>
        size_t in_size_;                       // 输入缓冲区大小 = 窗口大小
        size_t out_count_;                     // 输出计数
        size_t out_size_;                      // 输出计数阈值 = 每个比特对应的采样点数
        std::unique_ptr<TraceGoertzel> mark_;  // Mark频率的Goertzel
        std::unique_ptr<TraceGoertzel> space_; // Space频率的Goertzel

    public:
        /**
         * 构造函数
         * @param f_sample 采样率
         * @param mark_freq Mark频率
         * @param space_freq Space频率
         * @param bitrate 比特率
         * @param winsize 窗口大小
         */
        PairGoertzel(size_t f_sample, size_t mark_freq, size_t space_freq,
                     size_t bitrate, size_t winsize);

        /**
         * 处理输入采样点数组
         * @param samples 输入采样点向量
         * @return Prob(mark)的向量
         */
        std::vector<float> process(const std::vector<float> &samples);
    };

    /**
     * 接收状态枚举
     */
    enum class ReceiveState
    {
        Inactive, // 未激活状态
        Waiting,  // 等待状态
        Receiving // 接收状态
    };

    /**
     * 级联缓冲区，用于输出阶段的状态管理
     */
    class CascadeBuffer
    {
    private:
        ReceiveState state_;             // 接收状态
        std::deque<uint8_t> idents_;     // 标识符缓冲区
        size_t ident_size_;              // 标识符缓冲区大小
        std::vector<uint8_t> bits_;      // 位缓冲区(记录bit流)
        size_t bit_size_;                // 位缓冲区大小
        const std::vector<uint8_t> sot_; // 开始标识符
        const std::vector<uint8_t> eot_; // 结束标识符
        bool need_checksum_;             // 是否需要校验和验证

    public:
        std::optional<std::string> ascii; // 接收的ASCII字符串

        /**
         * 默认构造函数，使用默认的开始和结束标识符
         */
        CascadeBuffer();

        /**
         * 构造函数
         * @param bytesize 预期接收的字节数
         * @param sot 开始标识符
         * @param eot 结束标识符
         * @param need_checksum 是否需要校验和验证
         */
        CascadeBuffer(size_t bytesize, const std::vector<uint8_t> &sot,
                      const std::vector<uint8_t> &eot, bool need_checksum = false);

        /**
         * 扩展概率数据并处理
         * @param probs 概率向量
         * @param threshold 阈值
         * @return 是否成功接收到完整数据
         */
        bool extend_proba(const std::vector<float> &probs, float threshold = 0.5);

        /**
         * 计算ASCII字符串的校验和
         * @param ascii ASCII字符串
         * @return 校验和值（0-255）
         */
        static uint8_t check_sum(const std::string &ascii);

    private:
        /**
         * 将位向量转换为字节向量
         * @param bits 位向量
         * @return 字节向量
         */
        std::vector<uint8_t> bits_to_bytes(const std::vector<uint8_t> &bits) const;

        /**
         * 清空缓冲区
         */
        void clear();
    };

    // 默认的开始和结束标识符
    extern const std::vector<uint8_t> DEFAULT_START_VECTOR;
    extern const std::vector<uint8_t> DEFAULT_END_VECTOR;
}