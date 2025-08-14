#ifndef TOUCH_CONFIG_H
#define TOUCH_CONFIG_H

#include <cstdint>

// 触摸检测配置结构
struct TouchDetectionConfig {
    uint32_t tap_max_duration_ms;      // 单击最大持续时间
    uint32_t hold_min_duration_ms;     // 长按最小持续时间
    uint32_t cradled_min_duration_ms;  // 摇篮模式最小持续时间
    uint32_t tickled_window_ms;        // 挠痒检测窗口
    uint32_t tickled_min_touches;      // 挠痒最小触摸次数
    uint32_t debounce_time_ms;         // 消抖时间
    float touch_threshold_ratio;       // 触摸阈值比例
    
    // 默认值构造函数
    TouchDetectionConfig() 
        : tap_max_duration_ms(500)
        , hold_min_duration_ms(600)
        , cradled_min_duration_ms(2000)
        , tickled_window_ms(2000)
        , tickled_min_touches(4)
        , debounce_time_ms(30)
        , touch_threshold_ratio(1.5f) {}
};

// 触摸配置加载器
class TouchConfigLoader {
public:
    // 从JSON文件加载配置
    static bool LoadFromFile(const char* filepath, TouchDetectionConfig& config);
    
    // 从嵌入的默认配置加载
    static TouchDetectionConfig LoadDefaults();
    
    // 从JSON字符串解析配置
    static bool ParseFromJson(const char* json_str, TouchDetectionConfig& config);
};

#endif // TOUCH_CONFIG_H