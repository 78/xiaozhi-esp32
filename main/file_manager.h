/*
 * @Author: your name
 * @Date: 2021-10-14 19:39:11
 * @LastEditTime: 2021-10-16 14:16:39
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \esp-idf\pro\s3_i2s\main\file_manager.h
 */
// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _IOT_FILE_MANAGER_H_
#define _IOT_FILE_MANAGER_H_

#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOUNT_POINT  "/sdcard"

esp_err_t fm_sdcard_init(void);
esp_err_t fm_spiffs_init(void);
void fm_print_dir(char *direntName, int level);
const char *fm_get_basepath(void);
const char *fm_get_filename(const char *file);
size_t fm_get_file_size(const char *filepath);
esp_err_t fm_file_table_create(char ***list_out, uint16_t *files_number, const char *filter_suffix);
esp_err_t fm_file_table_free(char ***list,uint16_t files_number);
#ifdef __cplusplus
}
#endif

#endif

