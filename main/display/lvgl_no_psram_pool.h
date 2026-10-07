#ifndef LVGL_NO_PSRAM_POOL_H
#define LVGL_NO_PSRAM_POOL_H

#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_MEM_SIZE
#undef LV_MEM_SIZE
#endif
#define LV_MEM_SIZE (32 * 1024U)

static inline void* lvgl_no_psram_pool_alloc(size_t size) {
    return malloc(size);
}

#define LV_MEM_POOL_ALLOC(size) lvgl_no_psram_pool_alloc(size)

#ifdef __cplusplus
}
#endif

#endif  // LVGL_NO_PSRAM_POOL_H
