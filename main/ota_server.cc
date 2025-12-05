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
extern const uint8_t assets_index_html_start[] asm("_binary_assets_index_html_start");
extern const uint8_t assets_index_html_end[] asm("_binary_assets_index_html_end");

const char* kOtaIndexHtml = reinterpret_cast<const char*>(ota_index_html_start);
const char* kAssetIndexHtml = reinterpret_cast<const char*>(assets_index_html_start);
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
/*
Safari response example:
Content-Disposition: form-data; name="file"; filename="xiaozhi_app.bin"
Content-Type: application/macbinary

Chrome response example:
Content-Disposition: form-data; name="file"; filename="xiaozhi.bin"
Content-Type: application/octet-stream
*/
  const char* kFileMarker = "Content-Disposition: form-data";
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
      ESP_LOGI(kTag, "Header buffer content size %d: %.512s", (int)header_buffer.size(), header_buffer.c_str());
      size_t file_pos = header_buffer.find(kFileMarker);
      if (file_pos != std::string::npos) {
        ESP_LOGI(kTag, "✅ Found firmware file marker at position %zu",
                 file_pos);
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
        vTaskDelay(pdMS_TO_TICKS(10));  // Yield to avoid watchdog timer reset.
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
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, kAssetIndexHtml,
                  assets_index_html_end - assets_index_html_start);
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
  const char* kFileMarker = "Content-Disposition: form-data";
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
        vTaskDelay(pdMS_TO_TICKS(10));  // Yield to avoid watchdog timer reset.
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