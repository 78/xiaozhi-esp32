#include <hal/aosl_hal_file.h>

int aosl_hal_mkdir(const char *path) { return 0; }

int aosl_hal_rmdir(const char *path) { return 0; }

int aosl_hal_fexist(const char *path) { return 0; }

int aosl_hal_fsize(const char *path) { return 0; }

int aosl_hal_file_create(const char *filepath) { return 0; }

int aosl_hal_file_delete(const char *filepath) { return 0; }

int aosl_hal_file_rename(const char *old_name, const char *new_name) {
  return 0;
}

aosl_fs_t aosl_hal_fopen(const char *filepath, const char *mode) { return 0; }

int aosl_hal_fclose(aosl_fs_t fs) { return 0; }

int aosl_hal_fread(aosl_fs_t fs, void *buf, size_t size) { return 0; }

int aosl_hal_fwrite(aosl_fs_t fs, const void *buf, size_t size) { return 0; }
