#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

// #define audio_malloc  malloc
// #define audio_free    free
// #define audio_strdup  strdup
// #define audio_calloc  calloc
// #define audio_realloc realloc

void *audio_calloc_inner(size_t n, size_t size)
{
    void *data =  NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_calloc_prefer(n, size, 2,  MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
#else
    data = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif

#ifdef ENABLE_AUDIO_MEM_TRACE
    ESP_LOGI("AUIDO_MEM", "calloc_inner:%p, size:%d, called:0x%08x", data, size, (intptr_t)__builtin_return_address(0) - 2);
#endif
    return data;
}

void *audio_calloc(size_t n, size_t size)
{
    return calloc(n, size);
}

// void* realloc(void* ptr, size_t size)
// {
//     return heap_caps_realloc_default(ptr, size);
// }

void audio_free(void *ptr)
{
    free(ptr);
}
