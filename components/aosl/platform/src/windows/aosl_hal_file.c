#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <sys/stat.h>

#include <hal/aosl_hal_file.h>

int aosl_hal_mkdir(const char *path)
{
    return _mkdir(path);
}

int aosl_hal_rmdir(const char *path)
{
    return _rmdir(path);
}

int aosl_hal_fexist(const char *path)
{
    return (_access(path, 0) == 0);
}

int aosl_hal_fsize(const char *path)
{
    struct _stat64 statbuf;
    if (_stat64(path, &statbuf) != 0) {
        return -1;
    }

    return (int)statbuf.st_size;
}

int aosl_hal_file_create(const char *filepath)
{
    FILE *fp = fopen(filepath, "a+b");
    if (!fp) {
        return -1;
    }

    fclose(fp);
    return 0;
}

int aosl_hal_file_delete(const char *filepath)
{
    return remove(filepath);
}

int aosl_hal_file_rename(const char *old_name, const char *new_name)
{
    return rename(old_name, new_name);
}

aosl_fs_t aosl_hal_fopen(const char *filepath, const char *mode)
{
    return (aosl_fs_t)fopen(filepath, mode);
}

int aosl_hal_fclose(aosl_fs_t fs)
{
    return fclose((FILE *)fs);
}

int aosl_hal_fread(aosl_fs_t fs, void *buf, size_t size)
{
    return (int)fread(buf, 1, size, (FILE *)fs);
}

int aosl_hal_fwrite(aosl_fs_t fs, const void *buf, size_t size)
{
    return (int)fwrite(buf, 1, size, (FILE *)fs);
}
