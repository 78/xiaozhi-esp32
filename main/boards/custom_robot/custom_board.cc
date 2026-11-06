#include "wifi_board.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <esp_http_server.h>
#include <cJSON.h>
#include "config.h"
#include <esp_log.h>

#define TAG "CustomRobot"

class CustomRobotBoard : public WifiBoard {
private:
    httpd_handle_t server = NULL;

    void InitializeServos() {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_13_BIT,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = 50, // Tần số 50Hz chuẩn cho MG90S
            .clk_cfg = LEDC_AUTO_CLK
        };
        ledc_timer_config(&ledc_timer);

        ledc_channel_config_t channels[2] = {
            {
                .gpio_num = SERVO_PAN_PIN,
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .channel = LEDC_CHANNEL_0,
                .intr_type = LEDC_INTR_DISABLE,
                .timer_sel = LEDC_TIMER_0,
                .duty = 409, // Vị trí 90 độ ở giữa
                .hpoint = 0
            },
            {
                .gpio_num = SERVO_TILT_PIN,
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .channel = LEDC_CHANNEL_1,
                .intr_type = LEDC_INTR_DISABLE,
                .timer_sel = LEDC_TIMER_0,
                .duty = 409,
                .hpoint = 0
            }
        };
        ledc_channel_config(&channels[0]);
        ledc_channel_config(&channels[1]);
    }

    void InitializePIR() {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << PIR_SENSOR_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_POSEDGE
        };
        gpio_config(&io_conf);

        xTaskCreate([](void* arg) {
            CustomRobotBoard* board = (CustomRobotBoard*)arg;
            while (1) {
                if (gpio_get_level(PIR_SENSOR_PIN) == 1) {
                    ESP_LOGI(TAG, "PIR Motion Detected! Centering head...");
                    board->SetServoAngle(0, 90);
                    board->SetServoAngle(1, 90);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }, "pir_task", 2048, this, 5, NULL);
    }

    static esp_err_t ServoJsonHandler(httpd_req_t *req) {
        char buf[128];
        int ret = httpd_req_recv(req, buf, std::min(req->content_len, sizeof(buf) - 1));
        if (ret <= 0) return ESP_FAIL;
        buf[ret] = '\0';

        cJSON *root = cJSON_Parse(buf);
        if (root) {
            CustomRobotBoard* board = (CustomRobotBoard*)req->user_ctx;
            cJSON *pan = cJSON_GetObjectItem(root, "pan");
            cJSON *tilt = cJSON_GetObjectItem(root, "tilt");
            if (pan && cJSON_IsNumber(pan)) board->SetServoAngle(0, pan->valueint);
            if (tilt && cJSON_IsNumber(tilt)) board->SetServoAngle(1, tilt->valueint);
            cJSON_Delete(root);
        }
        httpd_resp_send(req, "{\"status\":\"success\"}", 19);
        return ESP_OK;
    }

    void StartWebServer() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        if (httpd_start(&server, &config) == ESP_OK) {
            httpd_uri_t servo_uri = {
                .uri      = "/api/servo",
                .method   = HTTP_POST,
                .handler  = ServoJsonHandler,
                .user_ctx = this
            };
            httpd_register_uri_handler(server, &servo_uri);
        }
    }

public:
    CustomRobotBoard() {
        InitializeServos();
        InitializePIR();
        StartWebServer();
    }

    // --- XÓA HOÀN TOÀN LỖI BÁO PIN ẢO ---
    virtual int GetBatteryLevel() override {
        return -1; // Trả về -1 để ẩn icon pin trên màn hình
    }

    void SetServoAngle(int channel, int angle) {
        if (angle < 0) angle = 0;
        if (angle > 180) angle = 180;
        int duty = 164 + (angle * (819 - 164) / 180);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    }
};

DECLARE_BOARD(CustomRobotBoard);
