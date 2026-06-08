#include <string.h>
#include <stdlib.h>
#include "driver/chip/psram/psram.h"
#include <hal/aosl_hal_memory.h>

void *aosl_hal_malloc(size_t size)
{
  return psram_malloc(size);
  // return malloc(size);
}

void aosl_hal_free(void *ptr)
{
  psram_free(ptr);
	// free(ptr);
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
  return psram_calloc(nmemb, size);
  // return calloc(nmemb, size);
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
  return psram_realloc(ptr, size);
  // return realloc(ptr, size);
}
