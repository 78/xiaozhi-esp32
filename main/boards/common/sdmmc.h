#ifndef XIAOZHI_SDMMC_H_
#define XIAOZHI_SDMMC_H_

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "sd_card.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

// Default GPIO pins for SD card interface
// CLK=40, CMD=39, D0=41, D1=42, D2=45, D3=38
#ifndef DEFAULT_SDMMC_CLK_GPIO
#define DEFAULT_SDMMC_CLK_GPIO GPIO_NUM_40
#endif
#ifndef DEFAULT_SDMMC_CMD_GPIO
#define DEFAULT_SDMMC_CMD_GPIO GPIO_NUM_39
#endif
#ifndef DEFAULT_SDMMC_D0_GPIO
#define DEFAULT_SDMMC_D0_GPIO GPIO_NUM_41
#endif
#ifndef DEFAULT_SDMMC_D1_GPIO
#define DEFAULT_SDMMC_D1_GPIO GPIO_NUM_42
#endif
#ifndef DEFAULT_SDMMC_D2_GPIO
#define DEFAULT_SDMMC_D2_GPIO GPIO_NUM_45
#endif
#ifndef DEFAULT_SDMMC_D3_GPIO
#define DEFAULT_SDMMC_D3_GPIO GPIO_NUM_38
#endif

constexpr const char* kSdCardMountPoint = "/sdcard";
constexpr int kSdCardMaxFiles = 5;
constexpr size_t kSdCardAllocationUnitSize = 16 * 1024;

class SdMMC : public SdCard {
 public:
  struct Config {
    const char* mount_point = kSdCardMountPoint;
    bool format_if_mount_failed = false;
    int max_files = kSdCardMaxFiles;
    size_t allocation_unit_size = kSdCardAllocationUnitSize;
    int bus_width = 4;  // 1 or 4
    gpio_num_t clk_pin = DEFAULT_SDMMC_CLK_GPIO;
    gpio_num_t cmd_pin = DEFAULT_SDMMC_CMD_GPIO;
    gpio_num_t d0_pin = DEFAULT_SDMMC_D0_GPIO;
    gpio_num_t d1_pin = DEFAULT_SDMMC_D1_GPIO;
    gpio_num_t d2_pin = DEFAULT_SDMMC_D2_GPIO;
    gpio_num_t d3_pin = DEFAULT_SDMMC_D3_GPIO;
    int max_freq_khz = SDMMC_FREQ_DEFAULT;  // 20MHz default
  };

  SdMMC();
  explicit SdMMC(const Config& config);
  explicit SdMMC(gpio_num_t clk_pin,
                  gpio_num_t cmd_pin,
                  gpio_num_t d0_pin,
                  gpio_num_t d1_pin,
                  gpio_num_t d2_pin,
                  gpio_num_t d3_pin,
                  int bus_width = 4,
                  const char* mount_point = kSdCardMountPoint,
                  bool format_if_mount_failed = false,
                  int max_files = kSdCardMaxFiles,
                  size_t allocation_unit_size = kSdCardAllocationUnitSize,
                  int max_freq_khz = SDMMC_FREQ_HIGHSPEED);
  explicit SdMMC(gpio_num_t clk_pin,
                  gpio_num_t cmd_pin,
                  gpio_num_t d0_pin,
                  int bus_width = 1,
                  const char* mount_point = kSdCardMountPoint,
                  bool format_if_mount_failed = false,
                  int max_files = kSdCardMaxFiles,
                  size_t allocation_unit_size = kSdCardAllocationUnitSize,
                  int max_freq_khz = SDMMC_FREQ_HIGHSPEED);
  ~SdMMC();

  // Disable copy and assign
  SdMMC(const SdMMC&) = delete;
  SdMMC& operator=(const SdMMC&) = delete;

  // Initialize and mount the SD card
  esp_err_t Initialize() override;

  // Unmount and deinitialize the SD card
  esp_err_t Deinitialize() override;

  // Get mount point path
  const char* GetMountPoint() const override { return config_.mount_point; }

  // Get card information
  const sdmmc_card_t* GetCardInfo() const { return card_; }

  // Print card information to stdout
  void PrintCardInfo() const override;

  // File operations
  esp_err_t WriteFile(const char* path, const char* data) override;
  esp_err_t ReadFile(const char* path, char* buffer, size_t buffer_size) override;
  esp_err_t DeleteFile(const char* path) override;
  esp_err_t RenameFile(const char* old_path, const char* new_path) override;
  bool FileExists(const char* path) override;
  // Format the SD card
  esp_err_t Format() override;

 private:
  Config config_;
  sdmmc_card_t* card_;

  static constexpr const char* kTag = "SdMMC";
};

#endif  // XIAOZHI_SDMMC_H_
