#ifndef MAIN_OTA_SERVER_H_
#define MAIN_OTA_SERVER_H_

#include "esp_err.h"
#include "esp_http_server.h"

namespace ota {

// OTA server class for handling firmware updates via HTTP.
// Provides a web interface for uploading and flashing firmware updates.
class OtaServer {
 public:
  // Returns the singleton instance of OtaServer.
  static OtaServer& GetInstance();

  // Deleted copy constructor and assignment operator.
  OtaServer(const OtaServer&) = delete;
  OtaServer& operator=(const OtaServer&) = delete;

  // Starts the OTA web server.
  // Returns ESP_OK on success, ESP_FAIL otherwise.
  esp_err_t Start(int port = 80);

  // Stops the OTA web server.
  void Stop();

  // Returns true if the server is currently running.
  bool IsRunning() const { return server_handle_ != nullptr; }

 private:
  OtaServer() = default;
  ~OtaServer();

  // HTTP handler for GET /ota - serves the OTA upload page.
  static esp_err_t HandleOtaGet(httpd_req_t* req);

  // HTTP handler for POST /ota_upload - processes firmware upload.
  static esp_err_t HandleOtaUpload(httpd_req_t* req);

  // HTTP handler for GET /assets - serves the assets upload page.
  static esp_err_t HandleAssetsGet(httpd_req_t* req);

  // HTTP handler for POST /assets_upload - processes assets upload.
  static esp_err_t HandleAssetsUpload(httpd_req_t* req);

  httpd_handle_t server_handle_ = nullptr;
};

}  // namespace ota

#endif  // MAIN_OTA_SERVER_H_