/*
    StackChan head + RGB control exposed to the model over MCP.
*/

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdlib>

#include "body_io_expander.h"
#include "config.h"
#include "mcp_server.h"
#include "scs_servo_bus.h"

#define TAG "StackChanController"

namespace {

enum ActionType {
    kActionMove = 1,
    kActionHome,
    kActionNod,
    kActionShake,
};

struct ActionRequest {
    ActionType type;
    int yaw;
    int pitch;
    int duration_ms;
    int repeat;
};

int Clamp(int value, int min_value, int max_value) {
    return std::max(min_value, std::min(max_value, value));
}

}  // namespace

class StackChanController {
public:
    StackChanController(BodyIoExpander* io_expander)
        : bus_(SERVO_UART_PORT, SERVO_UART_BAUD_RATE, SERVO_UART_TX_PIN, SERVO_UART_RX_PIN),
          io_expander_(io_expander) {
        if (!bus_.IsReady()) {
            ESP_LOGE(TAG, "Servo bus unavailable, MCP tools not registered");
            return;
        }

        bus_.EnableTorque(SERVO_YAW_ID, true);
        bus_.EnableTorque(SERVO_PITCH_ID, true);

        action_queue_ = xQueueCreate(4, sizeof(ActionRequest));
        xTaskCreate(ActionTaskEntry, "stackchan_action", 3072, this, 4, &action_task_);

        ApplyPose(0, 0, 800);
        RegisterMcpTools();
    }

private:
    static void ActionTaskEntry(void* arg) {
        static_cast<StackChanController*>(arg)->ActionTask();
    }

    void ActionTask() {
        ActionRequest request;
        while (true) {
            if (xQueueReceive(action_queue_, &request, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            switch (request.type) {
                case kActionMove:
                    ApplyPose(request.yaw, request.pitch, request.duration_ms);
                    break;
                case kActionHome:
                    ApplyPose(0, 0, request.duration_ms);
                    break;
                case kActionNod:
                    for (int i = 0; i < request.repeat; i++) {
                        ApplyPose(yaw_degree_, 25, 320);
                        ApplyPose(yaw_degree_, 0, 320);
                    }
                    break;
                case kActionShake:
                    for (int i = 0; i < request.repeat; i++) {
                        ApplyPose(-25, pitch_degree_, 280);
                        ApplyPose(25, pitch_degree_, 280);
                    }
                    ApplyPose(0, pitch_degree_, 280);
                    break;
            }
        }
    }

    // Blocks for the requested duration so queued actions never overlap.
    void ApplyPose(int yaw_degree, int pitch_degree, int duration_ms) {
        yaw_degree_ = Clamp(yaw_degree, SERVO_YAW_MIN_DEGREE, SERVO_YAW_MAX_DEGREE);
        pitch_degree_ = Clamp(pitch_degree, SERVO_PITCH_MIN_DEGREE, SERVO_PITCH_MAX_DEGREE);
        duration_ms = Clamp(duration_ms, 100, 5000);

        bus_.WritePosition(SERVO_YAW_ID, DegreeToRaw(SERVO_YAW_ZERO_POS, yaw_degree_), duration_ms);
        bus_.WritePosition(SERVO_PITCH_ID, DegreeToRaw(SERVO_PITCH_ZERO_POS, pitch_degree_), duration_ms);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
    }

    static uint16_t DegreeToRaw(int zero_position, int degree) {
        int raw = zero_position + degree * SERVO_STEPS_PER_DEGREE_NUM / SERVO_STEPS_PER_DEGREE_DEN;
        return static_cast<uint16_t>(Clamp(raw, SERVO_RAW_POS_MIN, SERVO_RAW_POS_MAX));
    }

    bool Enqueue(const ActionRequest& request) {
        return xQueueSend(action_queue_, &request, 0) == pdTRUE;
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.head.set_pose",
            "Turn the head to an absolute pose. yaw is horizontal (negative right, positive left, "
            "0 faces forward), pitch is vertical (0 is level, larger looks further up).",
            PropertyList({
                Property("yaw", kPropertyTypeInteger, 0, SERVO_YAW_MIN_DEGREE, SERVO_YAW_MAX_DEGREE),
                Property("pitch", kPropertyTypeInteger, 0, SERVO_PITCH_MIN_DEGREE, SERVO_PITCH_MAX_DEGREE),
                Property("duration_ms", kPropertyTypeInteger, 600, 100, 5000),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                ActionRequest request{kActionMove, properties["yaw"].value<int>(),
                                      properties["pitch"].value<int>(),
                                      properties["duration_ms"].value<int>(), 1};
                return Enqueue(request);
            });

        mcp_server.AddTool("self.head.home", "Return the head to the forward-facing home pose.",
                           PropertyList(), [this](const PropertyList&) -> ReturnValue {
                               ActionRequest request{kActionHome, 0, 0, 800, 1};
                               return Enqueue(request);
                           });

        mcp_server.AddTool(
            "self.head.nod", "Nod the head to agree or acknowledge.",
            PropertyList({Property("times", kPropertyTypeInteger, 2, 1, 5)}),
            [this](const PropertyList& properties) -> ReturnValue {
                ActionRequest request{kActionNod, 0, 0, 320, properties["times"].value<int>()};
                return Enqueue(request);
            });

        mcp_server.AddTool(
            "self.head.shake", "Shake the head left and right to disagree.",
            PropertyList({Property("times", kPropertyTypeInteger, 2, 1, 5)}),
            [this](const PropertyList& properties) -> ReturnValue {
                ActionRequest request{kActionShake, 0, 0, 280, properties["times"].value<int>()};
                return Enqueue(request);
            });

        if (io_expander_ != nullptr) {
            mcp_server.AddTool(
                "self.body.set_rgb_color",
                "Set the color of the 12 RGB LEDs on the body. Values are 0-255 per channel.",
                PropertyList({
                    Property("red", kPropertyTypeInteger, 0, 0, 255),
                    Property("green", kPropertyTypeInteger, 0, 0, 255),
                    Property("blue", kPropertyTypeInteger, 0, 0, 255),
                }),
                [this](const PropertyList& properties) -> ReturnValue {
                    uint8_t r = properties["red"].value<int>();
                    uint8_t g = properties["green"].value<int>();
                    uint8_t b = properties["blue"].value<int>();
                    for (int i = 0; i < BODY_RGB_LED_COUNT; i++) {
                        io_expander_->SetLedColor(i, r, g, b);
                    }
                    io_expander_->RefreshLeds();
                    return true;
                });
        }
    }

    ScsServoBus bus_;
    BodyIoExpander* io_expander_;
    QueueHandle_t action_queue_ = nullptr;
    TaskHandle_t action_task_ = nullptr;
    int yaw_degree_ = 0;
    int pitch_degree_ = 0;
};

void InitializeStackChanController(BodyIoExpander* io_expander) {
    static StackChanController controller(io_expander);
}
