#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <hal/aosl_hal_memory.h>
#include <hal/aosl_hal_file.h>
#include <api/aosl_log.h>
#include "fs/fatfs/ff.h"
#include "kernel/os/os.h"

#define FS_PATH_MAX_SIZE (256 + 4)

int aosl_hal_mkdir(const char *path)
{
	int ret = -1;
	FRESULT res;
	res = f_mkdir(path);
	if (res != FR_OK && res != FR_EXIST)
	{
		ret = -res;
	}
	else
	{
		ret = 0;
	}
	return ret;
}

int aosl_hal_rmdir(const char *path)
{
	int ret = -1;
	FRESULT res;
	DIR *dp = NULL;
	FILINFO *entry = NULL;
	char *temp_path = NULL;

	temp_path = (char*)aosl_hal_malloc(FS_PATH_MAX_SIZE);

	dp = (DIR*)aosl_hal_malloc(sizeof(DIR));
	memset(dp, 0, sizeof(DIR));

	entry = (FILINFO*)aosl_hal_malloc(sizeof(FILINFO));
	memset(entry, 0, sizeof(FILINFO));

	res = f_opendir(dp, path);
	if (res != FR_OK) {
		AOSL_LOG_ERR("open dir %s failed, return %d\n", path, res);
		goto out;
	}

	while (1) {
		res = f_readdir(dp, entry);
		if (res != FR_OK) {
			AOSL_LOG_ERR("read dir %s failed, return %d\n", path, res);
			break;
		}

		if (entry->fname[0] == 0) {
			AOSL_LOG_ERR("delete dir %s files finish\n", path);
			ret = 0;
			break;
		}

		snprintf(temp_path, FS_PATH_MAX_SIZE, "%s/%s", path, entry->fname);
		if (entry->fattrib & AM_DIR) {
			if (aosl_hal_rmdir(temp_path) < 0) {
				break;
			}
		} else {
			res = f_unlink(temp_path);
			AOSL_LOG_ERR("delete file %s %s\n", temp_path,
					(res != FR_OK) ? "failed" : "success");
			if (res != FR_OK) {
				break;
			}
		}
	}

	f_closedir(dp);
	if (ret == 0 ) {
		res = f_unlink(path);
		if (res != FR_OK) {
			AOSL_LOG_ERR("delete dir %s failed, return %d\n", path, res);
			ret = -res;
		} else {
			AOSL_LOG_ERR("delete dir %s success\n", path);
		}
	}

out:
	if (entry)
		aosl_hal_free(entry);

	if (dp)
		aosl_hal_free(dp);

	if (temp_path)
		aosl_hal_free(temp_path);

	return ret;
}

int aosl_hal_fexist(const char *path)
{
	FRESULT res = f_stat(path, NULL);
	if (res == FR_OK) {
		return 0;
	} else {
		return -res;		
	}
}

int aosl_hal_fsize(const char *path)
{
	FILINFO *finfo = aosl_hal_malloc(sizeof(FILINFO));
	FRESULT res = f_stat(path, finfo);
	if(res != FR_OK) {
		aosl_hal_free(finfo);
		return -res;
	}else{
		int size = finfo->fsize;
		aosl_hal_free(finfo);
		return size;
	}
}

int aosl_hal_file_create(const char *filepath)
{
	FIL *fp = aosl_hal_malloc(sizeof(FIL));
	memset(fp, 0, sizeof(FIL));

	FRESULT res = f_open(fp, filepath, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK) {
		AOSL_LOG_ERR("create file %s failed, return %d\n", filepath, res);
		aosl_hal_free(fp);
		return -res;
	}else {
		f_close(fp);
		aosl_hal_free(fp);	
		return 0;
	}
}

int aosl_hal_file_delete(const char *filepath)
{
	FRESULT res = f_unlink(filepath);
	if (res != FR_OK) {
		AOSL_LOG_ERR("delete file %s failed, return %d\n", filepath, res);
		return -res;
	} else {
		AOSL_LOG_ERR("delete file %s success\n", filepath);
		return 0;
	}
	
}

int aosl_hal_file_rename(const char *old_name, const char *new_name)
{
	FRESULT res = f_rename(old_name, new_name);
	if (res != FR_OK) {
		AOSL_LOG_ERR("rename file from %s to %s failed, return %d\n", old_name, new_name, res);
		return -res;	
	} else {
		AOSL_LOG_ERR("rename file from %s to %s success\n", old_name, new_name);
		return 0;
	}	
	
}

aosl_fs_t aosl_hal_fopen(const char *filepath, const char *mode)
{
	unsigned char fmode = 0;
    FIL * file = (FIL*)aosl_hal_malloc(sizeof(FIL));
    if (file == NULL)
        goto out;

    if (strstr(mode, "a"))
        fmode |= FA_OPEN_APPEND;
    if (strstr(mode, "+"))
        fmode |= FA_READ | FA_WRITE;
    if (strstr(mode, "w"))	
        fmode |= FA_WRITE | FA_CREATE_ALWAYS;
    if (strstr(mode, "r"))
        fmode |= FA_READ | FA_OPEN_EXISTING;

    FRESULT res = f_open(file, filepath, fmode);
    if (res != FR_OK) {
        AOSL_LOG_ERR("open file\"%s\" failed: %d\n", filepath, res);
        aosl_hal_free(file);
        file = NULL;
    }

out:
    return file;
}

int aosl_hal_fclose(aosl_fs_t fs)
{
	FRESULT res;

    if (fs == NULL)
        return -1;

    res = f_close(fs);
    if (res == FR_OK){
        aosl_hal_free(fs);
		return 0;
	}else{
		AOSL_LOG_ERR("close file failed: %d\n", res);
		return -1;
	}
}

int aosl_hal_fread(aosl_fs_t fs, void *buf, size_t size)
{

	unsigned int ret;
    FRESULT res;

    if (fs == NULL)
        return -1;

    res = f_read(fs, buf, size, &ret);
    if (res != FR_OK) {
        AOSL_LOG_ERR("read file failed: %d\n", res);
        return (int)-res;
    }

    return (int)ret;
}

int aosl_hal_fwrite(aosl_fs_t fs, const void *buf, size_t size)
{
	unsigned int ret;
    FRESULT res;

    if (fs == NULL)
        return -1;

    res = f_write(fs, buf, size, &ret);
    if (res != FR_OK) {
        AOSL_LOG_ERR("write file failed: %d\n", res);
        return (int)-res;
    }

    return (int)ret;
}
