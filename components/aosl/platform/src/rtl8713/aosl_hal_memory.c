#include <string.h>

#include <hal/aosl_hal_memory.h>
#include <os_wrapper.h>

void *aosl_hal_malloc(size_t size)
{
  return rtos_mem_malloc(size);
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
  return rtos_mem_zmalloc(nmemb * size);
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
  return rtos_mem_realloc(ptr, size);
}

void aosl_hal_free(void *ptr)
{
  if (ptr) {
    rtos_mem_free(ptr);
  }
}
