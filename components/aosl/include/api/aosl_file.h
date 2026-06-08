/***************************************************************************
 * Module:	AOSL regular file operations definition header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_FILE_H__
#define __AOSL_FILE_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_file.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a directory at the specified path.
 * @param [in] path  the directory path to create
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mkdir(const char *path);

/**
 * @brief Remove a directory at the specified path.
 * @param [in] path  the directory path to remove
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_rmdir(const char *path);

/**
 * @brief Check whether a file or directory exists at the specified path.
 * @param [in] path  the file path to check
 * @return           non-zero if exists, 0 if not
 **/
extern __aosl_api__ int aosl_fexist(const char *path);

/**
 * @brief Get the size of a file in bytes.
 * @param [in] path  the file path
 * @return           file size in bytes on success, <0 on failure
 **/
extern __aosl_api__ int aosl_fsize(const char *path);

/**
 * @brief Create a new empty file at the specified path.
 * @param [in] filepath  the file path to create
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_file_create(const char *filepath);

/**
 * @brief Delete a file at the specified path.
 * @param [in] filepath  the file path to delete
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_file_delete(const char *filepath);

/**
 * @brief Rename or move a file.
 * @param [in] old_name  the current file path
 * @param [in] new_name  the new file path
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_file_rename(const char *old_name, const char *new_name);

/**
 * @brief Open a file with the specified mode.
 * @param [in] filepath  the file path to open
 * @param [in] mode      the open mode string (e.g. "r", "w", "rb", "wb")
 * @return               file handle on success, invalid handle on failure
 **/
extern __aosl_api__ aosl_fs_t aosl_fopen(const char *filepath, const char *mode);

/**
 * @brief Close an opened file handle.
 * @param [in] fs  the file handle to close
 * @return         0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_fclose(aosl_fs_t fs);

/**
 * @brief Read data from an opened file.
 * @param [in]  fs    the file handle
 * @param [out] buf   the buffer to read data into
 * @param [in]  size  the number of bytes to read
 * @return            the number of bytes actually read, <0 on failure
 **/
extern __aosl_api__ int aosl_fread(aosl_fs_t fs, void *buf, size_t size);

/**
 * @brief Write data to an opened file.
 * @param [in] fs    the file handle
 * @param [in] buf   the buffer containing data to write
 * @param [in] size  the number of bytes to write
 * @return           the number of bytes actually written, <0 on failure
 **/
extern __aosl_api__ int aosl_fwrite(aosl_fs_t fs, const void *buf, size_t size);

#ifdef __cplusplus
}
#endif


#endif /* __AOSL_FILE_H__ */