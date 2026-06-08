#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"

void *aosl_hal_malloc(size_t size)
{
	return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void aosl_hal_free(void *ptr)
{
	heap_caps_free(ptr);
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
	void *ptr = heap_caps_malloc(nmemb * size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (ptr) {
		memset(ptr, 0, nmemb * size);
	}
	return ptr;
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
	return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
