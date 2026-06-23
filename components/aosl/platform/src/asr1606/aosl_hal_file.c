#include <string.h>

#include <fs/ql_fs.h>
#include <hal/aosl_hal_file.h>

int aosl_hal_mkdir(const char *path)
{
  return ql_mkdir(path, 0);
}

int aosl_hal_rmdir(const char *path)
{
  return ql_remove(path);
}

int aosl_hal_fexist(const char *path)
{
  /* ql_access returns 0 if file exists */
  return (ql_access(path, 0) == 0) ? 1 : 0;
}

int aosl_hal_fsize(const char *path)
{
  QFILE *fp = ql_fopen(path, "r");
  if (NULL == fp) {
    return -1;
  }

  ql_fseek(fp, 0, 2 /* SEEK_END */);
  long size = ql_ftell(fp);
  ql_fclose(fp);

  return (int)size;
}

int aosl_hal_file_create(const char *filepath)
{
  QFILE *fp = ql_fopen(filepath, "a");
  if (NULL == fp) {
    return -1;
  }
  ql_fclose(fp);
  return 0;
}

int aosl_hal_file_delete(const char *filepath)
{
  return ql_remove(filepath);
}

int aosl_hal_file_rename(const char *old_name, const char *new_name)
{
  return ql_rename(old_name, new_name);
}

aosl_fs_t aosl_hal_fopen(const char *filepath, const char *mode)
{
  QFILE *fp = ql_fopen(filepath, mode);
  return (aosl_fs_t)fp;
}

int aosl_hal_fclose(aosl_fs_t fs)
{
  if (NULL == (void *)fs) {
    return -1;
  }
  return ql_fclose((QFILE *)fs);
}

int aosl_hal_fread(aosl_fs_t fs, void *buf, size_t size)
{
  if (NULL == (void *)fs) {
    return -1;
  }
  return ql_fread(buf, 1, size, (QFILE *)fs);
}

int aosl_hal_fwrite(aosl_fs_t fs, const void *buf, size_t size)
{
  if (NULL == (void *)fs) {
    return -1;
  }
  return ql_fwrite((void *)buf, 1, size, (QFILE *)fs);
}
