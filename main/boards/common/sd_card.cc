#include <esp_log.h>
#include <esp_err.h>
#include "sd_card.h"

#define TAG "SdCard"

SdCard::SdCard() {
    is_mounted_ = false;
}

SdCard::~SdCard() {}

// Initialize and mount the SD card
esp_err_t SdCard::Initialize() {
  return ESP_FAIL;
}

// Unmount and deinitialize the SD card
esp_err_t SdCard::Deinitialize() {
  return ESP_FAIL;
}

// Check if SD card is mounted
bool SdCard::IsMounted() const { 
    return is_mounted_; 
}

const char* SdCard::GetMountPoint() const {
    return nullptr;
}

// Print card information to stdout
void SdCard::PrintCardInfo() const {
  ESP_LOGW(TAG, "PrintCardInfo: SD card info not available.");
}

// File operations
esp_err_t SdCard::WriteFile(const char* path, const char* data) {
  return ESP_FAIL;
}

esp_err_t SdCard::ReadFile(const char* path, char* buffer, size_t buffer_size) {
  return ESP_FAIL;
}

esp_err_t SdCard::DeleteFile(const char* path) {
  return ESP_FAIL;
}

esp_err_t SdCard::RenameFile(const char* old_path, const char* new_path) {
  return ESP_FAIL;
}

bool SdCard::FileExists(const char* path) {
  return false;
}

// Format the SD card
esp_err_t SdCard::Format() {
  return ESP_FAIL;
}