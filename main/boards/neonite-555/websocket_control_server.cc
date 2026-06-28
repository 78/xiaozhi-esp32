#include "websocket_control_server.h"
#include "mcp_server.h"
#include "dog_control.h"
#include "choreo.h"
#include "servo.h"
#include "beat_sync.h"
#include <esp_log.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/param.h>
#include <lwip/sockets.h>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <map>
#include <functional>

static const char* TAG = "WSControl";

/* ---- 异步动作任务：避免同步阻塞 WebSocket 消息循环 ----
 * 摇摆/跳舞等长时间动作放到独立任务执行，handle_tool_call 立即返回。
 * 这样 WebSocket 可以继续接收下一条消息（如紧急停止），
 * s_emergency_stop 标志会被正在执行的动作检测到并中断。 */
static TaskHandle_t s_action_task_handle = NULL;

/* 动作任务入口 */
static void action_task(void* arg) {
    auto* fn = (std::function<void()>*)arg;
    (*fn)();
    Dog_ResetAll();
    delete fn;
    s_action_task_handle = NULL;
    servo_set_action_task_handle(NULL);
    vTaskDelete(NULL);
}

/* 启动异步动作任务（如果上一个动作还在执行，先不覆盖，让急停先处理） */
static void run_action_async(std::function<void()> action) {
    if (s_action_task_handle != NULL) {
        ESP_LOGW(TAG, "上一个动作还在执行，跳过新动作");
        return;
    }
    auto* p = new std::function<void()>(std::move(action));
    xTaskCreate(action_task, "dog_action", 4096, p, 5, &s_action_task_handle);
    servo_set_action_task_handle(s_action_task_handle);
}

WebSocketControlServer* WebSocketControlServer::instance_ = nullptr;

WebSocketControlServer::WebSocketControlServer() : server_handle_(nullptr) {
    instance_ = this;
}

WebSocketControlServer::~WebSocketControlServer() {
    Stop();
    instance_ = nullptr;
}

// 发送 JSON-RPC 响应到 WebSocket 客户端
static esp_err_t ws_send_response(httpd_req_t *req, const char* json_response) {
    httpd_ws_frame_t ws_frame;
    memset(&ws_frame, 0, sizeof(httpd_ws_frame_t));
    ws_frame.final = true;
    ws_frame.type = HTTPD_WS_TYPE_TEXT;
    ws_frame.payload = (uint8_t*)json_response;
    ws_frame.len = strlen(json_response);
    return httpd_ws_send_frame(req, &ws_frame);
}

// 处理 JSON-RPC 响应（用于格式化 MCP 工具返回）
static void handle_tool_call(httpd_req_t *req, cJSON* params, int id)
{
    if (params == nullptr) {
        const char* err = "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Missing params\"}}";
        ws_send_response(req, err);
        return;
    }

    cJSON* name = cJSON_GetObjectItem(params, "name");
    if (!cJSON_IsString(name)) {
        const char* err = "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Missing tool name\"}}";
        ws_send_response(req, err);
        return;
    }

    const char* tool_name = name->valuestring;

    // 获取 arguments
    cJSON* arguments = cJSON_GetObjectItem(params, "arguments");
    int steps = 3;  // 默认步数
    int fb = 0, lr = 0;
    int cycles = 2, dir = 1;

    // 动态结果文本缓冲区（避免局部变量悬空指针）
    char result_buf[128] = {0};

    if (arguments && cJSON_IsObject(arguments)) {
        cJSON* steps_item = cJSON_GetObjectItem(arguments, "步数");
        if (cJSON_IsNumber(steps_item)) {
            steps = steps_item->valueint;
            if (steps < 1) steps = 1;
            if (steps > 10) steps = 10;
        }

        // 身体姿态控制参数
        cJSON* fb_item = cJSON_GetObjectItem(arguments, "fb");
        if (cJSON_IsNumber(fb_item)) {
            fb = fb_item->valueint;
        }
        cJSON* lr_item = cJSON_GetObjectItem(arguments, "lr");
        if (cJSON_IsNumber(lr_item)) {
            lr = lr_item->valueint;
        }

        // 摇摆参数
        cJSON* cycles_item = cJSON_GetObjectItem(arguments, "cycles");
        if (cJSON_IsNumber(cycles_item)) {
            cycles = cycles_item->valueint;
        }
        cJSON* dir_item = cJSON_GetObjectItem(arguments, "dir");
        if (cJSON_IsNumber(dir_item)) {
            dir = dir_item->valueint;
        }
    }

    ESP_LOGI(TAG, "执行工具: %s, 步数: %d", tool_name, steps);

    // 执行对应的动作
    const char* result_text = "执行完成";

    if (strcmp(tool_name, "机器狗向前走") == 0) {
        Dog_ForwardSteps(steps);
        result_text = "前进完成";
    }
    else if (strcmp(tool_name, "机器狗向后退") == 0) {
        Dog_BackwardSteps(steps);
        result_text = "后退完成";
    }
    else if (strcmp(tool_name, "机器狗左转") == 0) {
        Dog_TurnLeftSteps(steps);
        result_text = "左转完成";
    }
    else if (strcmp(tool_name, "机器狗右转") == 0) {
        Dog_TurnRightSteps(steps);
        result_text = "右转完成";
    }
    else if (strcmp(tool_name, "机器狗开始行走") == 0) {
        Dog_WalkStart();
        result_text = "开始持续前进";
    }
    else if (strcmp(tool_name, "机器狗停止行走") == 0) {
        Dog_WalkStop();
        result_text = "已停止";
    }
    else if (strcmp(tool_name, "腿部复位") == 0) {
        Dog_ResetAll();
        result_text = "已复位";
    }
    else if (strcmp(tool_name, "机器狗向前走1步") == 0) {
        Dog_ForwardSteps(1);
        result_text = "前进 1 步";
    }
    else if (strcmp(tool_name, "机器狗向前走3步") == 0) {
        Dog_ForwardSteps(3);
        result_text = "前进 3 步";
    }
    else if (strcmp(tool_name, "机器狗向前走5步") == 0) {
        Dog_ForwardSteps(5);
        result_text = "前进 5 步";
    }
    // 短名兼容（JS 发送的是短名，如"向前走"）
    else if (strcmp(tool_name, "向前走") == 0) {
        Dog_ForwardSteps(steps);
        result_text = "前进完成";
    }
    else if (strcmp(tool_name, "向后退") == 0) {
        Dog_BackwardSteps(steps);
        result_text = "后退完成";
    }
    else if (strcmp(tool_name, "左转") == 0) {
        Dog_TurnLeftSteps(steps);
        result_text = "左转完成";
    }
    else if (strcmp(tool_name, "右转") == 0) {
        Dog_TurnRightSteps(steps);
        result_text = "右转完成";
    }
    else if (strcmp(tool_name, "开始行走") == 0) {
        Dog_WalkStart();
        result_text = "开始持续前进";
    }
    else if (strcmp(tool_name, "停止行走") == 0) {
        Dog_WalkStop();
        result_text = "已停止";
    }
    else if (strcmp(tool_name, "腿部复位") == 0) {
        Dog_ResetAll();
        result_text = "已复位";
    }
    // 摇摆功能（异步执行，不阻塞 WebSocket 消息循环）
    else if (strcmp(tool_name, "前后摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingFB(cyc, 500); });
        result_text = "前后摇摆中…";
    }
    else if (strcmp(tool_name, "左右摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2); int d = dir;
        run_action_async([cyc, d]() { Dog_SwingLR(d, cyc, 500); });
        result_text = "左右摇摆中…";
    }
    else if (strcmp(tool_name, "旋转摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingTwist(cyc, 500); });
        result_text = "旋转摇摆中…";
    }
    else if (strcmp(tool_name, "上下摇摆") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingUpDown(cyc, 500); });
        result_text = "上下摇摆中…";
    }
    else if (strcmp(tool_name, "左侧侧摇") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingSideLeft(cyc, 500); });
        result_text = "左侧侧摇中…";
    }
    else if (strcmp(tool_name, "右侧侧摇") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingSideRight(cyc, 500); });
        result_text = "右侧侧摇中…";
    }
    else if (strcmp(tool_name, "波浪步") == 0) {
        int cyc = (cycles > 0 ? cycles : 2);
        run_action_async([cyc]() { Dog_SwingWave(cyc, 350); });
        result_text = "波浪步中…";
    }
    else if (strcmp(tool_name, "原地踏步") == 0) {
        int cyc = (cycles > 0 ? cycles : 3);
        run_action_async([cyc]() { Dog_SwingMarch(cyc, 300); });
        result_text = "原地踏步中…";
    }
    else if (strcmp(tool_name, "侧向点头") == 0) {
        int cyc = (cycles > 0 ? cycles : 3);
        run_action_async([cyc]() { Dog_SwingNod(cyc, 400); });
        result_text = "侧向点头中…";
    }
    else if (strcmp(tool_name, "颤抖") == 0) {
        int cyc = (cycles > 0 ? cycles : 6);
        run_action_async([cyc]() { Dog_SwingTremble(cyc, 150); });
        result_text = "颤抖中…";
    }
    else if (strcmp(tool_name, "振荡") == 0) {
        run_action_async([]() { Dog_SwingTremble(33, 150); });
        result_text = "振荡中…（约10秒）";
    }
    else if (strcmp(tool_name, "身体姿态控制") == 0) {
        Dog_BodySway(fb, lr);
        result_text = "姿态已设置";
    }
    else if (strcmp(tool_name, "系统关机") == 0) {
        Dog_Shutdown();
        result_text = "关机信号已发送";
    }
    else if (strcmp(tool_name, "紧急停止") == 0) {
        Dog_EmergencyStop();
        result_text = "紧急停止完成";
    }
    else if (strcmp(tool_name, "长跳舞") == 0) {
        run_action_async([]() { Dog_DanceLong(); });
        result_text = "长跳舞中…";
    }
    else if (strcmp(tool_name, "短跳舞") == 0) {
        run_action_async([]() { Dog_DanceShort(); });
        result_text = "短跳舞中…";
    }
    else if (strcmp(tool_name, "随音乐摇摆") == 0) {
        run_action_async([]() { beat_sync_run(); });
        result_text = "麦克风采音中，即将随音乐摇摆…";
    }
    // 舞蹈编排：支持通用 "舞蹈编排" + 自定义文件名
    else if (strcmp(tool_name, "舞蹈编排") == 0) {
        // 从 arguments 中提取 "名称"
        const char* choreo_name = "dance_01";
        if (arguments && cJSON_IsObject(arguments)) {
            cJSON* name_item = cJSON_GetObjectItem(arguments, "名称");
            if (cJSON_IsString(name_item)) {
                choreo_name = name_item->valuestring;
            }
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
    else {
        // 兜底：尝试作为自定义舞步文件名（/assets/<tool_name>.json）
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

    ws_send_response(req, response);
}

esp_err_t WebSocketControlServer::ws_handler(httpd_req_t *req) {
    if (instance_ == nullptr) {
        return ESP_FAIL;
    }

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        instance_->AddClient(req);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket close frame received");
        instance_->RemoveClient(req);
        free(buf);
        return ESP_OK;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
        // 响应 WebSocket ping → pong（心跳保活）
        httpd_ws_frame_t pong;
        memset(&pong, 0, sizeof(httpd_ws_frame_t));
        pong.final = true;
        pong.type = HTTPD_WS_TYPE_PONG;
        pong.payload = ws_pkt.payload;
        pong.len = ws_pkt.len;
        esp_err_t pong_ret = httpd_ws_send_frame(req, &pong);
        if (pong_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send pong: %d", pong_ret);
        }
        free(buf);
        return ESP_OK;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        if (ws_pkt.len > 0 && buf != nullptr) {
            buf[ws_pkt.len] = '\0';
            instance_->HandleMessage(req, (const char*)buf, ws_pkt.len);
        }
    } else {
        ESP_LOGW(TAG, "Unsupported frame type: %d", ws_pkt.type);
    }

    free(buf);
    return ESP_OK;
}

bool WebSocketControlServer::Start(int port) {
    if (server_handle_) {
        ESP_LOGW(TAG, "WebSocket server already running, stopping old instance first");
        Stop();
        vTaskDelay(pdMS_TO_TICKS(1000));  // 等端口释放
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_open_sockets = 7;
    config.ctrl_port = 32769;
    config.stack_size  = 8192;   /* 防止长操作时栈溢出（默认 4096） */
    config.uri_match_fn = httpd_uri_match_wildcard;  /* 支持通配符匹配 */

    /* 设置 open_fn 确保 SO_REUSEADDR，解决 httpd_stop 后端口未释放的问题 */
    config.open_fn = [](httpd_handle_t hd, int sockfd) -> esp_err_t {
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        return ESP_OK;
    };

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = nullptr,
        .is_websocket = true
    };

    if (httpd_start(&server_handle_, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle_, &ws_uri);
        ESP_LOGI(TAG, "WebSocket server started on port %d", port);
        return true;
    }

    ESP_LOGE(TAG, "Failed to start WebSocket server");
    return false;
}

void WebSocketControlServer::Stop() {
    if (server_handle_) {
        httpd_stop(server_handle_);
        server_handle_ = nullptr;
        clients_.clear();
        ESP_LOGI(TAG, "WebSocket server stopped");
    }
}

void WebSocketControlServer::HandleMessage(httpd_req_t *req, const char* data, size_t len) {
    if (data == nullptr || len == 0) {
        ESP_LOGE(TAG, "Invalid message: data is null or len is 0");
        return;
    }

    if (len > 4096) {
        ESP_LOGE(TAG, "Message too long: %zu bytes", len);
        return;
    }

    // 心跳 ping → 直接回复 pong
    if (len == 4 && strncmp(data, "ping", 4) == 0) {
        httpd_ws_frame_t ws_frame;
        memset(&ws_frame, 0, sizeof(httpd_ws_frame_t));
        ws_frame.final = true;
        ws_frame.type = HTTPD_WS_TYPE_TEXT;
        ws_frame.payload = (uint8_t*)"pong";
        ws_frame.len = 4;
        httpd_ws_send_frame(req, &ws_frame);
        return;
    }

    char* temp_buf = (char*)malloc(len + 1);
    if (temp_buf == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return;
    }
    memcpy(temp_buf, data, len);
    temp_buf[len] = '\0';

    cJSON* root = cJSON_Parse(temp_buf);
    free(temp_buf);

    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        const char* err = "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Invalid JSON\"}}";
        ws_send_response(req, err);
        return;
    }

    // 解析 JSON-RPC 2.0 格式
    cJSON* jsonrpc = cJSON_GetObjectItem(root, "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(root, "method");
    cJSON* id = cJSON_GetObjectItem(root, "id");
    cJSON* params = cJSON_GetObjectItem(root, "params");

    // 验证 jsonrpc 版本
    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version");
        const char* err = "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Invalid JSONRPC version\"}}";
        ws_send_response(req, err);
        cJSON_Delete(root);
        return;
    }

    // 检查 method
    if (!cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        const char* err = "{\"jsonrpc\":\"2.0\",\"error\":{\"message\":\"Missing method\"}}";
        ws_send_response(req, err);
        cJSON_Delete(root);
        return;
    }

    const char* method_str = method->valuestring;
    int id_int = 0;
    if (cJSON_IsNumber(id)) {
        id_int = id->valueint;
    }

    ESP_LOGI(TAG, "收到请求: method=%s, id=%d", method_str, id_int);

    if (strcmp(method_str, "tools/call") == 0) {
        // 将 root 中提取的 id 注入到 params（引用方式，不转移所有权）
        cJSON_AddNumberToObject(params, "__id", id_int);

        // 处理工具调用
        handle_tool_call(req, params, id_int);

        // 清理（只删除注入的 __id，root 由 HandleMessage 统一释放）
        cJSON_DeleteItemFromObject(params, "__id");
    }
    else if (strcmp(method_str, "tools/list") == 0) {
        // 返回工具列表（包含长短名称 + 摇摆功能）
        const char* tools_response =
            "{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":{\"tools\":["
            "{\"name\":\"机器狗向前走\",\"description\":\"前进 N 步\"},"
            "{\"name\":\"机器狗向后退\",\"description\":\"后退 N 步\"},"
            "{\"name\":\"机器狗左转\",\"description\":\"左转 N 步\"},"
            "{\"name\":\"机器狗右转\",\"description\":\"右转 N 步\"},"
            "{\"name\":\"机器狗开始行走\",\"description\":\"持续前进\"},"
            "{\"name\":\"机器狗停止行走\",\"description\":\"停止行走\"},"
            "{\"name\":\"腿部复位\",\"description\":\"全部归位\"},"
            "{\"name\":\"向前走\",\"description\":\"前进 N 步（短名）\"},"
            "{\"name\":\"向后退\",\"description\":\"后退 N 步（短名）\"},"
            "{\"name\":\"左转\",\"description\":\"左转 N 步（短名）\"},"
            "{\"name\":\"右转\",\"description\":\"右转 N 步（短名）\"},"
            "{\"name\":\"开始行走\",\"description\":\"持续前进（短名）\"},"
            "{\"name\":\"停止行走\",\"description\":\"停止行走（短名）\"},"
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
            "{\"name\":\"身体姿态控制\",\"description\":\"实时姿态控制（摇杆）\"},"
            "{\"name\":\"系统关机\",\"description\":\"关闭电源\"},"
            "{\"name\":\"紧急停止\",\"description\":\"立即中断所有运动并归位\"},"
            "{\"name\":\"长跳舞\",\"description\":\"8种动作随机顺序各一次\"},"
            "{\"name\":\"短跳舞\",\"description\":\"4种基础摇摆随机取3种\"},"
            "{\"name\":\"舞蹈编排\",\"description\":\"执行自定义舞蹈编排（/assets/<名称>.json）\"},"
            "{\"name\":\"随音乐摇摆\",\"description\":\"麦克风采音检测BPM，随机四动作对齐节拍摇摆\"}"
            "]}}";
        ws_send_response(req, tools_response);
    }
    else if (strcmp(method_str, "list_custom_dances") == 0) {
        // 返回 /assets/ 下所有自定义舞步名称列表
        choreo_name_list_t clist = choreo_list_names();
        cJSON* dances_arr = cJSON_CreateArray();
        for (int i = 0; i < clist.count; i++) {
            cJSON* obj = cJSON_CreateObject();
            cJSON_AddStringToObject(obj, "name",     clist.names[i]     ? clist.names[i]     : "");
            cJSON_AddStringToObject(obj, "filename", clist.filenames[i] ? clist.filenames[i] : "");
            cJSON_AddItemToArray(dances_arr, obj);
        }
        choreo_name_list_free(&clist);

        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "jsonrpc", "2.0");
        cJSON_AddNumberToObject(resp, "id", id_int);
        cJSON* result = cJSON_CreateObject();
        cJSON_AddItemToObject(result, "dances", dances_arr);
        cJSON_AddItemToObject(resp, "result", result);

        char* resp_str = cJSON_PrintUnformatted(resp);
        ws_send_response(req, resp_str);
        free(resp_str);
        cJSON_Delete(resp);
    }
    else if (strcmp(method_str, "initialize") == 0) {
        // MCP 初始化响应
        const char* init_response =
            "{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":{\"protocolVersion\":\"2024-11-05\","
            "\"capabilities\":{\"tools\":{}},"
            "\"serverInfo\":{\"name\":\"Dog-Controller\",\"version\":\"1.0\"}}}";
        ws_send_response(req, init_response);
    }
    else {
        ESP_LOGW(TAG, "不支持的方法: %s", method_str);
        char err[256];
        snprintf(err, sizeof(err),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"message\":\"Method not supported: %s\"}}",
            id_int, method_str);
        ws_send_response(req, err);
    }

    cJSON_Delete(root);
}

void WebSocketControlServer::AddClient(httpd_req_t *req) {
    int sock_fd = httpd_req_to_sockfd(req);
    if (clients_.find(sock_fd) == clients_.end()) {
        bool is_first = clients_.empty();
        clients_[sock_fd] = req;
        ESP_LOGI(TAG, "Client connected: %d (total: %zu)", sock_fd, clients_.size());
        // 首个客户端连接时触发回调（互斥：关闭 BLE）
        if (is_first && on_connected_cb_) {
            on_connected_cb_();
        }
    }
}

void WebSocketControlServer::RemoveClient(httpd_req_t *req) {
    int sock_fd = httpd_req_to_sockfd(req);
    clients_.erase(sock_fd);
    ESP_LOGI(TAG, "Client disconnected: %d (total: %zu)", sock_fd, clients_.size());
    // 最后一个客户端断开时触发回调（互斥：恢复 BLE）
    if (clients_.empty() && on_disconnected_cb_) {
        on_disconnected_cb_();
    }
}

size_t WebSocketControlServer::GetClientCount() const {
    return clients_.size();
}
