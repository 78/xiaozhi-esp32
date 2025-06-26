#include "afsk_demod.h"
#include <cstring>
#include <algorithm>
#include "esp_log.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace afsk
{
    static const char *TAG = "AFSK_DEMOD";

    void loop_provisioning(Application *app,
                           WifiConfigurationAp *wifi_ap)
    {
        const int input_fs = 16000;                                // 输入采样率
        const float step = (float)(input_fs) / (float)SAMPLE_RATE; // 下采样步长
        std::vector<int16_t> data;
        PairGoertzel pair_goertzel(SAMPLE_RATE, MARK_FREQ, SPACE_FREQ, BITRATE, WINSIZE);
        CascadeBuffer cascade_buffer;

        while (true)
        {
            app->ReadAudio(data, 16000, 480); // 16kHz, 480 samples 对应 30ms 数据
            // 抽样下采样
            std::vector<float> downsampled_data;
            size_t last_idx = 0;

            if (step > 1.0f)
            {
                downsampled_data.reserve(data.size() / static_cast<size_t>(step));
                for (size_t i = 0; i < data.size(); ++i)
                {
                    size_t sample_idx = static_cast<size_t>(i / step);
                    if ((sample_idx + 1) > last_idx)
                    {
                        downsampled_data.push_back(static_cast<float>(data[i]));
                        last_idx = sample_idx + 1;
                    }
                }
            }
            else
            {
                downsampled_data.reserve(data.size());
                for (int16_t sample : data)
                {
                    downsampled_data.push_back(static_cast<float>(sample));
                }
            }
            // 得到pair_goertzel的概率
            auto probas = pair_goertzel.process(downsampled_data);
            // 将概率数据传入级联缓冲区
            if (cascade_buffer.extend_proba(probas, 0.5f))
            {
                // 如果接收到完整数据，打印ASCII字符串
                if (cascade_buffer.ascii.has_value())
                {
                    ESP_LOGI(TAG, "Received ASCII: %s", cascade_buffer.ascii->c_str());
                    // 根据\n 分割SSID和密码
                    std::string ssid, password;
                    size_t pos = cascade_buffer.ascii->find('\n');
                    if (pos != std::string::npos)
                    {
                        ssid = cascade_buffer.ascii->substr(0, pos);
                        password = cascade_buffer.ascii->substr(pos + 1);
                        ESP_LOGI(TAG, "SSID: %s, Password: %s", ssid.c_str(), password.c_str());
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Invalid format, no \\n found in received data");
                        continue;
                    }
                    if (wifi_ap->ConnectToWifi(ssid, password))
                    {                                  // 尝试连接到WiFi
                        wifi_ap->Save(ssid, password); // 保存SSID和密码
                        esp_restart();                 // 重启设备以应用新的WiFi配置
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to connect to WiFi with received credentials");
                    }
                    cascade_buffer.ascii.reset(); // 清空已处理的ASCII数据
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1)); // 1ms
        }
    }

    // 默认的开始和结束标识符
    // \x01\x02 = 00000001 00000010
    const std::vector<uint8_t> DEFAULT_START_VECTOR = {
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0};

    // \x03\x04 = 00000011 00000100
    const std::vector<uint8_t> DEFAULT_END_VECTOR = {
        0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0};

    // TraceGoertzel 实现
    TraceGoertzel::TraceGoertzel(float freq, size_t n)
        : freq_(freq), n_(n)
    {
        k_ = std::floor(freq_ * static_cast<float>(n_));
        w_ = 2.0f * M_PI * freq_;
        cw_ = std::cos(w_);
        sw_ = std::sin(w_);
        c_ = 2.0f * cw_;

        // 初始化环形缓冲区
        zs_.push_back(0.0f);
        zs_.push_back(0.0f);
    }

    void TraceGoertzel::reset()
    {
        zs_.clear();
        zs_.push_back(0.0f);
        zs_.push_back(0.0f);
    }

    void TraceGoertzel::update(float x)
    {
        if (zs_.size() < 2)
        {
            return;
        }

        float z2 = zs_.front(); // S[-2]
        zs_.pop_front();
        float z1 = zs_.front(); // S[-1]
        zs_.pop_front();

        float z0 = x + c_ * z1 - z2;

        zs_.push_back(z1); // 将S[-1]放回
        zs_.push_back(z0); // 添加新的S[0]
    }

    float TraceGoertzel::amplitude() const
    {
        if (zs_.size() < 2)
        {
            return 0.0f;
        }

        float z1 = zs_[1];        // S[-1]
        float z2 = zs_[0];        // S[-2]
        float ip = cw_ * z1 - z2; // 实部
        float qp = sw_ * z1;      // 虚部

        return std::sqrt(ip * ip + qp * qp) / (static_cast<float>(n_) / 2.0f);
    }

    // PairGoertzel 实现
    PairGoertzel::PairGoertzel(size_t f_sample, size_t mark_freq, size_t space_freq,
                               size_t bitrate, size_t winsize)
        : in_size_(winsize), out_count_(0)
    {

        if (f_sample % bitrate != 0)
        {
            // 在ESP32中我们可以继续执行，但记录错误
        }

        float f1 = static_cast<float>(mark_freq) / static_cast<float>(f_sample);  // f1 归一化频率
        float f0 = static_cast<float>(space_freq) / static_cast<float>(f_sample); // f0 归一化频率

        mark_ = std::make_unique<TraceGoertzel>(f1, winsize);
        space_ = std::make_unique<TraceGoertzel>(f0, winsize);

        out_size_ = f_sample / bitrate; // 每个比特对应的采样点数
    }

    std::vector<float> PairGoertzel::process(const std::vector<float> &samples)
    {
        std::vector<float> result;

        for (float x : samples)
        {
            if (in_buffer_.size() < in_size_)
            {
                in_buffer_.push_back(x); // 仅放入，不计算
            }
            else
            {
                // 输入缓冲区满了，进行处理
                in_buffer_.pop_front();  // 弹出最早的采样点
                in_buffer_.push_back(x); // 添加新的采样点
                out_count_++;

                if (out_count_ >= out_size_)
                {
                    // 对窗口内的采样点进行Goertzel算法处理
                    for (float sample : in_buffer_)
                    {
                        mark_->update(sample);
                        space_->update(sample);
                    }

                    float amp1 = mark_->amplitude();  // Mark的振幅
                    float amp0 = space_->amplitude(); // Space的振幅

                    // 避免除以0
                    float prob_mark = amp1 / (amp0 + amp1 + std::numeric_limits<float>::epsilon());
                    result.push_back(prob_mark);

                    // 重置Goertzel窗口
                    mark_->reset();
                    space_->reset();
                    out_count_ = 0; // 重置输出计数
                }
            }
        }

        return result;
    }

    // CascadeBuffer 实现
    CascadeBuffer::CascadeBuffer()
        : state_(ReceiveState::Inactive),
          sot_(DEFAULT_START_VECTOR),
          eot_(DEFAULT_END_VECTOR),
          need_checksum_(true)
    {

        ident_size_ = std::max(sot_.size(), eot_.size());
        bit_size_ = 776; // 预设的位缓冲区大小，776位 (32 + 1 + 63 + 1) * 8 = 776

        bits_.reserve(bit_size_);
    }

    CascadeBuffer::CascadeBuffer(size_t bytesize, const std::vector<uint8_t> &sot,
                                 const std::vector<uint8_t> &eot, bool need_checksum)
        : state_(ReceiveState::Inactive),
          sot_(sot),
          eot_(eot),
          need_checksum_(need_checksum)
    {

        ident_size_ = std::max(sot_.size(), eot_.size());
        bit_size_ = bytesize * 8; // 位缓冲区大小，以字节为单位

        bits_.reserve(bit_size_);
    }

    uint8_t CascadeBuffer::check_sum(const std::string &ascii)
    {
        uint8_t sum = 0;
        for (char c : ascii)
        {
            sum += static_cast<uint8_t>(c);
        }
        return sum;
    }

    void CascadeBuffer::clear()
    {
        idents_.clear();
        bits_.clear();
    }

    bool CascadeBuffer::extend_proba(const std::vector<float> &probs, float threshold)
    {
        for (float proba : probs)
        {
            uint8_t bit = (proba > threshold) ? 1 : 0;

            if (idents_.size() >= ident_size_)
            {
                idents_.pop_front(); // 保持缓冲区大小
            }
            idents_.push_back(bit);

            // 根据状态机处理接收到的比特
            switch (state_)
            {
            case ReceiveState::Inactive:
                if (idents_.size() >= sot_.size())
                {
                    state_ = ReceiveState::Waiting; // 进入等待接收状态
                    ESP_LOGI(TAG, "Entering Waiting state");
                }
                break;

            case ReceiveState::Waiting:
                // 等待状态，可能是等待接收结束
                if (idents_.size() >= sot_.size())
                {
                    std::vector<uint8_t> ident_snapshot(idents_.begin(), idents_.end());
                    if (ident_snapshot == sot_)
                    {
                        clear();                          // 清空缓冲区
                        state_ = ReceiveState::Receiving; // 进入接收状态
                        ESP_LOGI(TAG, "Entering Receiving state");
                    }
                }
                break;

            case ReceiveState::Receiving:
                bits_.push_back(bit);
                if (idents_.size() >= eot_.size())
                {
                    std::vector<uint8_t> ident_snapshot(idents_.begin(), idents_.end());
                    if (ident_snapshot == eot_)
                    {
                        state_ = ReceiveState::Inactive; // 进入空闲状态

                        // 将位转换为字节
                        std::vector<uint8_t> bytes = bits_to_bytes(bits_);

                        uint8_t checksum = 0;
                        size_t least_len = 0;

                        if (need_checksum_)
                        {
                            // 如果需要校验和，最后一个字节是校验和
                            least_len = 1 + sot_.size() / 8;
                            if (bytes.size() >= least_len)
                            {
                                checksum = bytes[bytes.size() - sot_.size() / 8 - 1];
                            }
                        }
                        else
                        {
                            least_len = sot_.size() / 8;
                        }

                        if (bytes.size() < least_len)
                        {
                            clear();
                            ESP_LOGW(TAG, "Data too short, clearing buffer");
                            return false; // 数据太短，返回失败状态
                        }

                        // 提取ASCII字符串（去掉最后的标识符部分）
                        std::vector<uint8_t> ascii_bytes(
                            bytes.begin(), bytes.begin() + bytes.size() - least_len);

                        std::string result(ascii_bytes.begin(), ascii_bytes.end());

                        // 如果需要校验和验证
                        if (need_checksum_)
                        {
                            uint8_t calculated_checksum = check_sum(result);
                            if (calculated_checksum != checksum)
                            {
                                // 校验和不匹配
                                ESP_LOGW(TAG, "Checksum mismatch: expected %d, got %d", checksum, calculated_checksum);
                                clear();
                                return false;
                            }
                        }

                        clear();
                        ascii = result;
                        return true; // 返回成功状态
                    }
                    else if (bits_.size() >= bit_size_)
                    {
                        // 如非结束标识符且位缓冲区已满，则重置
                        clear();
                        ESP_LOGW(TAG, "Buffer overflow, clearing buffer");
                        state_ = ReceiveState::Inactive; // 重置状态机
                    }
                }
                break;
            }
        }

        return false;
    }

    std::vector<uint8_t> CascadeBuffer::bits_to_bytes(const std::vector<uint8_t> &bits) const
    {
        std::vector<uint8_t> bytes;

        // 确保位数是8的倍数
        size_t num_complete_bytes = bits.size() / 8;
        bytes.reserve(num_complete_bytes);

        for (size_t i = 0; i < num_complete_bytes; ++i)
        {
            uint8_t byte = 0;
            for (size_t j = 0; j < 8; ++j)
            {
                byte |= bits[i * 8 + j] << (7 - j);
            }
            bytes.push_back(byte);
        }

        return bytes;
    }
}