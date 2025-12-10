#ifndef XIAOZHI_SDSPI_H_
#define XIAOZHI_SDSPI_H_

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "sd_card.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

// Default GPIO pins for SPI SD card interface
#ifndef DEFAULT_SDSPI_MISO_GPIO
#define DEFAULT_SDSPI_MISO_GPIO GPIO_NUM_2
#endif
#ifndef DEFAULT_SDSPI_MOSI_GPIO
#define DEFAULT_SDSPI_MOSI_GPIO GPIO_NUM_15
#endif
#ifndef DEFAULT_SDSPI_CLK_GPIO
#define DEFAULT_SDSPI_CLK_GPIO GPIO_NUM_14
#endif
#ifndef DEFAULT_SDSPI_CS_GPIO
#define DEFAULT_SDSPI_CS_GPIO GPIO_NUM_13
#endif

constexpr const char* kSdSpiMountPoint = "/sdcard";
constexpr int kSdSpiMaxFiles = 5;
constexpr size_t kSdSpiAllocationUnitSize = 16 * 1024;

class SdSPI : public SdCard {
 public:
  struct Config {
    const char* mount_point = kSdSpiMountPoint;
    bool format_if_mount_failed = false;
    int max_files = kSdSpiMaxFiles;
    size_t allocation_unit_size = kSdSpiAllocationUnitSize;
    gpio_num_t miso_pin = DEFAULT_SDSPI_MISO_GPIO;
    gpio_num_t mosi_pin = DEFAULT_SDSPI_MOSI_GPIO;
    gpio_num_t clk_pin = DEFAULT_SDSPI_CLK_GPIO;
    gpio_num_t cs_pin = DEFAULT_SDSPI_CS_GPIO;
    int max_freq_khz = SDMMC_FREQ_DEFAULT;  // 20MHz default (can be lower for SPI)
    spi_host_device_t host_id = SPI2_HOST;  // SPI2_HOST or SPI3_HOST
  };

  SdSPI();
  explicit SdSPI(const Config& config);
  explicit SdSPI(gpio_num_t miso_pin,
                  gpio_num_t mosi_pin,
                  gpio_num_t clk_pin,
                  gpio_num_t cs_pin,
                  spi_host_device_t host_id = SPI2_HOST,
                  const char* mount_point = kSdSpiMountPoint,
                  bool format_if_mount_failed = false,
                  int max_files = kSdSpiMaxFiles,
                  size_t allocation_unit_size = kSdSpiAllocationUnitSize,
                  int max_freq_khz = SDMMC_FREQ_DEFAULT);
  ~SdSPI();

  // Disable copy and assign
  SdSPI(const SdSPI&) = delete;
  SdSPI& operator=(const SdSPI&) = delete;

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
  bool spi_bus_initialized_;

  static constexpr const char* kTag = "SdSPI";
};

#endif  // XIAOZHI_SDSPI_H_
