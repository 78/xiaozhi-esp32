#include "sdspi.h"
#include "esp_log.h"

SdSPI::SdSPI() : card_(nullptr), spi_bus_initialized_(false) {
  config_ = Config();
}

SdSPI::SdSPI(const Config& config)
    : config_(config), card_(nullptr), spi_bus_initialized_(false) {}

SdSPI::SdSPI(gpio_num_t miso_pin,
             gpio_num_t mosi_pin,
             gpio_num_t clk_pin,
             gpio_num_t cs_pin,
             spi_host_device_t host_id,
             const char* mount_point,
             bool format_if_mount_failed,
             int max_files,
             size_t allocation_unit_size,
             int max_freq_khz)
    : config_{mount_point, format_if_mount_failed, max_files, 
              allocation_unit_size, miso_pin, mosi_pin, clk_pin, 
              cs_pin, max_freq_khz, host_id},
      card_(nullptr),
      spi_bus_initialized_(false) {}

SdSPI::~SdSPI() {
  if (is_mounted_) {
    Deinitialize();
  }
}

esp_err_t SdSPI::Initialize() {
  if (is_mounted_) {
    ESP_LOGW(kTag, "SD card already mounted");
    return ESP_OK;
  }

  ESP_LOGI(kTag, "Initializing SD card using SPI");

  // Mount configuration
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = config_.format_if_mount_failed,
      .max_files = config_.max_files,
      .allocation_unit_size = config_.allocation_unit_size,
      .disk_status_check_enable = false};

  // Initialize SPI bus
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = config_.mosi_pin,
      .miso_io_num = config_.miso_pin,
      .sclk_io_num = config_.clk_pin,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };

  esp_err_t ret = spi_bus_initialize(config_.host_id, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
    return ret;
  }
  spi_bus_initialized_ = true;
  ESP_LOGI(kTag, "SPI bus initialized");

  // Host configuration for SDSPI
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.max_freq_khz = config_.max_freq_khz;

  // Slot configuration for SDSPI
  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = config_.cs_pin;
  slot_config.host_id = config_.host_id;

  // Mount the filesystem
  ESP_LOGI(kTag, "Mounting filesystem at %s", config_.mount_point);
  ret = esp_vfs_fat_sdspi_mount(config_.mount_point, &host, &slot_config, 
                                &mount_config, &card_);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(kTag,
               "Failed to mount filesystem. "
               "If you want the card to be formatted, set format_if_mount_failed option.");
    } else {
      ESP_LOGE(kTag,
               "Failed to initialize the card (%s). "
               "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    
    // Clean up SPI bus on failure
    if (spi_bus_initialized_) {
      spi_bus_free(config_.host_id);
      spi_bus_initialized_ = false;
    }
    
    card_ = nullptr;
    return ret;
  }

  is_mounted_ = true;
  ESP_LOGI(kTag, "Filesystem mounted successfully");
  PrintCardInfo();

  return ESP_OK;
}

esp_err_t SdSPI::Deinitialize() {
  if (!is_mounted_) {
    ESP_LOGW(kTag, "SD card not mounted");
    return ESP_OK;
  }

  ESP_LOGI(kTag, "Unmounting SD card");
  
  // Unmount the card
  esp_err_t ret = esp_vfs_fat_sdcard_unmount(config_.mount_point, card_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to unmount SD card: %s", esp_err_to_name(ret));
    return ret;
  }

  card_ = nullptr;
  is_mounted_ = false;
  ESP_LOGI(kTag, "Card unmounted");

  // Deinitialize the SPI bus after all devices are removed
  if (spi_bus_initialized_) {
    spi_bus_free(config_.host_id);
    spi_bus_initialized_ = false;
    ESP_LOGI(kTag, "SPI bus freed");
  }

  return ESP_OK;
}

void SdSPI::PrintCardInfo() const {
  if (card_ != nullptr) {
    sdmmc_card_print_info(stdout, card_);
  } else {
    ESP_LOGW(kTag, "No card information available");
  }
}

esp_err_t SdSPI::WriteFile(const char* path, const char* data) {
  if (!is_mounted_) {
    ESP_LOGE(kTag, "SD card not mounted");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(kTag, "Writing file: %s", path);
  FILE* f = fopen(path, "w");
  if (f == nullptr) {
    ESP_LOGE(kTag, "Failed to open file for writing: %s", path);
    return ESP_FAIL;
  }

  fprintf(f, "%s", data);
  fclose(f);
  ESP_LOGI(kTag, "File written successfully");

  return ESP_OK;
}

esp_err_t SdSPI::ReadFile(const char* path, char* buffer, size_t buffer_size) {
  if (!is_mounted_) {
    ESP_LOGE(kTag, "SD card not mounted");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(kTag, "Reading file: %s", path);
  FILE* f = fopen(path, "r");
  if (f == nullptr) {
    ESP_LOGE(kTag, "Failed to open file for reading: %s", path);
    return ESP_FAIL;
  }

  if (fgets(buffer, buffer_size, f) == nullptr) {
    ESP_LOGE(kTag, "Failed to read file: %s", path);
    fclose(f);
    return ESP_FAIL;
  }

  fclose(f);

  // Strip newline
  char* pos = strchr(buffer, '\n');
  if (pos != nullptr) {
    *pos = '\0';
  }

  ESP_LOGI(kTag, "Read from file: '%s'", buffer);
  return ESP_OK;
}

esp_err_t SdSPI::DeleteFile(const char* path) {
  if (!is_mounted_) {
    ESP_LOGE(kTag, "SD card not mounted");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(kTag, "Deleting file: %s", path);
  if (unlink(path) != 0) {
    ESP_LOGE(kTag, "Failed to delete file: %s", path);
    return ESP_FAIL;
  }

  ESP_LOGI(kTag, "File deleted successfully");
  return ESP_OK;
}

esp_err_t SdSPI::RenameFile(const char* old_path, const char* new_path) {
  if (!is_mounted_) {
    ESP_LOGE(kTag, "SD card not mounted");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(kTag, "Renaming file from %s to %s", old_path, new_path);

  // Check if destination file exists
  struct stat st;
  if (stat(new_path, &st) == 0) {
    ESP_LOGI(kTag, "Destination file exists, deleting it first");
    unlink(new_path);
  }

  if (rename(old_path, new_path) != 0) {
    ESP_LOGE(kTag, "Failed to rename file");
    return ESP_FAIL;
  }

  ESP_LOGI(kTag, "File renamed successfully");
  return ESP_OK;
}

bool SdSPI::FileExists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

esp_err_t SdSPI::Format() {
  if (!is_mounted_) {
    ESP_LOGE(kTag, "SD card not mounted");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(kTag, "Formatting SD card");
  esp_err_t ret = esp_vfs_fat_sdcard_format(config_.mount_point, card_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to format SD card: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(kTag, "SD card formatted successfully");
  return ESP_OK;
}
