#include <string.h>
#include <ipro_osal.h>

#include <hal/aosl_hal_memory.h>

void *aosl_hal_malloc(size_t size)
{
  return ipro_osal_malloc(size);
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
  return ipro_osal_calloc(nmemb, size);
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
  return ipro_osal_realloc(ptr, size);
}

void aosl_hal_free(void *ptr)
{
  if (ptr) {
    ipro_osal_free(ptr);
  }
}
