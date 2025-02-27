#include "iot/thing.h"
#include "board.h"
#include "config.h"
#include "my_camera.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "base64.h"
#include <cJSON.h>
#include "application.h"

#define TAG "Camera"

// completion = client.chat.completions.create(
//     model="qwen-omni-turbo",
//     messages=[
//         {
//             "role": "system",
//             "content": [{"type": "text", "text": "You are a helpful assistant."}],
//         },
//         {
//             "role": "user",
//             "content": [
//                 {
//                     "type": "image_url",
//                     "image_url": {"url": f"data:image/png;base64,{base64_image}"},
//                 },
//                 {"type": "text", "text": "图中描绘的是什么景象？"},
//             ],
//         },
//     ],
//     # 设置输出数据的模态，当前支持两种：["text","audio"]、["text"]
//     modalities=["text", "audio"],
//     audio={"voice": "Cherry", "format": "wav"},
//     # stream 必须设置为 True，否则会报错
//     stream=True,
//     stream_options={"include_usage": True},
// )
// response:
// non-streaming
// {
//     "id": "chatcmpl-123",
//     "object": "chat.completion",
//     "created": 1677652288,
//     "model": "gpt-4o-mini",
//     "system_fingerprint": "fp_44709d6fcb",
//     "choices": [{
//       "index": 0,
//       "message": {
//         "role": "assistant",
//         "content": "\n\nThis image shows a wooden boardwalk extending through a lush green marshland.",
//       },
//       "logprobs": null,
//       "finish_reason": "stop"
//     }],
//     "usage": {
//       "prompt_tokens": 9,
//       "completion_tokens": 12,
//       "total_tokens": 21,
//       "completion_tokens_details": {
//         "reasoning_tokens": 0,
//         "accepted_prediction_tokens": 0,
//         "rejected_prediction_tokens": 0
//       }
//     }
// }
// streaming
// {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1694268190,"model":"gpt-4o-mini", "system_fingerprint": "fp_44709d6fcb", "choices":[{"index":0,"delta":{"role":"assistant","content":""},"logprobs":null,"finish_reason":null}]}

// {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1694268190,"model":"gpt-4o-mini", "system_fingerprint": "fp_44709d6fcb", "choices":[{"index":0,"delta":{"content":"Hello"},"logprobs":null,"finish_reason":null}]}

// ....

// {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1694268190,"model":"gpt-4o-mini", "system_fingerprint": "fp_44709d6fcb", "choices":[{"index":0,"delta":{},"logprobs":null,"finish_reason":"stop"}]}

std::string vllm_response = "";

void llm_response_parse(char* data, int data_len)
{
    cJSON *root = cJSON_Parse(data);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON data, err %s", cJSON_GetErrorPtr());
        return;
    }

    cJSON *object = cJSON_GetObjectItem(root, "object");
    if (!object) {
        ESP_LOGE(TAG, "Missing object, data: %s", data);
        cJSON_Delete(root);
        return;
    }

    // 访问 choices 数组
    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (choices != NULL && cJSON_IsArray(choices)) {
        // 访问第一个元素
        cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
        if (first_choice != NULL) {
            // 访问 delta 对象
            cJSON *delta = cJSON_GetObjectItemCaseSensitive(first_choice, "delta");
            if (delta != NULL) {
                // 访问 content 字段
                cJSON *content = cJSON_GetObjectItemCaseSensitive(delta, "content");
                if (content != NULL && cJSON_IsString(content)) {
                    // 打印 content 的值
                    ESP_LOGI(TAG, "Content: %s", content->valuestring);
                    vllm_response.append(content->valuestring);
                }
            }
        }
    }

    cJSON_Delete(root);
}

void vision_response_commit(std::string result)
{
    ESP_LOGI(TAG, "result: %s", result.c_str());
}

esp_err_t response_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;

    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;

    case HTTP_EVENT_ON_HEADER:
        if (evt->data_len) {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER %.*s", evt->data_len, (char *)evt->user_data);
        }
        break;

    case HTTP_EVENT_ON_DATA:
        /*skip the first 6 bytes 'data: '*/
        //ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA");
        llm_response_parse((char*)evt->data + 6, evt->data_len - 6);
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        vision_response_commit(vllm_response);
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;

    default:
        break;
    }

    return ESP_OK;
}

char* create_json(const uint8_t* base64_image)
{
    // 创建根对象
    cJSON *root = cJSON_CreateObject();

    // 添加 model 字段
    cJSON_AddStringToObject(root, "model", "qwen-omni-turbo");

    // 创建 messages 数组
    cJSON *messages = cJSON_CreateArray();

    // 创建第一个消息对象（system 角色）
    cJSON *system_message = cJSON_CreateObject();
    cJSON_AddStringToObject(system_message, "role", "system");

    // 创建 content 数组
    cJSON *system_content = cJSON_CreateArray();
    cJSON *system_text = cJSON_CreateObject();
    cJSON_AddStringToObject(system_text, "type", "text");
    cJSON_AddStringToObject(system_text, "text", "You are a helpful assistant.");
    cJSON_AddItemToArray(system_content, system_text);
    cJSON_AddItemToObject(system_message, "content", system_content);
    cJSON_AddItemToArray(messages, system_message);

    // 创建第二个消息对象（user 角色）
    cJSON *user_message = cJSON_CreateObject();
    cJSON_AddStringToObject(user_message, "role", "user");

    // 创建 content 数组
    cJSON *user_content = cJSON_CreateArray();

    // 创建 image_url 对象
    cJSON *image_url = cJSON_CreateObject();
    cJSON_AddStringToObject(image_url, "type", "image_url");

    // 创建 image_url 的 url 对象
    cJSON *image_url_obj = cJSON_CreateObject();
    size_t size = strlen((char*)base64_image) + 32; //extra 32 bytes to hold the prefix string 'data:image/jpeg;base64,'
    char *url = (char*)malloc(size);
    snprintf(url, size, "data:image/jpeg;base64,%s", (char*)base64_image);
    cJSON_AddStringToObject(image_url_obj, "url", url);
    free(url);
    cJSON_AddItemToObject(image_url, "image_url", image_url_obj);
    cJSON_AddItemToArray(user_content, image_url);

    // 创建 text 对象
    cJSON *user_text = cJSON_CreateObject();
    cJSON_AddStringToObject(user_text, "type", "text");
    cJSON_AddStringToObject(user_text, "text", "图中描绘的是什么景象？如果有常见的图标，请识别出来");
    cJSON_AddItemToArray(user_content, user_text);

    cJSON_AddItemToObject(user_message, "content", user_content);
    cJSON_AddItemToArray(messages, user_message);

    // 将 messages 数组添加到根对象
    cJSON_AddItemToObject(root, "messages", messages);

    // 创建 modalities 数组
    cJSON *modalities = cJSON_CreateArray();
    cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
    //cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));
    cJSON_AddItemToObject(root, "modalities", modalities);

    // 创建 audio 对象
    // cJSON *audio = cJSON_CreateObject();
    // cJSON_AddStringToObject(audio, "voice", "Cherry");
    // cJSON_AddStringToObject(audio, "format", "wav");
    // cJSON_AddItemToObject(root, "audio", audio);

    // 添加 stream 字段
    cJSON_AddTrueToObject(root, "stream");

    // 创建 stream_options 对象
    cJSON *stream_options = cJSON_CreateObject();
    cJSON_AddTrueToObject(stream_options, "include_usage");
    cJSON_AddItemToObject(root, "stream_options", stream_options);

    // 生成 JSON 字符串
    char *json_str = cJSON_Print(root);

    // 释放 cJSON 对象
    cJSON_Delete(root);

    return json_str;
}

esp_err_t llm_vision_request(char *data, int size)
{
    ESP_LOGI(TAG, "raise llm vision request");
    const char *url_request = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";

    esp_http_client_config_t config = {
        .url = url_request,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .event_handler = response_handler,
        .buffer_size = 10 * 1024,
        .is_async = false,
    };
    // Set the headers
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", "Bearer " DASHSCOPE_API_KEY);

    size_t olen = 0;
    uint8_t* base64_image = NULL;

    base64_encode(base64_image, 0, &olen, (uint8_t*)data, size);
    base64_image = (uint8_t*)malloc(olen+8);
    if(!base64_image) {
        ESP_LOGE(TAG, "base64 encode memory alloc failed!");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    base64_encode(base64_image, olen+8, &olen, (uint8_t*)data, size);
    char *json_str = create_json(base64_image);
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    // Send the request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    if(base64_image) {
        free(base64_image);
        base64_image = NULL;
    }

    if(json_str) {
        free(json_str);
        json_str = NULL;
    }

    return ESP_OK;
}

#define RESPONSE_COMMIT_EVENT (1 << 0)

namespace iot {

// 这里仅定义 Camera 的属性和方法，不包含具体的实现
class Camera : public Thing {
private:
    bool status_;
    EventGroupHandle_t event_group_;
#ifdef CAM_PIN_PWDN
    gpio_num_t pwr_ctrl_pin_ = CAM_PIN_PWDN;
#else
    gpio_num_t pwr_ctrl_pin_ = GPIO_NUM_NC;
#endif
    
    void InitializeGpio() {
        if(pwr_ctrl_pin_ != GPIO_NUM_NC) {
            gpio_config_t config = {
                .pin_bit_mask = (1ULL << pwr_ctrl_pin_),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            ESP_ERROR_CHECK(gpio_config(&config));
            gpio_set_level(pwr_ctrl_pin_, 0);
        }        
    }

public:
    Camera() : Thing("Camera", "这是摄像头，也是你的眼睛，可以看到这个真实世界") {
        event_group_ = xEventGroupCreate();
        InitializeGpio();

        // 定义设备的属性
        properties_.AddBooleanProperty("power", "摄像头是否打开", [this]() -> bool {
            ESP_LOGI(TAG, "check camera status");
            return status_;
        });

        properties_.AddStringProperty("vllm_response", "上次拍到的图像的内容", [this]() -> std::string {
            ESP_LOGI(TAG, "get image description %s", vllm_response.c_str());
            std::string response = vllm_response;
            
            // 过滤掉 '\r' 和 '\n'以免生成非法json字串
            response.erase(std::remove_if(response.begin(), response.end(), [](char c) {
                return c == '\r' || c == '\n'; 
            }), response.end());

            /*clear the last content*/
            vllm_response.clear();
            return response;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("TurnOn", "打开摄像头", ParameterList(), [this](const ParameterList& parameters) {
            ESP_LOGI(TAG, "turn on camera");
            status_ = true;
            //gpio_set_level(pwr_ctrl_pin_, 1);
        });

        methods_.AddMethod("TurnOff", "关闭摄像头", ParameterList(), [this](const ParameterList& parameters) {
            ESP_LOGI(TAG, "turn off camera");
            status_ = false;
            //gpio_set_level(pwr_ctrl_pin_, 0);
        });

        methods_.AddMethod("Capture", "拍照", ParameterList(), [this](const ParameterList& parameters) {
            /* TODO: need to move this block in another task rather than the background task in
               Application main loop, otherwise it will block the TTS playback task.
            */
            Application::GetInstance().Schedule([this]() {
                auto& board = Board::GetInstance();
                auto camera = board.GetCamera();
                char* pImageBuf = (char*)camera->Capture("jpeg");
            
                if(pImageBuf) {
            #if 0
                    printf("dump image buffer begin=============\n");
                    for(int i = 0; i < camera->GetBufferSize(); i++) {
                        printf("%02x ", pImageBuf[i]);
                        if((i+1)%16 == 0)
                            printf("\n");
                    }
                    printf("\ndump image buffer end=============\n");
            #endif
                    vllm_response.clear();
                    llm_vision_request(pImageBuf, camera->GetBufferSize());
                    ESP_LOGI(TAG, "Set RESPONSE_COMMIT_EVENT");
                }else{
                    ESP_LOGE(TAG, "capture failed!");
                }
            });
        });
    }

    ~Camera() {
        vEventGroupDelete(event_group_);
    }
};

} // namespace iot

DECLARE_THING(Camera);
