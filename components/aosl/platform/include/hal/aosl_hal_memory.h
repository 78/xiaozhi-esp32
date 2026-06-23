/***************************************************************************
 * Module:	memory hal definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_MEMORY_H__
#define __AOSL_HAL_MEMORY_H__
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief allocate memory
 * @param [in] size size of memory to allocate
 * @return pointer to allocated memory, or NULL on error
 */
void *aosl_hal_malloc(size_t size);

/**
 * @brief   free memory
 * @param [in] ptr pointer to memory to free
 */
void aosl_hal_free(void *ptr);

/**
 * @brief allocate memory and initialize it to zero
 * @param [in] nmemb number of elements
 * @param [in] size size of each element
 * @return pointer to allocated memory, or NULL on error
 */
void *aosl_hal_calloc(size_t nmemb, size_t size);

/**
 * @brief reallocate memory
 * @param [in] ptr pointer to existing memory block
 * @param [in] size new size of memory block
 * @return pointer to reallocated memory, or NULL on error
 */
void *aosl_hal_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif