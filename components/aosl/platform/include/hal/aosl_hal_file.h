/***************************************************************************
 * Module:	file hal definitions.
 *
 * Copyright (c) 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_FILE_H__
#define __AOSL_HAL_FILE_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief file stream type
 */
typedef void* aosl_fs_t;

/**
 * @brief create file directory
 * @param [in] path file path
 * @return 0 on success, < 0 on error
 */
int aosl_hal_mkdir(const char *path);
/**
 * @brief remove file directory
 * @param [in] path file path
 * @return 0 on success, < 0 on error
 */
int aosl_hal_rmdir(const char *path);
/**
 * @brief check file exist
 * @param [in] path file path
 * @return 0: not exist, non-zero: exist
 */
int aosl_hal_fexist(const char *path);

/**
 * @brief get file size
 * @param [in] path file path
 * @return >=0: file size, <0: failed
 */
int aosl_hal_fsize(const char *path);

/**
 * @brief create file
 * @param [in] filepath file path
 * @return 0 on success, < 0 on error
 */
int aosl_hal_file_create(const char *filepath);

/**
 * @brief delete file
 * @param [in] filepath file path
 * @return 0 on success, < 0 on error
 */
int aosl_hal_file_delete(const char *filepath);

/**
 * @brief rename file
 * @param [in] old_name old file name
 * @param [in] new_name new file name
 * @return 0: success, < 0: failed
 */
int aosl_hal_file_rename(const char *old_name, const char *new_name);

/**
 * @brief open file
 * @param [in] filepath file path
 * @param [in] mode open mode: "r", "rb", "r+", "rb+", "w", "wb", "w+", "wb+", "a", "ab", "a+", "ab+"
 * @return file stream handle, return NULL on failure
 */
aosl_fs_t aosl_hal_fopen(const char *filepath, const char *mode);

/**
 * @brief close file
 * @param fs file stream handle
 * @return 0: success, < 0: failed
 */
int aosl_hal_fclose(aosl_fs_t fs);

/**
 * @brief read file
 * @param [in] fs file stream handle
 * @param [in] buf data buffer
 * @param [in] size read data size
 * @return actual read data size, failed return -1
 */
int aosl_hal_fread (aosl_fs_t fs, void *buf, size_t size);

/**
 * @brief write file
 * @param [in] fs file stream handle
 * @param [in] buf data buffer
 * @param [in] size write data size
 * @return actual write data size, failed return -1
 */
int aosl_hal_fwrite (aosl_fs_t fs, const void *buf, size_t size);


#ifdef __cplusplus
}
#endif

#endif
