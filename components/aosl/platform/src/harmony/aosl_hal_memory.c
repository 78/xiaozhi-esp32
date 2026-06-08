#include <stdlib.h>
#include <hal/aosl_hal_memory.h>


void *aosl_hal_malloc(size_t size)
{
	return malloc(size);
}

void aosl_hal_free(void *ptr)
{
	free(ptr);
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}