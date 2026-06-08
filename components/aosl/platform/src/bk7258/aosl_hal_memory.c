#include <os/mem.h>
#include <string.h>

#include <hal/aosl_hal_memory.h>
#include <hal/aosl_hal_atomic.h>

//#define AOSL_MM_DEBUG

#ifdef AOSL_MM_DEBUG

#include <stdio.h>
#include <assert.h>

typedef struct {
  void *original_alloc_ptr;
  size_t original_alloc_size;
} mm_trace_header_t;

static intptr_t __mm_trace_curr_usage = 0;
static intptr_t __mm_trace_curr_slice = 0;

static inline size_t __mm_trace_get_mask_size(void)
{
  return (((sizeof(mm_trace_header_t) + 15) >> 4) << 4);
}

static void* __mm_trace_mask(void *ptr, size_t size)
{
  mm_trace_header_t *header = (mm_trace_header_t *)ptr;
  header->original_alloc_ptr = ptr;
  header->original_alloc_size = size;
  aosl_hal_atomic_add(size, &__mm_trace_curr_usage);
  aosl_hal_atomic_inc(&__mm_trace_curr_slice);
  return (char *)ptr + __mm_trace_get_mask_size();
}

static void* __mm_trace_unmask(void *ptr)
{
  char *p;
  mm_trace_header_t *header;
  p = (char *)ptr - __mm_trace_get_mask_size();
  header = (mm_trace_header_t *)p;
  assert(header->original_alloc_ptr == header);
  aosl_hal_atomic_sub(header->original_alloc_size, &__mm_trace_curr_usage);
  memset(header, 0, sizeof(mm_trace_header_t));
  aosl_hal_atomic_dec(&__mm_trace_curr_slice);
  return p;
}

void *aosl_hal_malloc(size_t size)
{
  void *ptr = psram_malloc(size + __mm_trace_get_mask_size());
  if (ptr != NULL) {
    ptr = __mm_trace_mask(ptr, size);
  }

  return ptr;
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
  size_t s;
  void *ptr;

  s = nmemb * size;
  ptr = aosl_hal_malloc(s);
  if (ptr != NULL) {
    memset(ptr, 0, s);
  }

  return ptr;
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
  if (NULL == ptr && 0 == size) return NULL;
  if (NULL == ptr) return aosl_hal_malloc(size);
  if (0 == size) {
    aosl_hal_free(ptr);
    return NULL;
  }

  ptr = __mm_trace_unmask(ptr);
  ptr = bk_psram_realloc(ptr, size + __mm_trace_get_mask_size());
  if (NULL == ptr) return NULL;
  return __mm_trace_mask(ptr, size);
}

void aosl_hal_free(void *ptr)
{
  if (ptr) {
    psram_free(__mm_trace_unmask(ptr));
  }
}

void aosl_curr_memory_usage_info()
{
  printf("%s%d: [MEM] curr total alloc memory slice=%d size=%d\n", __FUNCTION__, __LINE__,
         (int)__mm_trace_curr_slice, (int)__mm_trace_curr_usage);
}

#else

void *aosl_hal_malloc(size_t size)
{
  return psram_malloc(size);
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
  return psram_zalloc(nmemb * size);
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
  return bk_psram_realloc(ptr, size);
}

void aosl_hal_free(void *ptr)
{
  if (ptr) {
    psram_free(ptr);
  }
}

#endif