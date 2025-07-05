#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"

#define TAG "game_scripts"

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

namespace iot {

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

/********* TinyUSB HID callbacks ***************/

// 收到 GET HID REPORT DESCRIPTOR 请求时调用
// 应用程序返回指向描述符的指针，其内容必须存在足够长的时间才能完成传输
extern "C" uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // 我们仅使用一个接口和一个 HID 报告描述符，因此我们可以忽略参数“instance”
    return hid_report_descriptor;
}

// 收到GET_REPORT控制请求时调用
// 应用程序必须填充缓冲区报告的内容并返回其长度。
// 返回零将导致堆栈STALL请求
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// 当接收到 SET_REPORT 控制请求或在 OUT 端点上接收到数据时调用（报告 ID = 0，类型 = 0）
extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

// 这里仅定义 GameScripts 的属性和方法，不包含具体的实现
class GameScripts : public Thing {
private:

    bool running_ = false;

    uint8_t keycode[6];

    int interval_time = 3 * 1000;    // 设置默认的间隔时间是3秒



    const char* hid_string_descriptor[5] = {
        // array of pointer to string descriptors
        (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
        "TinyUSB",             // 1: Manufacturer
        "TinyUSB Device",      // 2: Product
        "123456",              // 3: Serials, should use chip ID
        "Example HID interface",  // 4: HID
    };

    const uint8_t hid_configuration_descriptor[512] = {
        // Configuration number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

        // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
        TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
    };

    void app_send_hid_demo(void){
        // Keyboard output: Send key 'a/A' pressed and released
        ESP_LOGI(TAG, "Sending Keyboard report");
        // uint8_t keycode[6] = {HID_KEY_E};
        keycode[0] = HID_KEY_E;
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
        vTaskDelay(pdMS_TO_TICKS(50));
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);

        vTaskDelay(pdMS_TO_TICKS(30));

        // Mouse output: Move mouse cursor in square trajectory
        ESP_LOGI(TAG, "Sending Mouse report");
        int8_t delta_x;
        int8_t delta_y;

        // 左移鼠标
        delta_x = -15;
        delta_y = 0;
        for (int i = 0; i < 10; i++) {    // 
            tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, delta_x, delta_y, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // 点击鼠标
        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x01, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(40));
        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(50));

        // 移动到确定那里
        delta_x = 14;
        delta_y = 5;
        for (int i = 0; i < 18; i++) {    // 
            tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, delta_x, delta_y, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(20));
        }    

        // 点击鼠标
        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x01, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(40));
        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, 0, 0, 0, 0);

        vTaskDelay(pdMS_TO_TICKS(40));
        vTaskDelay(pdMS_TO_TICKS(1000));


        keycode[0] = HID_KEY_R;
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
        vTaskDelay(pdMS_TO_TICKS(50));
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        vTaskDelay(pdMS_TO_TICKS(30));

        vTaskDelay(pdMS_TO_TICKS(4000));

        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x01, 0, 0, 0, 0);

        vTaskDelay(pdMS_TO_TICKS(interval_time));    // 留六秒打怪


        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, 0, 0, 0, 0);


    }

    // 假设你的类名是 MyDevice
    static void HidLoopTask(void* pvParameters) {
        auto* self = static_cast<GameScripts*>(pvParameters);  // 如果在类中，需要传 this 指针

        while (true){
            while (self->running_) {
                if (tud_mounted()) {
                    self->app_send_hid_demo();
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        vTaskDelete(NULL); // 任务退出前自删（可选）
    }

    void  InitializeGpio() {
        const  tinyusb_config_t  tusb_cfg = {
            .device_descriptor = NULL,
            .string_descriptor = hid_string_descriptor,
            .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
            .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
            .fs_configuration_descriptor = hid_configuration_descriptor, // HID configuration descriptor for full-speed and high-speed are the same
            .hs_configuration_descriptor = hid_configuration_descriptor,
            .qualifier_descriptor = NULL,
#else
            .configuration_descriptor = hid_configuration_descriptor,
#endif // TUD_OPT_HIGH_SPEED
        };
        ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
        // 启动 HID 任务（this 传入当前类实例）
        xTaskCreate(HidLoopTask, "hid_task", 4096, this, 5, NULL);
    }

public:
    int interval_time_ = 3000;    // 设置默认的间隔时间是3秒

    GameScripts() : Thing("游戏脚本", "穿越火线游戏脚本"), running_(false) {
        InitializeGpio();

        // 定义设备的属性
        properties_.AddBooleanProperty("游戏脚本运行状态", "查看游戏脚本在不在运行状态", [this]() -> bool {
            // if (running_){
            //     ESP_LOGI(TAG, "脚本正在运行");
            // }
            // else{
            //     ESP_LOGI(TAG, "脚本没有运行");
            // }
            return running_;
        });

        properties_.AddNumberProperty("获取游戏脚本运行间隔时间", "获取游戏脚本运行的间隔时间，单位是毫秒", [this]() -> int {
            // ESP_LOGI(TAG, "当前间隔时间是%d", interval_time_);
            return interval_time_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("open_game_scripts", "打开游戏脚本", ParameterList(), [this](const ParameterList& parameters) {
            running_ = true;
        });

        methods_.AddMethod("close_game_scripts", "关闭游戏脚本", ParameterList(), [this](const ParameterList& parameters) {
            running_ = false;
        });

        Parameter param("new_interval_time", "调整后的新的游戏脚本间隔时间，单位是毫秒", kValueTypeNumber);  // 声明为 int 类型
        ParameterList param_list(std::vector<Parameter>{param});
        methods_.AddMethod("adjust_interval_time", "调整游戏脚本间隔时间到指定的毫秒数", param_list, [this](const ParameterList& parameters) {
            interval_time_ = parameters["new_interval_time"].number();
        });


    }
};

}
DECLARE_THING(GameScripts);
