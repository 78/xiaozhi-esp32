#include "ble_control_server.h"
#include "dog_control.h"
#include "choreo.h"
#include "beat_sync.h"
#include "servo.h"

#include <esp_log.h>
#include <cJSON.h>
#include <cstring>
#include <cstdio>
#include <functional>

#ifdef CONFIG_BT_NIMBLE_ENABLED
#include <esp_bt.h>
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#endif

static const char* TAG = "BLEControl";

/* ─────────────── 异步动作任务（同 WebSocketControlServer） ──────── */
static TaskHandle_t s_action_task_handle = NULL;

static void action_task(void* arg) {
    auto* fn = (std::function<void()>*)arg;
    (*fn)();
    Dog_ResetAll();
    delete fn;
    s_action_task_handle = NULL;
    servo_set_action_task_handle(NULL);
    vTaskDelete(NULL);
}

static void run_action_async(std::function<void()> action) {
    if (s_action_task_handle != NULL) {
        ESP_LOGW(TAG, "上一个动作还在执行，跳过新动作");
        return;
    }
    auto* p = new std::function<void()>(std::move(action));
    xTaskCreate(action_task, "ble_action", 4096, p, 5, &s_action_task_handle);
    servo_set_action_task_handle(s_action_task_handle);
}

/* ─────────────── 全局实例（始终有定义） ─────────────────────── */
// 注意：instance_ 必须始终有定义，即使 NimBLE 未启用，
//       否则 compact_wifi_board.cc 链接时会 undefined reference。
BleControlServer* BleControlServer::instance_ = nullptr;

// 以下全部只在 CONFIG_BT_NIMBLE_ENABLED 时编译

#ifdef CONFIG_BT_NIMBLE_ENABLED

/* ─────────────── NimBLE GATT 定义 ───────────────────────────── */
// 128-bit UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E (NimBLE 用小端)
#define BLE_SVC_UUID \
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
                     0xF3, 0x93, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E)
#define BLE_RX_UUID \
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
                     0xF3, 0x93, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E)
#define BLE_TX_UUID \
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
                     0xF3, 0x93, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E)

static const ble_uuid128_t svc_uuid = BLE_SVC_UUID;
static const ble_uuid128_t rx_uuid  = BLE_RX_UUID;
static const ble_uuid128_t tx_uuid  = BLE_TX_UUID;

static uint16_t tx_val_handle;  // 由 NimBLE 在注册时写入

// 广播参数（文件作用域，断开重连时复用）
static struct ble_gap_adv_params s_adv_params = {0};

// RX characteristic 访问回调：客户端写入 → 把数据交给 BleControlServer
// 注意：gatt_chrs 是静态数组，.arg 默认为 nullptr，
//       因此通过 instance_ 获取实例指针，不依赖 arg 参数。
// TX characteristic 不需要处理客户端访问，但 NimBLE 要求 access_cb 非空
static int gatt_svr_tx_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    (void)conn_handle; (void)attr_handle; (void)ctxt; (void)arg;
    return 0;
}

static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    (void)arg;
    BleControlServer* self = BleControlServer::instance_;
    if (!self) return 0;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && ctxt->om != nullptr) {
        // 从 mbuf 中读取写入数据
        uint8_t buf[BleControlServer::MAX_MSG_LEN];
        size_t len = 0;
        struct os_mbuf* om = ctxt->om;
        while (om && len < sizeof(buf) - 1) {
            size_t chunk = OS_MBUF_PKTLEN(om);
            if (chunk > sizeof(buf) - 1 - len) chunk = sizeof(buf) - 1 - len;
            memcpy(buf + len, om->om_data, chunk);
            len += chunk;
            om = SLIST_NEXT(om, om_next);
        }
        buf[len] = '\0';

        if (len > 0) {
            // 复制消息入队（ble_control_server.cc 的 _proc_task 处理）
            char* msg = (char*)malloc(len + 1);
            if (msg) {
                memcpy(msg, buf, len + 1);
                if (xQueueSend(self->rx_queue_, &msg, 0) != pdTRUE) {
                    free(msg);
                    ESP_LOGW(TAG, "RX queue full, dropping message");
                }
            }
        }
    }
    return 0;
}

// GATT Service 定义
static const struct ble_gatt_chr_def gatt_chrs[] = {
    {
        .uuid = (ble_uuid_t*)&rx_uuid,
        .access_cb = gatt_svr_chr_access,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        .uuid = (ble_uuid_t*)&tx_uuid,
        .access_cb = gatt_svr_tx_access,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &tx_val_handle,
    },
    { 0 }
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t*)&svc_uuid,
        .characteristics = (struct ble_gatt_chr_def*)gatt_chrs,
    },
    { 0 }
};

/* ────────────── NimBLE 任务和回调 ───────────────────────── */
void BleControlServer::_nimble_host_task(void* param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}

static void nimble_on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE reset; reason=%d", reason);
}

static void nimble_on_sync(void) {
    ESP_LOGI(TAG, "NimBLE synced, registering GATT service");

    // 配置设备名
    ble_svc_gap_device_name_set("Xiaozhi-Dog");

    // 注册 GATT service
    int rc = ble_gatts_count_cfg((struct ble_gatt_svc_def*)gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "GATT count_cfg OK");

    rc = ble_gatts_add_svcs((struct ble_gatt_svc_def*)gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "GATT add_svcs OK");

    // 关键：GATT 服务注册完成后，必须调用 ble_gatts_start() 刷新服务数据库
    // 因为 nimble_on_sync 是在 ble_hs_start() 之后触发的，此时 ble_gatts_start()
    // 已经自动调用过了，新注册的服务需要再次 start 才能被客户端发现
    rc = ble_gatts_start();
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "GATT server started, services ready");

    // 设置广播数据：设备名 + 标志位
    struct ble_hs_adv_fields adv_fields = {0};
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.name = (uint8_t*)"Xiaozhi-Dog";
    adv_fields.name_len = strlen("Xiaozhi-Dog");
    adv_fields.name_is_complete = 1;
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Adv fields set OK");

    // 开始广播
    s_adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    s_adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                              &s_adv_params, BleControlServer::_gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising started as 'Xiaozhi-Dog'");
    }
}

// GAP 事件处理（连接 / 断开）
int BleControlServer::_gap_event_cb(struct ble_gap_event* event, void* arg) {
    (void)arg;
    int r;  // 各 case 块内赋值，函数开头统一声明
    BleControlServer* self = instance_;
    if (!self) return 0;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "BLE client connected, handle=%d",
                     event->connect.conn_handle);
            self->conn_handle_ = event->connect.conn_handle;
            self->connected_ = true;
            // 触发连接回调（互斥逻辑：关闭 WebSocket）
            if (self->on_connected_cb_) {
                self->on_connected_cb_();
            }
        } else {
            ESP_LOGW(TAG, "BLE connection failed, status=%d",
                     event->connect.status);
            self->connected_ = false;
            // 重新开始广播
            r = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                                    &s_adv_params, _gap_event_cb, NULL);
            ESP_LOGI(TAG, "adv restart after conn fail: %d", r);
        }
        break;
    }

    case BLE_GAP_EVENT_DISCONNECT: {
        ESP_LOGI(TAG, "BLE client disconnected, reason=%d",
                 event->disconnect.reason);
        self->conn_handle_ = BLE_HS_CONN_HANDLE_NONE;
        self->connected_ = false;
        // 触发断开回调（互斥逻辑：重启 WebSocket）
        if (self->on_disconnected_cb_) {
            self->on_disconnected_cb_();
        }
        // 重新开始广播
        r = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                                &s_adv_params, _gap_event_cb, NULL);
        ESP_LOGI(TAG, "adv restart after disconnect: %d", r);
        break;
    }

    case BLE_GAP_EVENT_ADV_COMPLETE: {
        ESP_LOGI(TAG, "Advertising complete, restarting");
        r = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                              &s_adv_params, _gap_event_cb, NULL);
        ESP_LOGI(TAG, "adv restart after complete: %d", r);
        break;
    }

    case BLE_GAP_EVENT_MTU: {
        ESP_LOGI(TAG, "MTU negotiated: %d", event->mtu.value);
        break;
    }

    default:
        break;
    }
    return 0;
}

/* ─────────────── 响应发送（NimBLE 可用） ─────────────────────── */
void BleControlServer::SendNotification(const char* json, size_t len) {
    if (!connected_ || conn_handle_ == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "No BLE client connected, cannot send notification");
        return;
    }

    struct os_mbuf* om = ble_hs_mbuf_from_flat((const uint8_t*)json, len);
    if (om == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for notification");
        return;
    }

    int rc = ble_gatts_notify_custom(conn_handle_, tx_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Notify failed: %d", rc);
        os_mbuf_free_chain(om);
    }
}

/* ─────────────── Stop / Deinit / Reinit（NimBLE 启用时） ──────── */
void BleControlServer::Stop() {
#ifdef CONFIG_BT_NIMBLE_ENABLED
    if (connected_) {
        ble_gap_terminate(conn_handle_, BLE_ERR_REM_USER_CONN_TERM);
        connected_ = false;
    }

    int ret = nimble_port_stop();
    if (ret == 0) {
        nimble_port_freertos_deinit();
    }
    esp_nimble_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
#endif

    if (proc_task_) {
        vTaskDelete(proc_task_);
        proc_task_ = nullptr;
    }
    if (rx_queue_) {
        vQueueDelete(rx_queue_);
        rx_queue_ = nullptr;
    }

    ESP_LOGI(TAG, "BLE Control Server stopped");
}

void BleControlServer::Deinit() {
    if (deinit_done_) {
        ESP_LOGW(TAG, "BLE Deinit: already deinitialized, skipping");
        return;
    }
    ESP_LOGI(TAG, "BLE Deinit: releasing BT stack memory for wifi config");

    // 1. 停止广播
    ble_gap_adv_stop();

    // 2. 断开已有连接
    if (connected_) {
        ble_gap_terminate(conn_handle_, BLE_ERR_REM_USER_CONN_TERM);
        connected_ = false;
    }

    // 3. 停止 NimBLE host task
    //    注意: nimble_port_stop() 会让 host task 在自身上下文调用
    //    nimble_port_freertos_deinit() 后自我退出。
    //    stop() 返回后 host task 已删除，不可再调用 freertos_deinit()
    //    （host_task_ 已为野指针，二次 vTaskDelete 必崩）。
    //    此时只需 esp_nimble_deinit() 清理协议栈残余资源。
    int ret = nimble_port_stop();
    if (ret == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));  // 确保 host task 完全离开调度
    } else {
        ESP_LOGW(TAG, "nimble_port_stop returned %d", ret);
    }

    // 4. 反初始化 NimBLE 协议栈（不包含任务删除，host task 已自删）
    esp_nimble_deinit();

    // 5. 关闭并反初始化 BT Controller
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    deinit_done_ = true;

    // 注意：proc_task_ 和 rx_queue_ 此处故意不释放，
    //       Reinit() 后 _proc_task 可继续复用同一队列。
    ESP_LOGI(TAG, "BLE Deinit complete (protocol stack freed)");
}

bool BleControlServer::Reinit() {
    ESP_LOGI(TAG, "BLE Reinit: re-initializing BT stack");

    // 重置 deinit 标记
    deinit_done_ = false;

    // 1. 重新初始化 BT Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller re-init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        esp_bt_controller_deinit();
        return false;
    }

    // 2. 重新初始化 NimBLE
    ret = esp_nimble_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE re-init failed: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return false;
    }

    // 3. 重新配置 NimBLE 回调
    ble_hs_cfg.reset_cb = nimble_on_reset;
    ble_hs_cfg.sync_cb  = nimble_on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;

    // 4. 重新启动 NimBLE host task
    //    nimble_on_sync() 会在 sync 后自动被调用，
    //    它会重新注册 GATT 服务并开启广播。
    nimble_port_freertos_init(_nimble_host_task);

    ESP_LOGI(TAG, "BLE Reinit triggered, waiting for sync + advertising...");
    return true;
}

#endif /* CONFIG_BT_NIMBLE_ENABLED */


/* ─────────────── 空实现（NimBLE 未启用时） ────────────────── */

#ifndef CONFIG_BT_NIMBLE_ENABLED
void BleControlServer::SendNotification(const char* json, size_t len) {
    ESP_LOGI(TAG, "BLE not available, response: %.*s", (int)len, json);
}

void BleControlServer::Stop() {
    ESP_LOGW(TAG, "BLE not available, Stop() ignored");
    if (proc_task_) {
        vTaskDelete(proc_task_);
        proc_task_ = nullptr;
    }
    if (rx_queue_) {
        vQueueDelete(rx_queue_);
        rx_queue_ = nullptr;
    }
}

void BleControlServer::Deinit() {
    ESP_LOGW(TAG, "BLE not available, Deinit() ignored");
}

bool BleControlServer::Reinit() {
    ESP_LOGW(TAG, "BLE not available, Reinit() ignored");
    return false;
}
#endif /* !CONFIG_BT_NIMBLE_ENABLED */


/* ─────────────── 工具调用处理 ──────────────────────────────────── */

void BleControlServer::HandleToolCall(const char* tool_name,
                                       cJSON* arguments, int id) {
    int steps = 3;
    int fb = 0, lr = 0;
    int cycles = 2, dir = 1;
    char result_buf[128] = {0};

    if (arguments && cJSON_IsObject(arguments)) {
        cJSON* steps_item = cJSON_GetObjectItem(arguments, "步数");
        if (cJSON_IsNumber(steps_item)) {
            steps = steps_item->valueint;
            if (steps < 1) steps = 1;
            if (steps > 10) steps = 10;
        }
        cJSON* fb_item = cJSON_GetObjectItem(arguments, "fb");
        if (cJSON_IsNumber(fb_item)) fb = fb_item->valueint;
        cJSON* lr_item = cJSON_GetObjectItem(arguments, "lr");
        if (cJSON_IsNumber(lr_item)) lr = lr_item->valueint;
        cJSON* cycles_item = cJSON_GetObjectItem(arguments, "cycles");
        if (cJSON_IsNumber(cycles_item)) cycles = cycles_item->valueint;
        cJSON* dir_item = cJSON_GetObjectItem(arguments, "dir");
        if (cJSON_IsNumber(dir_item)) dir = dir_item->valueint;
    }

    const char* result_text = "执行完成";

    // ── 行走命令 ──
    if (strcmp(tool_name, "向前走") == 0 || strcmp(tool_name, "机器狗向前走") == 0) {
        Dog_ForwardSteps(steps); result_text = "前进完成";
    } else if (strcmp(tool_name, "向后退") == 0 || strcmp(tool_name, "机器狗向后退") == 0) {
        Dog_BackwardSteps(steps); result_text = "后退完成";
    } else if (strcmp(tool_name, "左转") == 0 || strcmp(tool_name, "机器狗左转") == 0) {
        Dog_TurnLeftSteps(steps); result_text = "左转完成";
    } else if (strcmp(tool_name, "右转") == 0 || strcmp(tool_name, "机器狗右转") == 0) {
        Dog_TurnRightSteps(steps); result_text = "右转完成";
    } else if (strcmp(tool_name, "机器狗向前走1步") == 0) {
        Dog_ForwardSteps(1); result_text = "前进 1 步";
    } else if (strcmp(tool_name, "机器狗向前走3步") == 0) {
        Dog_ForwardSteps(3); result_text = "前进 3 步";
    } else if (strcmp(tool_name, "机器狗向前走5步") == 0) {
        Dog_ForwardSteps(5); result_text = "前进 5 步";
    }
    // ── 持续行走 ──
    else if (strcmp(tool_name, "开始行走") == 0 || strcmp(tool_name, "机器狗开始行走") == 0) {
        Dog_WalkStart(); result_text = "开始持续前进";
    } else if (strcmp(tool_name, "停止行走") == 0 || strcmp(tool_name, "机器狗停止行走") == 0) {
        Dog_WalkStop(); result_text = "已停止";
    }
    // ── 复位 / 紧急停止 / 关机 ──
    else if (strcmp(tool_name, "腿部复位") == 0) {
        Dog_ResetAll(); result_text = "已复位";
    } else if (strcmp(tool_name, "紧急停止") == 0) {
        Dog_EmergencyStop(); result_text = "紧急停止完成";
    } else if (strcmp(tool_name, "系统关机") == 0) {
        Dog_Shutdown(); result_text = "关机信号已发送";
    }
    // ── 身体姿态 ──
    else if (strcmp(tool_name, "身体姿态控制") == 0) {
        Dog_BodySway(fb, lr); result_text = "姿态已设置";
    }
    // ── 摇摆（异步） ──
    else if (strcmp(tool_name, "前后摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingFB(cyc, 500); });
        result_text = "前后摇摆中…";
    } else if (strcmp(tool_name, "左右摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2); int d = dir;
        run_action_async([cyc, d]() { Dog_SwingLR(d, cyc, 500); });
        result_text = "左右摇摆中…";
    } else if (strcmp(tool_name, "旋转摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingTwist(cyc, 500); });
        result_text = "旋转摇摆中…";
    } else if (strcmp(tool_name, "上下摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingUpDown(cyc, 500); });
        result_text = "上下摇摆中…";
    } else if (strcmp(tool_name, "左侧侧摇") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingSideLeft(cyc, 500); });
        result_text = "左侧侧摇中…";
    } else if (strcmp(tool_name, "右侧侧摇") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingSideRight(cyc, 500); });
        result_text = "右侧侧摇中…";
    } else if (strcmp(tool_name, "波浪步") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingWave(cyc, 350); });
        result_text = "波浪步中…";
    } else if (strcmp(tool_name, "原地踏步") == 0) {
        int cyc = (cycles > 0 ? cycles : 3);
        run_action_async([cyc]() { Dog_SwingMarch(cyc, 300); });
        result_text = "原地踏步中…";
    } else if (strcmp(tool_name, "侧向点头") == 0) {
        int cyc = (cycles > 0 ? cycles : 3);
        run_action_async([cyc]() { Dog_SwingNod(cyc, 400); });
        result_text = "侧向点头中…";
    } else if (strcmp(tool_name, "颤抖") == 0) {
        int cyc = (cycles > 0 ? cycles : 6);
        run_action_async([cyc]() { Dog_SwingTremble(cyc, 150); });
        result_text = "颤抖中…";
    } else if (strcmp(tool_name, "振荡") == 0) {
        run_action_async([]() { Dog_SwingTremble(33, 150); });
        result_text = "振荡中…（约10秒）";
    }
    // ── 跳舞（异步） ──
    else if (strcmp(tool_name, "长跳舞") == 0) {
        run_action_async([]() { Dog_DanceLong(); });
        result_text = "长跳舞中…";
    } else if (strcmp(tool_name, "短跳舞") == 0) {
        run_action_async([]() { Dog_DanceShort(); });
        result_text = "短跳舞中…";
    } else if (strcmp(tool_name, "随音乐摇摆") == 0) {
        run_action_async([]() { beat_sync_run(); });
        result_text = "麦克风采音中，即将随音乐摇摆…";
    }
    // ── 舞蹈编排 ──
    else if (strcmp(tool_name, "舞蹈编排") == 0) {
        const char* choreo_name = "dance_01";
        if (arguments && cJSON_IsObject(arguments)) {
            cJSON* name_item = cJSON_GetObjectItem(arguments, "名称");
            if (cJSON_IsString(name_item)) choreo_name = name_item->valuestring;
        }
        choreo_routine_t* routine = choreo_load(choreo_name);
        if (routine) {
            run_action_async([routine]() {
                choreo_play_async(routine, NULL);
                while (choreo_is_playing()) { vTaskDelay(pdMS_TO_TICKS(100)); }
            });
            snprintf(result_buf, sizeof(result_buf), "舞蹈编排 %s 执行中…", choreo_name);
            result_text = result_buf;
        } else {
            result_text = "舞蹈编排加载失败";
        }
    }
    // ── 兜底：尝试作为自定义舞步文件名 ──
    else {
        choreo_routine_t* routine = choreo_load(tool_name);
        if (routine) {
            run_action_async([routine]() {
                choreo_play_async(routine, NULL);
                while (choreo_is_playing()) { vTaskDelay(pdMS_TO_TICKS(100)); }
            });
            snprintf(result_buf, sizeof(result_buf), "自定义舞蹈 %s 执行中…", tool_name);
            result_text = result_buf;
        } else {
            ESP_LOGW(TAG, "未知工具: %s", tool_name);
            result_text = "未知命令";
        }
    }

    // 构造 JSON-RPC 响应
    char response[512];
    snprintf(response, sizeof(response),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}],\"isError\":false}}",
        id, result_text);

    SendNotification(response, strlen(response));
}


/* ─────────────── 消息处理任务 ──────────────────────────────────── */

void BleControlServer::_proc_task(void* param) {
    BleControlServer* self = (BleControlServer*)param;

    while (true) {
        char* msg = nullptr;
        if (xQueueReceive(self->rx_queue_, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (msg == nullptr) continue;

        cJSON* root = cJSON_Parse(msg);
        free(msg);

        if (root == nullptr) {
            self->SendNotification(
                "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Invalid JSON\"}}", 54);
            continue;
        }

        // 解析 JSON-RPC 2.0
        cJSON* jsonrpc = cJSON_GetObjectItem(root, "jsonrpc");
        cJSON* method  = cJSON_GetObjectItem(root, "method");
        cJSON* id_item = cJSON_GetObjectItem(root, "id");
        cJSON* params  = cJSON_GetObjectItem(root, "params");

        if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0) {
            self->SendNotification(
                "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Invalid JSONRPC version\"}}", 66);
            cJSON_Delete(root);
            continue;
        }

        if (!cJSON_IsString(method)) {
            self->SendNotification(
                "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Missing method\"}}", 55);
            cJSON_Delete(root);
            continue;
        }

        int id_int = cJSON_IsNumber(id_item) ? id_item->valueint : 0;
        const char* method_str = method->valuestring;

        ESP_LOGI(TAG, "RX: method=%s id=%d", method_str, id_int);

        if (strcmp(method_str, "tools/call") == 0) {
            self->HandleToolCall(
                params ? cJSON_GetObjectItem(params, "name")->valuestring : nullptr,
                params ? cJSON_GetObjectItem(params, "arguments") : nullptr,
                id_int);
        }
        else if (strcmp(method_str, "tools/list") == 0) {
            const char* tools_json =
                "{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":{\"tools\":["
                "{\"name\":\"向前走\",\"description\":\"前进 N 步\"},"
                "{\"name\":\"向后退\",\"description\":\"后退 N 步\"},"
                "{\"name\":\"左转\",\"description\":\"左转 N 步\"},"
                "{\"name\":\"右转\",\"description\":\"右转 N 步\"},"
                "{\"name\":\"开始行走\",\"description\":\"持续前进\"},"
                "{\"name\":\"停止行走\",\"description\":\"停止行走\"},"
                "{\"name\":\"腿部复位\",\"description\":\"全部归位\"},"
                "{\"name\":\"前后摇摆\",\"description\":\"前后摇摆身体\"},"
                "{\"name\":\"左右摇摆\",\"description\":\"左右摇摆身体\"},"
                "{\"name\":\"旋转摇摆\",\"description\":\"身体旋转扭摆\"},"
                "{\"name\":\"上下摇摆\",\"description\":\"蹲起上下摇摆\"},"
                "{\"name\":\"左侧侧摇\",\"description\":\"左腿前后摆右腿内收\"},"
                "{\"name\":\"右侧侧摇\",\"description\":\"右腿前后摆左腿内收\"},"
                "{\"name\":\"波浪步\",\"description\":\"四腿依次前伸波浪感\"},"
                "{\"name\":\"原地踏步\",\"description\":\"对角腿交替踏步\"},"
                "{\"name\":\"侧向点头\",\"description\":\"前腿左右交替前伸\"},"
                "{\"name\":\"颤抖\",\"description\":\"高频微幅颤抖\"},"
                "{\"name\":\"振荡\",\"description\":\"约10秒高频振荡\"},"
                "{\"name\":\"身体姿态控制\",\"description\":\"实时姿态控制（摇杆）\"},"
                "{\"name\":\"系统关机\",\"description\":\"关闭电源\"},"
                "{\"name\":\"紧急停止\",\"description\":\"立即中断所有运动并归位\"},"
                "{\"name\":\"长跳舞\",\"description\":\"8种动作随机顺序各一次\"},"
                "{\"name\":\"短跳舞\",\"description\":\"4种基础摇摆随机取3种\"},"
                "{\"name\":\"舞蹈编排\",\"description\":\"执行自定义舞蹈编排\"},"
                "{\"name\":\"随音乐摇摆\",\"description\":\"麦克风采音检测BPM随机摇摆\"}"
                "]}}";
            self->SendNotification(tools_json, strlen(tools_json));
        }
        else if (strcmp(method_str, "list_custom_dances") == 0) {
            choreo_name_list_t clist = choreo_list_names();
            cJSON* dances_arr = cJSON_CreateArray();
            for (int i = 0; i < clist.count; i++) {
                cJSON* obj = cJSON_CreateObject();
                cJSON_AddStringToObject(obj, "name",     clist.names[i]     ? clist.names[i]     : "");
                cJSON_AddStringToObject(obj, "filename", clist.filenames[i] ? clist.filenames[i] : "");
                cJSON_AddItemToArray(dances_arr, obj);
            }
            choreo_name_list_free(&clist);

            cJSON* resp    = cJSON_CreateObject();
            cJSON* result  = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "jsonrpc", "2.0");
            cJSON_AddNumberToObject(resp, "id", id_int);
            cJSON_AddItemToObject(resp, "result", result);
            cJSON_AddItemToObject(result, "dances", dances_arr);

            char* resp_str = cJSON_PrintUnformatted(resp);
            self->SendNotification(resp_str, strlen(resp_str));
            free(resp_str);
            cJSON_Delete(resp);
        }
        else if (strcmp(method_str, "initialize") == 0) {
            const char* init_json =
                "{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":{\"protocolVersion\":\"2024-11-05\","
                "\"capabilities\":{\"tools\":{}},"
                "\"serverInfo\":{\"name\":\"Dog-Controller-BLE\",\"version\":\"1.0\"}}}";
            self->SendNotification(init_json, strlen(init_json));
        }
        else if (strcmp(method_str, "ping") == 0) {
            self->SendNotification("{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":\"pong\"}", 48);
        }
        else {
            char err[256];
            snprintf(err, sizeof(err),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"message\":\"Method not supported: %s\"}}",
                id_int, method_str);
            self->SendNotification(err, strlen(err));
        }

        cJSON_Delete(root);
    }
}


/* ─────────────── 公共接口 ──────────────────────────────────────── */

BleControlServer::BleControlServer()
#ifdef CONFIG_BT_NIMBLE_ENABLED
    : rx_queue_(nullptr),
      tx_val_handle_(0),
      conn_handle_(BLE_HS_CONN_HANDLE_NONE),
      connected_(false),
      deinit_done_(false),
      proc_task_(nullptr)
#else
    : rx_queue_(nullptr),
      tx_val_handle_(0),
      conn_handle_(0),
      connected_(false),
      deinit_done_(false),
      proc_task_(nullptr)
#endif
{
    instance_ = this;
}

BleControlServer::~BleControlServer() {
    Stop();
    instance_ = nullptr;
}

bool BleControlServer::Start() {
    // 创建消息队列
    rx_queue_ = xQueueCreate(8, sizeof(char*));
    if (rx_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        return false;
    }

    // 创建处理任务
    if (xTaskCreate(_proc_task, "ble_proc", 4096, this, 5, &proc_task_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create processing task");
        vQueueDelete(rx_queue_);
        rx_queue_ = nullptr;
        return false;
    }

#ifdef CONFIG_BT_NIMBLE_ENABLED
    // 初始化 BT Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        goto start_fail;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        esp_bt_controller_deinit();
        goto start_fail;
    }

    // 初始化 NimBLE
    ret = esp_nimble_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE init failed: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        goto start_fail;
    }

    // 配置 NimBLE 回调
    ble_hs_cfg.reset_cb = nimble_on_reset;
    ble_hs_cfg.sync_cb  = nimble_on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;

    // 启动 NimBLE host task
    nimble_port_freertos_init(_nimble_host_task);

    ESP_LOGI(TAG, "BLE Control Server started");
    return true;

start_fail:
    vTaskDelete(proc_task_);
    proc_task_ = nullptr;
    vQueueDelete(rx_queue_);
    rx_queue_ = nullptr;
    return false;
#else
    ESP_LOGW(TAG, "BLE not available (NimBLE disabled in sdkconfig)");
    return false;
#endif
}
