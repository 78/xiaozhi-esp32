#ifndef XIAOZHI_SD_CARD_H_
#define XIAOZHI_SD_CARD_H_

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <esp_vfs_fat.h>
#include <esp_log.h>
#include <esp_err.h>


class SdCard {
 public:
  SdCard();
  virtual ~SdCard();

  // Initialize and mount the SD card
  virtual esp_err_t Initialize();

  // Unmount and deinitialize the SD card
  virtual esp_err_t Deinitialize();

  // Check if SD card is mounted
  virtual bool IsMounted() const;

  // Get mount point path
  virtual const char* GetMountPoint() const;

  // Print card information to stdout
  virtual void PrintCardInfo() const;

  // File operations
  virtual esp_err_t WriteFile(const char* path, const char* data);
  virtual esp_err_t ReadFile(const char* path, char* buffer, size_t buffer_size);
  virtual esp_err_t DeleteFile(const char* path);
  virtual esp_err_t RenameFile(const char* old_path, const char* new_path);
  virtual bool FileExists(const char* path);

  // Format the SD card
  virtual esp_err_t Format();

 protected:
  bool is_mounted_;
};

#endif  // XIAOZHI_SD_CARD_H_
