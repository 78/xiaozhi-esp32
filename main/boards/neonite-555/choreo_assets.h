#ifndef CHOREO_ASSETS_H
#define CHOREO_ASSETS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 从 mmap assets 读取文件数据到 malloc 缓冲区（调用者 free）。
 * name 可以是带路径前缀的文件名（如 "/assets/dance_01.json"，自动去前缀）
 * 或纯文件名（如 "dance_01.json"）。
 * 返回 true 成功。 */
bool choreo_assets_read(const char* name, uint8_t** out_data, size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif /* CHOREO_ASSETS_H */
