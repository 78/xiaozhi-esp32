#ifndef LVGL_PSRAM_POOL_H
#define LVGL_PSRAM_POOL_H

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void* lvgl_psram_pool_alloc(size_t size) {
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == NULL) {
        ESP_LOGE("LvglPsramPool", "Failed to allocate %u bytes in PSRAM", (unsigned)size);
    }
    return p;
}

#define LV_MEM_POOL_ALLOC(size) lvgl_psram_pool_alloc(size)

#ifdef __cplusplus
}
#endif

#endif  // LVGL_PSRAM_POOL_H
