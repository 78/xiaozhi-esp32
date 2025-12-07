#include "sdmmc.h"
#include "esp_log.h"

SdMMC::SdMMC() : card_(nullptr){
  config_ = Config();
}

SdMMC::SdMMC(const Config& config)
    : config_(config), card_(nullptr){}

SdMMC::SdMMC(gpio_num_t clk_pin,
                 gpio_num_t cmd_pin,
                 gpio_num_t d0_pin,
                 gpio_num_t d1_pin,
                 gpio_num_t d2_pin,
                 gpio_num_t d3_pin,
                 int bus_width,
                 const char* mount_point,
                 bool format_if_mount_failed,
                 int max_files,
                 size_t allocation_unit_size,
                 int max_freq_khz)
    : config_{mount_point, format_if_mount_failed, max_files, allocation_unit_size, bus_width, clk_pin, cmd_pin, d0_pin, d1_pin, d2_pin, d3_pin, max_freq_khz},
      card_(nullptr) {}

SdMMC::SdMMC(gpio_num_t clk_pin,
                 gpio_num_t cmd_pin,
                 gpio_num_t d0_pin,
                 int bus_width,
                 const char* mount_point,
                 bool format_if_mount_failed,
                 int max_files,
                 size_t allocation_unit_size,
                 int max_freq_khz)
    : config_{mount_point, format_if_mount_failed, max_files, allocation_unit_size, bus_width, clk_pin, cmd_pin, d0_pin, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, max_freq_khz},
      card_(nullptr) {}

SdMMC::~SdMMC() {
  if (is_mounted_) {
    Deinitialize();
  }
}

esp_err_t SdMMC::Initialize() {
  if (is_mounted_) {
    ESP_LOGW(kTag, "SD card already mounted");
    return ESP_OK;
  }

  ESP_LOGI(kTag, "Initializing SD card");

  // Mount configuration
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = config_.format_if_mount_failed,
      .max_files = config_.max_files,
      .allocation_unit_size = config_.allocation_unit_size,
      .disk_status_check_enable = false};

  // Host configuration
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = config_.max_freq_khz;

  // Slot configuration
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = config_.bus_width;
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
  // Configure GPIO pins
  slot_config.clk = config_.clk_pin;
  slot_config.cmd = config_.cmd_pin;
  slot_config.d0 = config_.d0_pin;
  if (config_.bus_width == 4) {
    slot_config.d1 = config_.d1_pin;
    slot_config.d2 = config_.d2_pin;
    slot_config.d3 = config_.d3_pin;
  }
#endif

  // Mount the filesystem
  ESP_LOGI(kTag, "Mounting filesystem at %s", config_.mount_point);
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(config_.mount_point, &host,
                                          &slot_config, &mount_config, &card_);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(kTag,
               "Failed to mount filesystem. "
               "Consider setting format_if_mount_failed option.");
    } else {
      ESP_LOGE(kTag,
               "Failed to initialize the card (%s). "
               "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    card_ = nullptr;
    return ret;
  }

  is_mounted_ = true;
  ESP_LOGI(kTag, "Filesystem mounted successfully");
  PrintCardInfo();

  return ESP_OK;
}

esp_err_t SdMMC::Deinitialize() {
  if (!is_mounted_) {
    ESP_LOGW(kTag, "SD card not mounted");
    return ESP_OK;
  }

  ESP_LOGI(kTag, "Unmounting SD card");
  esp_err_t ret = esp_vfs_fat_sdcard_unmount(config_.mount_point, card_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to unmount SD card: %s", esp_err_to_name(ret));
    return ret;
  }

  card_ = nullptr;
  is_mounted_ = false;
  ESP_LOGI(kTag, "Card unmounted");

  return ESP_OK;
}

void SdMMC::PrintCardInfo() const {
  if (card_ != nullptr) {
    sdmmc_card_print_info(stdout, card_);
  } else {
    ESP_LOGW(kTag, "No card information available");
  }
}

esp_err_t SdMMC::WriteFile(const char* path, const char* data) {
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

esp_err_t SdMMC::ReadFile(const char* path, char* buffer,
                           size_t buffer_size) {
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

esp_err_t SdMMC::DeleteFile(const char* path) {
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

esp_err_t SdMMC::RenameFile(const char* old_path, const char* new_path) {
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

bool SdMMC::FileExists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

esp_err_t SdMMC::Format() {
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