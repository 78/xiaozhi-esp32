#include "ota_server.h"

#include <string>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

#include "application.h"
#include "assets.h"
#include "assets/lang_config.h"
#include "board.h"
#include "display.h"

namespace ota {
namespace {

const char* kTag = "OTA_WEB";

extern const uint8_t ota_index_html_start[] asm("_binary_ota_index_html_start");
extern const uint8_t ota_index_html_end[] asm("_binary_ota_index_html_end");

const char* kOtaIndexHtml = reinterpret_cast<const char*>(ota_index_html_start);

}  // namespace

// Singleton implementation
OtaServer& OtaServer::GetInstance() {
  static OtaServer instance;
  return instance;
}

OtaServer::~OtaServer() {
  Stop();
}

esp_err_t OtaServer::Start(int port) {
  if (server_handle_ != nullptr) {
    ESP_LOGW(kTag, "OTA server already running");
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  esp_err_t ret = httpd_start(&server_handle_, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to start OTA Webserver: %s", esp_err_to_name(ret));
    return ret;
  }

  httpd_uri_t ota_get = {
      .uri = "/ota",
      .method = HTTP_GET,
      .handler = HandleOtaGet,
      .user_ctx = nullptr};
  httpd_register_uri_handler(server_handle_, &ota_get);

  httpd_uri_t ota_upload = {
      .uri = "/ota_upload",
      .method = HTTP_POST,
      .handler = HandleOtaUpload,
      .user_ctx = nullptr};
  httpd_register_uri_handler(server_handle_, &ota_upload);

  httpd_uri_t assets_get = {
      .uri = "/assets",
      .method = HTTP_GET,
      .handler = HandleAssetsGet,
      .user_ctx = nullptr};
  httpd_register_uri_handler(server_handle_, &assets_get);

  httpd_uri_t assets_upload = {
      .uri = "/assets_upload",
      .method = HTTP_POST,
      .handler = HandleAssetsUpload,
      .user_ctx = nullptr};
  httpd_register_uri_handler(server_handle_, &assets_upload);

  ESP_LOGI(kTag, "OTA Webserver started");
  return ESP_OK;
}

void OtaServer::Stop() {
  if (server_handle_ != nullptr) {
    httpd_stop(server_handle_);
    server_handle_ = nullptr;
    ESP_LOGI(kTag, "OTA Webserver stopped");
  }
}

esp_err_t OtaServer::HandleOtaGet(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, kOtaIndexHtml,
                  ota_index_html_end - ota_index_html_start);
  return ESP_OK;
}

esp_err_t OtaServer::HandleOtaUpload(httpd_req_t* req) {
  ESP_LOGI(kTag, "=== OTA UPLOAD START ===");
  ESP_LOGI(kTag, "Content length: %d bytes", req->content_len);

  // Validate content length.
  if (req->content_len <= 0 || req->content_len > 8 * 1024 * 1024) {
    ESP_LOGE(kTag, "Invalid content length");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"invalid_length\"}");
    return ESP_FAIL;
  }

  // Update UI to show upgrade status.
  auto& app = Application::GetInstance();
  auto& board = Board::GetInstance();
  auto display = board.GetDisplay();

  // Switch UI to upgrading state.
  app.Schedule([&app, display]() {
    app.Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "download",
              Lang::Sounds::OGG_UPGRADE);
  });

  // Wait for UI update.
  vTaskDelay(pdMS_TO_TICKS(1500));

  app.Schedule([display]() {
    display->SetChatMessage("system", "Receiving firmware...");
  });

  vTaskDelay(pdMS_TO_TICKS(500));

  // Extract boundary from Content-Type header.
  char content_type[128];
  if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                  sizeof(content_type)) != ESP_OK) {
    ESP_LOGE(kTag, "Failed to get Content-Type");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"no_content_type\"}");
    return ESP_FAIL;
  }

  ESP_LOGI(kTag, "Content-Type: %s", content_type);

  char boundary[128] = {0};
  char* boundary_start = strstr(content_type, "boundary=");
  if (boundary_start == nullptr) {
    ESP_LOGE(kTag, "No boundary found");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"no_boundary\"}");
    return ESP_FAIL;
  }

  strcpy(boundary, boundary_start + 9);
  ESP_LOGI(kTag, "Boundary: %s", boundary);

  // Prepare OTA partition.
  esp_ota_handle_t ota_handle = 0;
  const esp_partition_t* update_partition =
      esp_ota_get_next_update_partition(nullptr);
  if (!update_partition) {
    ESP_LOGE(kTag, "No update partition found");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"no_partition\"}");
    return ESP_FAIL;
  }

  ESP_LOGI(kTag, "Writing to partition: %s at 0x%lx", update_partition->label,
           update_partition->address);

  // Disable power save mode during OTA.
  board.SetPowerSaveMode(false);

  // Allocate buffer for incremental reading.
  const size_t kBufferSize = 4096;
  char* buffer = static_cast<char*>(malloc(kBufferSize));
  if (!buffer) {
    ESP_LOGE(kTag, "Failed to allocate buffer");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"malloc_failed\"}");
    return ESP_FAIL;
  }

  // State machine for parsing multipart data.
  enum class ParseState {
    kLookingForBinary,
    kFoundBinary,
    kWritingBinary
  };
  ParseState state = ParseState::kLookingForBinary;

  int total_received = 0;
  int binary_written = 0;
  bool ota_begun = false;
  int64_t last_update_time = esp_timer_get_time();
  size_t recent_bytes = 0;

  // Temporary buffer for finding headers.
  std::string header_buffer;
  header_buffer.reserve(2048);

  // Patterns to search for.
  const char* kFileMarker = "Content-Type: application/octet-stream";
  const char* kHeaderEnd = "\r\n\r\n";
  char end_boundary[140];
  snprintf(end_boundary, sizeof(end_boundary), "\r\n--%s", boundary);

  ESP_LOGI(kTag, "Starting to receive and parse...");

  while (total_received < req->content_len) {
    int remaining = req->content_len - total_received;
    int to_read = remaining > kBufferSize ? kBufferSize : remaining;

    int ret = httpd_req_recv(req, buffer, to_read);

    if (ret <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }
      ESP_LOGE(kTag, "Receive failed: %d", ret);
      if (ota_begun) {
        esp_ota_abort(ota_handle);
      }
      free(buffer);

      app.Schedule([&app]() {
        app.Alert(Lang::Strings::ERROR, "Failed to receive data",
                  "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
      });

      httpd_resp_set_type(req, "application/json");
      httpd_resp_sendstr(req,
                         "{\"success\": false, \"error\": \"recv_failed\"}");
      return ESP_FAIL;
    }

    total_received += ret;

    // Parse according to current state.
    if (state == ParseState::kLookingForBinary) {
      // Append to header buffer for searching.
      header_buffer.append(buffer, ret);

      size_t file_pos = header_buffer.find(kFileMarker);
      if (file_pos != std::string::npos) {
        size_t data_start = header_buffer.find(kHeaderEnd, file_pos);
        if (data_start != std::string::npos) {
          data_start += 4;  // Skip \r\n\r\n

          // Check for ESP32 binary magic byte.
          if (data_start < header_buffer.length() &&
              static_cast<unsigned char>(header_buffer[data_start]) == 0xE9) {
            ESP_LOGI(kTag, "✅ Found firmware binary at position %zu",
                     data_start);

            // Update UI.
            app.Schedule(
                [display]() { display->SetChatMessage("system", "Writing firmware..."); });

            // Begin OTA operation.
            esp_err_t err = esp_ota_begin(update_partition,
                                          OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
            if (err != ESP_OK) {
              ESP_LOGE(kTag, "esp_ota_begin failed: %s", esp_err_to_name(err));
              free(buffer);

              app.Schedule([&app]() {
                app.Alert(Lang::Strings::ERROR, "OTA begin failed",
                          "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
              });

              httpd_resp_set_type(req, "application/json");
              httpd_resp_sendstr(
                  req, "{\"success\": false, \"error\": \"ota_begin_failed\"}");
              return ESP_FAIL;
            }
            ota_begun = true;

            // Write initial binary data already in buffer.
            size_t binary_in_buffer = header_buffer.length() - data_start;
            err = esp_ota_write(
                ota_handle,
                reinterpret_cast<const uint8_t*>(header_buffer.data()) +
                    data_start,
                binary_in_buffer);
            if (err != ESP_OK) {
              ESP_LOGE(kTag, "esp_ota_write failed: %s",
                       esp_err_to_name(err));
              esp_ota_abort(ota_handle);
              free(buffer);

              app.Schedule([&app]() {
                app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark",
                          Lang::Sounds::OGG_EXCLAMATION);
              });

              httpd_resp_set_type(req, "application/json");
              httpd_resp_sendstr(
                  req, "{\"success\": false, \"error\": \"ota_write_failed\"}");
              return ESP_FAIL;
            }

            binary_written += binary_in_buffer;
            recent_bytes += binary_in_buffer;
            ESP_LOGI(kTag, "Written initial %zu bytes", binary_in_buffer);

            state = ParseState::kWritingBinary;
            header_buffer.clear();  // Free memory.
          }
        }
      }

      // Keep last 512 bytes to avoid missing patterns.
      if (header_buffer.length() > 2048) {
        header_buffer = header_buffer.substr(header_buffer.length() - 512);
      }

    } else if (state == ParseState::kWritingBinary) {
      // Search for end boundary in buffer.
      std::string check_buf(buffer, ret);
      size_t boundary_pos = check_buf.find(end_boundary);

      if (boundary_pos != std::string::npos) {
        // Found boundary, only write data before it.
        if (boundary_pos > 0) {
          esp_err_t err = esp_ota_write(
              ota_handle, reinterpret_cast<const uint8_t*>(buffer),
              boundary_pos);
          if (err != ESP_OK) {
            ESP_LOGE(kTag, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            free(buffer);

            app.Schedule([&app]() {
              app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark",
                        Lang::Sounds::OGG_EXCLAMATION);
            });

            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(
                req, "{\"success\": false, \"error\": \"ota_write_failed\"}");
            return ESP_FAIL;
          }
          binary_written += boundary_pos;
          recent_bytes += boundary_pos;
        }
        ESP_LOGI(kTag, "✅ Firmware binary complete: %d bytes", binary_written);

        // Update UI with 100% completion.
        app.Schedule(
            [display]() { display->SetChatMessage("system", "100% - Complete!"); });

        break;
      } else {
        // No boundary yet, write entire buffer.
        esp_err_t err = esp_ota_write(ota_handle,
                                      reinterpret_cast<const uint8_t*>(buffer), ret);
        if (err != ESP_OK) {
          ESP_LOGE(kTag, "esp_ota_write failed: %s", esp_err_to_name(err));
          esp_ota_abort(ota_handle);
          free(buffer);

          app.Schedule([&app]() {
            app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark",
                      Lang::Sounds::OGG_EXCLAMATION);
          });

          httpd_resp_set_type(req, "application/json");
          httpd_resp_sendstr(
              req, "{\"success\": false, \"error\": \"ota_write_failed\"}");
          return ESP_FAIL;
        }
        binary_written += ret;
        recent_bytes += ret;
      }

      // Update UI periodically (every second).
      int64_t current_time = esp_timer_get_time();
      if (current_time - last_update_time >= 1000000) {  // 1 second
        // Estimate firmware size (minus header ~200 bytes).
        int estimated_firmware_size = req->content_len - 200;
        int progress = (binary_written * 100) / estimated_firmware_size;
        if (progress > 100) {
          progress = 100;
        }

        // Calculate speed (bytes/sec).
        size_t speed = recent_bytes;

        ESP_LOGI(kTag, "Progress: %d%% (%d bytes), Speed: %u B/s", progress,
                 binary_written, static_cast<unsigned int>(speed));

        // Update UI - capture by value to avoid dangling references.
        app.Schedule([display, progress, speed]() {
          char msg_buffer[32];
          snprintf(msg_buffer, sizeof(msg_buffer), "%d%% %uKB/s", progress,
                   static_cast<unsigned int>(speed / 1024));
          display->SetChatMessage("system", msg_buffer);
        });

        last_update_time = current_time;
        recent_bytes = 0;
      }
    }
  }

  free(buffer);

  // Validate firmware was received properly.
  if (!ota_begun || binary_written < 100000) {
    ESP_LOGE(kTag, "Invalid firmware: ota_begun=%d, written=%d", ota_begun,
             binary_written);
    if (ota_begun) {
      esp_ota_abort(ota_handle);
    }

    app.Schedule([&app]() {
      app.Alert(Lang::Strings::ERROR, "Invalid firmware file", "circle_xmark",
                Lang::Sounds::OGG_EXCLAMATION);
    });

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"invalid_firmware\"}");
    return ESP_FAIL;
  }

  ESP_LOGI(kTag, "=== FINALIZING OTA ===");

  // Display finalizing message.
  app.Schedule(
      [display]() { display->SetChatMessage("system", "Finalizing..."); });

  vTaskDelay(pdMS_TO_TICKS(500));

  esp_err_t err = esp_ota_end(ota_handle);
  if (err != ESP_OK) {
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      ESP_LOGE(kTag, "Image validation failed");
    } else {
      ESP_LOGE(kTag, "esp_ota_end failed: %s", esp_err_to_name(err));
    }

    app.Schedule([&app]() {
      app.Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED,
                "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
    });

    board.SetPowerSaveMode(true);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"ota_end_failed\"}");
    return ESP_FAIL;
  }

  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "esp_ota_set_boot_partition failed: %s",
             esp_err_to_name(err));

    app.Schedule([&app]() {
      app.Alert(Lang::Strings::ERROR, "Failed to set boot partition",
                "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
    });

    board.SetPowerSaveMode(true);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(
        req, "{\"success\": false, \"error\": \"set_boot_failed\"}");
    return ESP_FAIL;
  }

  ESP_LOGI(kTag, "✅ OTA UPDATE SUCCESSFUL!");
  ESP_LOGW(kTag, "Device will reboot in 2 seconds...");

  // Display success message.
  app.Schedule([display]() {
    display->SetChatMessage("system", "Update successful!\nRebooting...");
  });

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"success\": true}");

  // Delay and reboot.
  vTaskDelay(pdMS_TO_TICKS(2000));
  esp_restart();

  return ESP_OK;
}

esp_err_t OtaServer::HandleAssetsGet(httpd_req_t* req) {
  const char* assets_html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Assets Update</title>
    <style>
        :root {
            --bg-main: #02030b;
            --bg-card: #05081c;
            --bg-card-soft: #070b26;
            --accent: #00ffd0;
            --accent-2: #5f89ff;
            --text-main: #f8f9ff;
            --text-sub: #a8b0c8;
            --border-soft: rgba(255, 255, 255, 0.12);
            --danger: #ff4d4f;
        }

        * { box-sizing: border-box; }

        body {
            margin: 0;
            padding: 20px;
            font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", sans-serif;
            background:
                radial-gradient(circle at top, #151b3c 0, #02030b 55%, #000000 100%),
                #000000;
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .container {
            background:
                radial-gradient(circle at 0 0, rgba(0, 255, 200, 0.18), transparent 60%),
                radial-gradient(circle at 100% 0, rgba(95, 137, 255, 0.18), transparent 55%),
                var(--bg-card);
            border-radius: 22px;
            padding: 22px 20px 18px;
            box-shadow:
                0 24px 60px rgba(0, 0, 0, 0.9),
                0 0 0 1px rgba(255, 255, 255, 0.04);
            border: 1px solid rgba(0, 255, 208, 0.22);
            max-width: 600px;
            width: 100%;
        }

        h1 {
            color: var(--text-main);
            text-align: center;
            margin: 0 0 14px 0;
            font-size: 22px;
            letter-spacing: 0.02em;
        }

        .upload-box {
            border: 1px dashed rgba(0, 255, 200, 0.55);
            border-radius: 18px;
            padding: 28px 16px;
            text-align: center;
            margin-bottom: 14px;
            background: rgba(0, 0, 0, 0.45);
            cursor: pointer;
            transition: border-color 0.16s ease, background 0.16s ease, transform 0.06s ease;
        }
        .upload-box:hover { background: rgba(255, 255, 255, 0.06); transform: translateY(-0.5px); }
        .upload-box.dragover { background: rgba(95, 137, 255, 0.12); }
        .upload-icon { font-size: 42px; margin-bottom: 8px; }
        .upload-text { color: var(--text-main); font-size: 14px; margin-bottom: 4px; }
        .upload-hint { color: var(--text-sub); font-size: 12px; }
        input[type="file"] { display: none; }

        .file-info {
            background: rgba(3, 6, 22, 0.86);
            border-radius: 14px;
            padding: 12px;
            margin-bottom: 14px;
            display: none;
            border: 1px solid rgba(255, 255, 255, 0.06);
        }
        .file-info.show { display: block; }
        .file-name { color: var(--text-main); font-weight: 600; word-break: break-all; }
        .file-size { color: var(--text-sub); font-size: 12px; margin-top: 4px; }

        .progress-container { display: none; margin-bottom: 14px; }
        .progress-container.show { display: block; }
        .progress-text {
            font-size: 13px;
            color: var(--text-sub);
            margin-bottom: 6px;
            display: flex;
            justify-content: space-between;
        }
        .progress-bar {
            width: 100%;
            height: 10px;
            background: rgba(255, 255, 255, 0.12);
            border-radius: 6px;
            overflow: hidden;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(135deg, var(--accent), var(--accent-2));
            width: 0%;
            transition: width 0.3s ease;
        }

        .buttons { display: flex; gap: 10px; margin-top: 14px; }
        button {
            flex: 1;
            padding: 10px 12px;
            border: none;
            border-radius: 999px;
            font-size: 14px;
            font-weight: 700;
            cursor: pointer;
            transition: background 0.16s ease, box-shadow 0.16s ease, transform 0.06s ease;
        }
        .btn-upload {
            background: linear-gradient(135deg, var(--accent), var(--accent-2));
            color: #02040d;
            box-shadow: 0 12px 28px rgba(0, 255, 200, 0.35);
        }
        .btn-upload:hover:not(:disabled) { box-shadow: 0 16px 34px rgba(0, 255, 200, 0.55); transform: translateY(-0.5px); }
        .btn-upload:disabled { background: linear-gradient(135deg, #555d78, #30374d); cursor: not-allowed; box-shadow: none; opacity: 0.75; }
        .btn-cancel { background: rgba(255, 255, 255, 0.14); color: var(--text-main); border: 1px solid rgba(255, 255, 255, 0.16); }
        .btn-cancel:hover { background: rgba(255, 255, 255, 0.22); }

        .status {
            padding: 12px;
            border-radius: 12px;
            margin-bottom: 14px;
            display: none;
            text-align: center;
            font-weight: 600;
            border: 1px solid rgba(255, 255, 255, 0.12);
        }
        .status.show { display: block; }
        .status.success { background: rgba(0, 255, 171, 0.12); color: var(--text-main); border-color: rgba(0, 255, 171, 0.35); }
        .status.error { background: rgba(255, 77, 79, 0.12); color: var(--text-main); border-color: rgba(255, 77, 79, 0.35); }
        .status.info { background: rgba(95, 137, 255, 0.12); color: var(--text-main); border-color: rgba(95, 137, 255, 0.35); }

        .info-box {
            background:
                radial-gradient(circle at top left, rgba(0, 255, 200, 0.08), transparent 65%),
                radial-gradient(circle at bottom right, rgba(95, 137, 255, 0.12), transparent 55%),
                var(--bg-card-soft);
            border-left: 4px solid var(--accent);
            padding: 12px;
            border-radius: 14px;
            font-size: 12px;
            color: var(--text-sub);
            line-height: 1.6;
            margin-top: 14px;
            border: 1px solid rgba(255, 255, 255, 0.06);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>📦 Assets Update</h1>
        
        <div class="upload-box" id="uploadArea">
            <div class="upload-icon">🎨</div>
            <div class="upload-text">Click or drag file here</div>
            <div class="upload-hint">Supported: .bin files only</div>
            <input type="file" id="fileInput" accept=".bin" />
        </div>
        
        <div class="file-info" id="fileInfo">
            <div class="file-name" id="fileName"></div>
            <div class="file-size" id="fileSize"></div>
        </div>
        
        <div class="status" id="status"></div>
        
        <div class="progress-container" id="progressContainer">
            <div class="progress-text">
                <span id="progressText">Uploading...</span>
                <span id="progressPercent">0%</span>
            </div>
            <div class="progress-bar">
                <div class="progress-fill" id="progressFill"></div>
            </div>
        </div>
        
        <div class="buttons">
            <button class="btn-upload" id="uploadBtn" onclick="uploadFile()">Upload & Update</button>
            <button class="btn-cancel" id="cancelBtn" onclick="cancelUpload()" style="display:none;">Cancel</button>
        </div>
        
        <div class="info-box">
            <strong>⚠️ Important:</strong><br>
            • File must be valid assets binary (.bin)<br>
            • Do not turn off device during update<br>
            • Update takes 1-3 minutes<br>
            • Device will apply assets after upload
        </div>
    </div>

    <script>
        let selectedFile = null;
        let uploadInProgress = false;

        const uploadArea = document.getElementById('uploadArea');
        const fileInput = document.getElementById('fileInput');
        const fileInfo = document.getElementById('fileInfo');
        const fileName = document.getElementById('fileName');
        const fileSize = document.getElementById('fileSize');
        const uploadBtn = document.getElementById('uploadBtn');
        const cancelBtn = document.getElementById('cancelBtn');
        const status = document.getElementById('status');
        const progressContainer = document.getElementById('progressContainer');
        const progressFill = document.getElementById('progressFill');
        const progressText = document.getElementById('progressText');
        const progressPercent = document.getElementById('progressPercent');

        uploadArea.addEventListener('click', () => fileInput.click());

        uploadArea.addEventListener('dragover', (e) => {
            e.preventDefault();
            uploadArea.classList.add('dragover');
        });

        uploadArea.addEventListener('dragleave', () => {
            uploadArea.classList.remove('dragover');
        });

        uploadArea.addEventListener('drop', (e) => {
            e.preventDefault();
            uploadArea.classList.remove('dragover');
            if (e.dataTransfer.files.length > 0) {
                selectFile(e.dataTransfer.files[0]);
            }
        });

        fileInput.addEventListener('change', (e) => {
            if (e.target.files.length > 0) {
                selectFile(e.target.files[0]);
            }
        });

        function selectFile(file) {
            if (!file.name.endsWith('.bin')) {
                showStatus('❌ Only .bin files supported', 'error');
                return;
            }
            
            selectedFile = file;
            fileName.textContent = '📄 ' + file.name;
            fileSize.textContent = 'Size: ' + formatFileSize(file.size);
            fileInfo.classList.add('show');
            uploadBtn.disabled = false;
        }

        function formatFileSize(bytes) {
            if (bytes === 0) return '0 Bytes';
            const k = 1024;
            const sizes = ['Bytes', 'KB', 'MB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
        }

        function uploadFile() {
            if (!selectedFile) {
                showStatus('❌ Please select a file', 'error');
                return;
            }

            uploadInProgress = true;
            uploadBtn.disabled = true;
            uploadBtn.style.display = 'none';
            cancelBtn.style.display = 'block';
            progressContainer.classList.add('show');
            status.classList.remove('show');

            const formData = new FormData();
            formData.append('file', selectedFile);

            const xhr = new XMLHttpRequest();

            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    progressFill.style.width = percentComplete + '%';
                    progressPercent.textContent = Math.round(percentComplete) + '%';
                    progressText.textContent = 'Uploading: ' + formatFileSize(e.loaded) + ' / ' + formatFileSize(e.total);
                }
            });

            xhr.addEventListener('load', () => {
                if (xhr.status === 200) {
                    try {
                        const resp = JSON.parse(xhr.responseText);
                        if (resp.success) {
                            progressText.textContent = 'Processing assets...';
                            progressPercent.textContent = '100%';
                            progressFill.style.width = '100%';
                            
                            setTimeout(() => {
                                showStatus('✅ Upload successful! Assets updated.', 'success');
                                setTimeout(() => {
                                    location.reload();
                                }, 3000);
                            }, 1000);
                        } else {
                            showStatus('❌ Update failed: ' + (resp.error || 'Unknown error'), 'error');
                            resetUpload();
                        }
                    } catch (e) {
                        showStatus('❌ Invalid response', 'error');
                        resetUpload();
                    }
                } else {
                    showStatus('❌ Upload failed: ' + xhr.status, 'error');
                    resetUpload();
                }
            });

            xhr.addEventListener('error', () => {
                showStatus('❌ Connection error', 'error');
                resetUpload();
            });

            xhr.open('POST', '/assets_upload');
            xhr.send(formData);
            showStatus('⏳ Uploading...', 'info');
        }

        function cancelUpload() {
            resetUpload();
            showStatus('⏸️ Upload cancelled', 'info');
        }

        function resetUpload() {
            uploadInProgress = false;
            uploadBtn.disabled = false;
            uploadBtn.style.display = 'block';
            cancelBtn.style.display = 'none';
            progressContainer.classList.remove('show');
            progressFill.style.width = '0%';
            progressPercent.textContent = '0%';
        }

        function showStatus(message, type) {
            status.textContent = message;
            status.className = 'status show ' + type;
        }
    </script>
</body>
</html>
)html";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_sendstr(req, assets_html);
  return ESP_OK;
}

esp_err_t OtaServer::HandleAssetsUpload(httpd_req_t* req) {
  ESP_LOGI(kTag, "=== ASSETS UPLOAD START ===");
  ESP_LOGI(kTag, "Content length: %d bytes", req->content_len);

  // Validate content length.
  if (req->content_len <= 0 || req->content_len > 8 * 1024 * 1024) {
    ESP_LOGE(kTag, "Invalid content length");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"invalid_length\"}");
    return ESP_FAIL;
  }

  // Get assets partition.
  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, "assets");
  if (partition == nullptr) {
    ESP_LOGE(kTag, "No assets partition found");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"no_partition\"}");
    return ESP_FAIL;
  }

  if (req->content_len > partition->size) {
    ESP_LOGE(kTag, "Assets file size (%d) is larger than partition size (%lu)",
             req->content_len, partition->size);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"file_too_large\"}");
    return ESP_FAIL;
  }

  // Update UI.
  auto& app = Application::GetInstance();
  auto& board = Board::GetInstance();
  auto display = board.GetDisplay();

  // Switch UI to upgrading state.
  app.Schedule([&app, display]() {
    app.Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "download",
              Lang::Sounds::OGG_UPGRADE);
  });

    // Wait for UI update.
  vTaskDelay(pdMS_TO_TICKS(1500));

  app.Schedule([display]() {
    display->SetChatMessage("system", "Receiving assets...");
  });

  vTaskDelay(pdMS_TO_TICKS(500));

  // Extract boundary from Content-Type header.
  char content_type[128];
  if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                  sizeof(content_type)) != ESP_OK) {
    ESP_LOGE(kTag, "Failed to get Content-Type");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"no_content_type\"}");
    return ESP_FAIL;
  }

  char boundary[128] = {0};
  char* boundary_start = strstr(content_type, "boundary=");
  if (boundary_start == nullptr) {
    ESP_LOGE(kTag, "No boundary found");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"no_boundary\"}");
    return ESP_FAIL;
  }

  strcpy(boundary, boundary_start + 9);
  ESP_LOGI(kTag, "Boundary: %s", boundary);

  // Disable power save mode during upload.
  board.SetPowerSaveMode(false);

  // Allocate buffer for incremental reading.
  const size_t kBufferSize = 4096;
  char* buffer = static_cast<char*>(malloc(kBufferSize));
  if (!buffer) {
    ESP_LOGE(kTag, "Failed to allocate buffer");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"malloc_failed\"}");
    return ESP_FAIL;
  }

  // Get sector size.
  const size_t kSectorSize = esp_partition_get_main_flash_sector_size();

  // State machine for parsing multipart data.
  enum class ParseState { kLookingForBinary, kFoundBinary, kWritingBinary };
  ParseState state = ParseState::kLookingForBinary;

  int total_received = 0;
  int binary_written = 0;
  size_t current_sector = 0;
  int64_t last_update_time = esp_timer_get_time();
  size_t recent_bytes = 0;

  // Temporary buffer for finding headers.
  std::string header_buffer;
  header_buffer.reserve(2048);

  // Patterns to search for.
  const char* kFileMarker = "Content-Type: application/octet-stream";
  const char* kHeaderEnd = "\r\n\r\n";
  char end_boundary[140];
  snprintf(end_boundary, sizeof(end_boundary), "\r\n--%s", boundary);

  ESP_LOGI(kTag, "Starting to receive and parse...");

  while (total_received < req->content_len) {
    int remaining = req->content_len - total_received;
    int to_read = remaining > kBufferSize ? kBufferSize : remaining;

    int ret = httpd_req_recv(req, buffer, to_read);

    if (ret <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }
      ESP_LOGE(kTag, "Receive failed: %d", ret);
      free(buffer);

      app.Schedule([&app]() {
        app.Alert(Lang::Strings::ERROR, "Failed to receive data",
                  "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
      });

      httpd_resp_set_type(req, "application/json");
      httpd_resp_sendstr(req,
                         "{\"success\": false, \"error\": \"recv_failed\"}");
      return ESP_FAIL;
    }

    total_received += ret;

    // Parse according to current state.
    if (state == ParseState::kLookingForBinary) {
      // Append to header buffer for searching.
      header_buffer.append(buffer, ret);

      size_t file_pos = header_buffer.find(kFileMarker);
      if (file_pos != std::string::npos) {
        size_t data_start = header_buffer.find(kHeaderEnd, file_pos);
        if (data_start != std::string::npos) {
          data_start += 4;  // Skip \r\n\r\n

          ESP_LOGI(kTag, "✅ Found assets binary at position %zu", data_start);

          // Update UI.
          app.Schedule([display]() {
            display->SetChatMessage("system", "Writing assets...");
          });

          // Write initial binary data already in buffer.
          size_t binary_in_buffer = header_buffer.length() - data_start;
          const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(
              header_buffer.data() + data_start);

          // Erase and write sectors as needed.
          size_t write_end_offset = binary_written + binary_in_buffer;
          size_t needed_sectors =
              (write_end_offset + kSectorSize - 1) / kSectorSize;

          while (current_sector < needed_sectors) {
            size_t sector_start = current_sector * kSectorSize;
            esp_err_t err =
                esp_partition_erase_range(partition, sector_start, kSectorSize);
            if (err != ESP_OK) {
              ESP_LOGE(kTag, "Failed to erase sector %zu: %s", current_sector,
                       esp_err_to_name(err));
              free(buffer);
              httpd_resp_set_type(req, "application/json");
              httpd_resp_sendstr(
                  req,
                  "{\"success\": false, \"error\": \"erase_failed\"}");
              return ESP_FAIL;
            }
            current_sector++;
          }

          esp_err_t err = esp_partition_write(partition, binary_written,
                                              data_ptr, binary_in_buffer);
          if (err != ESP_OK) {
            ESP_LOGE(kTag, "esp_partition_write failed: %s",
                     esp_err_to_name(err));
            free(buffer);

            app.Schedule([&app]() {
              app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark",
                        Lang::Sounds::OGG_EXCLAMATION);
            });

            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(
                req, "{\"success\": false, \"error\": \"write_failed\"}");
            return ESP_FAIL;
          }

          binary_written += binary_in_buffer;
          recent_bytes += binary_in_buffer;
          ESP_LOGI(kTag, "Written initial %zu bytes", binary_in_buffer);

          state = ParseState::kWritingBinary;
          header_buffer.clear();  // Free memory.
        }
      }

      // Keep last 512 bytes to avoid missing patterns.
      if (header_buffer.length() > 2048) {
        header_buffer = header_buffer.substr(header_buffer.length() - 512);
      }

    } else if (state == ParseState::kWritingBinary) {
      // Search for end boundary in buffer.
      std::string check_buf(buffer, ret);
      size_t boundary_pos = check_buf.find(end_boundary);

      if (boundary_pos != std::string::npos) {
        // Found boundary, only write data before it.
        if (boundary_pos > 0) {
          // Erase sectors as needed.
          size_t write_end_offset = binary_written + boundary_pos;
          size_t needed_sectors =
              (write_end_offset + kSectorSize - 1) / kSectorSize;

          while (current_sector < needed_sectors) {
            size_t sector_start = current_sector * kSectorSize;
            esp_err_t err = esp_partition_erase_range(partition, sector_start,
                                                      kSectorSize);
            if (err != ESP_OK) {
              ESP_LOGE(kTag, "Failed to erase sector %zu: %s", current_sector,
                       esp_err_to_name(err));
              free(buffer);
              httpd_resp_set_type(req, "application/json");
              httpd_resp_sendstr(
                  req,
                  "{\"success\": false, \"error\": \"erase_failed\"}");
              return ESP_FAIL;
            }
            current_sector++;
          }

          esp_err_t err =
              esp_partition_write(partition, binary_written,
                                  reinterpret_cast<const uint8_t*>(buffer),
                                  boundary_pos);
          if (err != ESP_OK) {
            ESP_LOGE(kTag, "esp_partition_write failed: %s",
                     esp_err_to_name(err));
            free(buffer);

            app.Schedule([&app]() {
              app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark",
                        Lang::Sounds::OGG_EXCLAMATION);
            });

            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(
                req, "{\"success\": false, \"error\": \"write_failed\"}");
            return ESP_FAIL;
          }
          binary_written += boundary_pos;
          recent_bytes += boundary_pos;
        }
        ESP_LOGI(kTag, "✅ Assets binary complete: %d bytes", binary_written);

        // Update UI with 100% completion.
        app.Schedule([display]() {
          display->SetChatMessage("system", "100% - Complete!");
        });

        break;
      } else {
        // No boundary yet, write entire buffer.
        // Erase sectors as needed.
        size_t write_end_offset = binary_written + ret;
        size_t needed_sectors =
            (write_end_offset + kSectorSize - 1) / kSectorSize;

        while (current_sector < needed_sectors) {
          size_t sector_start = current_sector * kSectorSize;
          esp_err_t err =
              esp_partition_erase_range(partition, sector_start, kSectorSize);
          if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to erase sector %zu: %s", current_sector,
                     esp_err_to_name(err));
            free(buffer);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(
                req, "{\"success\": false, \"error\": \"erase_failed\"}");
            return ESP_FAIL;
          }
          current_sector++;
        }

        esp_err_t err =
            esp_partition_write(partition, binary_written,
                                reinterpret_cast<const uint8_t*>(buffer), ret);
        if (err != ESP_OK) {
          ESP_LOGE(kTag, "esp_partition_write failed: %s",
                   esp_err_to_name(err));
          free(buffer);

          app.Schedule([&app]() {
            app.Alert(Lang::Strings::ERROR, "Write failed", "circle_xmark",
                      Lang::Sounds::OGG_EXCLAMATION);
          });

          httpd_resp_set_type(req, "application/json");
          httpd_resp_sendstr(
              req, "{\"success\": false, \"error\": \"write_failed\"}");
          return ESP_FAIL;
        }
        binary_written += ret;
        recent_bytes += ret;
      }

      // Update UI periodically (every second).
      int64_t current_time = esp_timer_get_time();
      if (current_time - last_update_time >= 1000000) {  // 1 second
        int progress = (binary_written * 100) / req->content_len;
        if (progress > 100) {
          progress = 100;
        }

        size_t speed = recent_bytes;

        ESP_LOGI(kTag, "Progress: %d%% (%d bytes), Speed: %u B/s", progress,
                 binary_written, static_cast<unsigned int>(speed));

        // Update UI - capture by value to avoid dangling references.
        app.Schedule([display, progress, speed]() {
          char msg_buffer[32];
          snprintf(msg_buffer, sizeof(msg_buffer), "%d%% %uKB/s", progress,
                   static_cast<unsigned int>(speed / 1024));
          display->SetChatMessage("system", msg_buffer);
        });

        last_update_time = current_time;
        recent_bytes = 0;
      }
    }
  }

  free(buffer);

  // Validate assets was received properly.
  if (binary_written < 1000) {
    ESP_LOGE(kTag, "Invalid assets: written=%d", binary_written);

    app.Schedule([&app]() {
      app.Alert(Lang::Strings::ERROR, "Invalid assets file", "circle_xmark",
                Lang::Sounds::OGG_EXCLAMATION);
    });

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req,
                       "{\"success\": false, \"error\": \"invalid_assets\"}");
    return ESP_FAIL;
  }

  ESP_LOGI(kTag, "=== FINALIZING ASSETS ===");

  // Display finalizing message.
  app.Schedule(
      [display]() { display->SetChatMessage("system", "Finalizing..."); });

  vTaskDelay(pdMS_TO_TICKS(500));

  board.SetPowerSaveMode(true);

  ESP_LOGI(kTag, "✅ ASSETS UPDATE SUCCESSFUL!");
  ESP_LOGI(kTag, "Total written: %d bytes, Sectors erased: %zu", binary_written,
           current_sector);

  // Display success message.
  app.Schedule([display]() {
    display->SetChatMessage("system", "Assets updated!\nApplying...");
  });

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"success\": true}");

  // Re-initialize and apply assets.
  vTaskDelay(pdMS_TO_TICKS(1000));
  esp_restart();

  return ESP_OK;
}

}  // namespace ota