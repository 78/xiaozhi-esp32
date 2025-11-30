#include "ota_server.h"
#include "application.h"
#include "board.h"
#include "display.h"
#include "assets/lang_config.h"
#include <string>
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "OTA_WEB";

extern const uint8_t ota_index_html_start[] asm("_binary_ota_index_html_start");
extern const uint8_t ota_index_html_end[]   asm("_binary_ota_index_html_end");

const char *OTA_INDEX_HTML = (const char *)ota_index_html_start;
httpd_handle_t server_handle_;

static esp_err_t ota_upload_handler(httpd_req_t *req);
static esp_err_t ota_get_handler(httpd_req_t *req);

void start_ota_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server_handle_, &config) == ESP_OK) {
        httpd_uri_t ota_get = {
            .uri = "/ota",
            .method = HTTP_GET,
            .handler = ota_get_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server_handle_, &ota_get);
        
        httpd_uri_t ota_upload = {
            .uri = "/ota_upload",
            .method = HTTP_POST,
            .handler = ota_upload_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server_handle_, &ota_upload);
        ESP_LOGI(TAG, "OTA Webserver started");
        return;
    }

    ESP_LOGE(TAG, "Failed to start OTA Webserver");
}

static esp_err_t ota_upload_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "=== OTA UPLOAD START ===");
    ESP_LOGI(TAG, "Content length: %d bytes", req->content_len);
    
    // Kiểm tra content length
    if (req->content_len <= 0 || req->content_len > 8 * 1024 * 1024) {
        ESP_LOGE(TAG, "Invalid content length");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"invalid_length\"}");
        return ESP_FAIL;
    }
    
    // ========== HIỂN THỊ UI NGAY TỪ ĐẦU ==========
    auto& app = Application::GetInstance();
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    // Chuyển UI sang trạng thái upgrading (không dùng SetDeviceState trực tiếp)
    app.Schedule([&app, display]() {
        app.Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "download", Lang::Sounds::OGG_UPGRADE);
    });
    
    // Đợi UI cập nhật
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    app.Schedule([display]() {
        display->SetChatMessage("system", "Receiving firmware...");
    });
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Lấy boundary từ Content-Type
    char content_type[128];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get Content-Type");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"no_content_type\"}");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Content-Type: %s", content_type);
    
    char boundary[128] = {0};
    char* boundary_start = strstr(content_type, "boundary=");
    if (boundary_start == NULL) {
        ESP_LOGE(TAG, "No boundary found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"no_boundary\"}");
        return ESP_FAIL;
    }
    
    strcpy(boundary, boundary_start + 9);
    ESP_LOGI(TAG, "Boundary: %s", boundary);
    
    // Chuẩn bị OTA partition
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No update partition found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"no_partition\"}");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to partition: %s at 0x%lx", 
             update_partition->label, update_partition->address);
    
    // Tắt power save mode
    board.SetPowerSaveMode(false);
    
    // Buffer nhỏ để đọc dần
    const size_t buf_size = 4096;
    char* buffer = (char*)malloc(buf_size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"malloc_failed\"}");
        return ESP_FAIL;
    }
    
    // State machine để parse multipart
    enum ParseState {
        LOOKING_FOR_BINARY,
        FOUND_BINARY,
        WRITING_BINARY
    } state = LOOKING_FOR_BINARY;
    
    int total_received = 0;
    int binary_written = 0;
    bool ota_begun = false;
    int64_t last_update_time = esp_timer_get_time();
    size_t recent_bytes = 0;
    
    // Buffer tạm để tìm header
    std::string header_buffer;
    header_buffer.reserve(2048);
    
    // Pattern để tìm
    const char* file_marker = "Content-Type: application/octet-stream";
    const char* header_end = "\r\n\r\n";
    char end_boundary[140];
    snprintf(end_boundary, sizeof(end_boundary), "\r\n--%s", boundary);
    
    ESP_LOGI(TAG, "Starting to receive and parse...");
    
    while (total_received < req->content_len) {
        int remaining = req->content_len - total_received;
        int to_read = remaining > buf_size ? buf_size : remaining;
        
        int ret = httpd_req_recv(req, buffer, to_read);
        
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Receive failed: %d", ret);
            if (ota_begun) esp_ota_abort(ota_handle);
            free(buffer);
            
            app.Schedule([&app]() {
                app.Alert(Lang::Strings::ERROR, "Failed to receive data", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
            });
            
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"recv_failed\"}");
            return ESP_FAIL;
        }
        
        total_received += ret;
        
        // Parse theo state
        if (state == LOOKING_FOR_BINARY) {
            // Thêm vào header buffer để tìm
            header_buffer.append(buffer, ret);
            
            size_t file_pos = header_buffer.find(file_marker);
            if (file_pos != std::string::npos) {
                size_t data_start = header_buffer.find(header_end, file_pos);
                if (data_start != std::string::npos) {
                    data_start += 4; // Skip \r\n\r\n
                    
                    // Kiểm tra magic byte
                    if (data_start < header_buffer.length() && 
                        (unsigned char)header_buffer[data_start] == 0xE9) {
                        
                        ESP_LOGI(TAG, "✅ Found firmware binary at position %zu", data_start);
                        
                        // Cập nhật UI
                        app.Schedule([display]() {
                            display->SetChatMessage("system", "Writing firmware...");
                        });
                        
                        // Begin OTA
                        esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
                            free(buffer);
                            
                            app.Schedule([&app]() {
                                app.Alert(Lang::Strings::ERROR, "OTA begin failed", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
                            });
                            
                            httpd_resp_set_type(req, "application/json");
                            httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"ota_begin_failed\"}");
                            return ESP_FAIL;
                        }
                        ota_begun = true;
                        
                        // Ghi phần binary data đã có trong buffer
                        size_t binary_in_buffer = header_buffer.length() - data_start;
                        err = esp_ota_write(ota_handle, 
                                           (const uint8_t*)header_buffer.data() + data_start, 
                                           binary_in_buffer);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
                            esp_ota_abort(ota_handle);
                            free(buffer);
                            
                            app.Schedule([&app]() {
                                app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
                            });
                            
                            httpd_resp_set_type(req, "application/json");
                            httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"ota_write_failed\"}");
                            return ESP_FAIL;
                        }
                        
                        binary_written += binary_in_buffer;
                        recent_bytes += binary_in_buffer;
                        ESP_LOGI(TAG, "Written initial %zu bytes", binary_in_buffer);
                        
                        state = WRITING_BINARY;
                        header_buffer.clear(); // Giải phóng memory
                    }
                }
            }
            
            // Giữ lại 512 bytes cuối để tránh miss pattern
            if (header_buffer.length() > 2048) {
                header_buffer = header_buffer.substr(header_buffer.length() - 512);
            }
            
        } else if (state == WRITING_BINARY) {
            // Tìm end boundary trong buffer
            std::string check_buf(buffer, ret);
            size_t boundary_pos = check_buf.find(end_boundary);
            
            if (boundary_pos != std::string::npos) {
                // Tìm thấy boundary, chỉ ghi phần trước boundary
                if (boundary_pos > 0) {
                    esp_err_t err = esp_ota_write(ota_handle, (const uint8_t*)buffer, boundary_pos);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
                        esp_ota_abort(ota_handle);
                        free(buffer);
                        
                        app.Schedule([&app]() {
                            app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
                        });
                        
                        httpd_resp_set_type(req, "application/json");
                        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"ota_write_failed\"}");
                        return ESP_FAIL;
                    }
                    binary_written += boundary_pos;
                    recent_bytes += boundary_pos;
                }
                ESP_LOGI(TAG, "✅ Firmware binary complete: %d bytes", binary_written);
                
                // Cập nhật UI lần cuối với 100%
                app.Schedule([display]() {
                    display->SetChatMessage("system", "100% - Complete!");
                });
                
                break;
            } else {
                // Chưa gặp boundary, ghi toàn bộ
                esp_err_t err = esp_ota_write(ota_handle, (const uint8_t*)buffer, ret);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
                    esp_ota_abort(ota_handle);
                    free(buffer);
                    
                    app.Schedule([&app]() {
                        app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
                    });
                    
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"ota_write_failed\"}");
                    return ESP_FAIL;
                }
                binary_written += ret;
                recent_bytes += ret;
            }
            
            // ========== CẬP NHẬT UI MỖI GIÂY ==========
            int64_t current_time = esp_timer_get_time();
            if (current_time - last_update_time >= 1000000) { // 1 second
                // Ước tính firmware size (trừ đi phần header ~200 bytes)
                int estimated_firmware_size = req->content_len - 200;
                int progress = (binary_written * 100) / estimated_firmware_size;
                if (progress > 100) progress = 100;
                
                // Tính speed (bytes/giây)
                size_t speed = recent_bytes;
                
                ESP_LOGI(TAG, "Progress: %d%% (%d bytes), Speed: %u B/s", 
                         progress, binary_written, (unsigned int)speed);
                
                // Cập nhật UI - capture by value để tránh dangling reference
                app.Schedule([display, progress, speed]() {
                    char msg_buffer[32];
                    snprintf(msg_buffer, sizeof(msg_buffer), "%d%% %uKB/s", progress, (unsigned int)(speed / 1024));
                    display->SetChatMessage("system", msg_buffer);
                });
                
                last_update_time = current_time;
                recent_bytes = 0;
            }
        }
    }
    
    free(buffer);
    
    if (!ota_begun || binary_written < 100000) {
        ESP_LOGE(TAG, "Invalid firmware: ota_begun=%d, written=%d", ota_begun, binary_written);
        if (ota_begun) esp_ota_abort(ota_handle);
        
        app.Schedule([&app]() {
            app.Alert(Lang::Strings::ERROR, "Invalid firmware file", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        });
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"invalid_firmware\"}");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "=== FINALIZING OTA ===");
    
    // Hiển thị thông báo đang finalize
    app.Schedule([display]() {
        display->SetChatMessage("system", "Finalizing...");
    });
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        }
        
        app.Schedule([&app]() {
            app.Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        });
        
        board.SetPowerSaveMode(true);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"ota_end_failed\"}");
        return ESP_FAIL;
    }
    
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        
        app.Schedule([&app]() {
            app.Alert(Lang::Strings::ERROR, "Failed to set boot partition", "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        });
        
        board.SetPowerSaveMode(true);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\": false, \"error\": \"set_boot_failed\"}");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ OTA UPDATE SUCCESSFUL!");
    ESP_LOGW(TAG, "Device will reboot in 2 seconds...");
    
    // Hiển thị thông báo thành công
    app.Schedule([display]() {
        display->SetChatMessage("system", "Update successful!\nRebooting...");
    });
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": true}");
    
    // Delay và reboot
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

static esp_err_t ota_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, OTA_INDEX_HTML, ota_index_html_end - ota_index_html_start);
    return ESP_OK;
}