#ifndef BLE_CONTROL_SERVER_H
#define BLE_CONTROL_SERVER_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string>
#include <cstdint>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif
#include "cJSON.h"
#ifdef __cplusplus
}
#endif

/**
 * BLE MCP 控制服务器
 *
 * 仿照 WebSocketControlServer 的独立传输通道设计，通过 BLE GATT 提供
 * JSON-RPC 2.0 控制接口，供微信小程序直连控制机器狗。
 *
 * BLE Service:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * RX Char (Write): 6E400002-...  小程序 → ESP32 写入命令
 * TX Char (Notify): 6E400003-...  ESP32 → 小程序推送响应
 */

class BleControlServer {
public:
    BleControlServer();
    ~BleControlServer();

    /** 启动 BLE 服务器（BT Controller + NimBLE + 广播） */
    bool Start();

    /** 停止 BLE 服务器 */
    void Stop();

    /**
     * 临时释放 BLE 协议栈内存（供配网等需要大内存的操作前调用）
     * - 停止广播、断开连接、deinit NimBLE + BT Controller
     * - 不释放 rx_queue_ / proc_task_（保留管道，Reinit 时复用）
     */
    void Deinit();

    /**
     * 重新初始化 BLE 协议栈（配网失败后恢复 BLE 功能）
     * - 重新 init BT Controller + NimBLE + 广播
     * - 要求在 Deinit() 之后调用
     */
    bool Reinit();

    /** 是否有 BLE 客户端已连接 */
    bool IsConnected() const { return connected_; }

    /** 设置 BLE 客户端连接时的回调（在 GAP CONNECT 事件成功时触发） */
    void SetOnConnectedCallback(std::function<void()> cb) { on_connected_cb_ = cb; }

    /** 设置 BLE 客户端断开时的回调（在 GAP DISCONNECT 事件触发） */
    void SetOnDisconnectedCallback(std::function<void()> cb) { on_disconnected_cb_ = cb; }

    /** 单条消息最大长度（字节） */
    static constexpr size_t MAX_MSG_LEN = 512;

    // 供静态 NimBLE 回调访问（自由函数非类成员，必须 public）
    QueueHandle_t rx_queue_;
    uint16_t tx_val_handle_;

    static BleControlServer* instance_;
    static int  _gap_event_cb(struct ble_gap_event* event, void* arg);

private:
    uint16_t conn_handle_;
    volatile bool connected_;
    volatile bool deinit_done_;  // Deinit 重入保护

    std::function<void()> on_connected_cb_;
    std::function<void()> on_disconnected_cb_;

    TaskHandle_t proc_task_;

    static void _nimble_host_task(void* param);
    static void _proc_task(void* param);

    void HandleToolCall(const char* tool_name, cJSON* arguments, int id);
    void SendNotification(const char* json, size_t len);
};

#endif // BLE_CONTROL_SERVER_H
