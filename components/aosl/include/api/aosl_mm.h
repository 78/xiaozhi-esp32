/***************************************************************************
 * Module:	Memory Management relative utilities header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_MM_H__
#define __AOSL_MM_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate a block of memory of the specified size.
 * @param [in] size  the number of bytes to allocate
 * @return           pointer to the allocated memory, or NULL on failure
 **/
extern __aosl_api__ void *aosl_malloc_impl (size_t size);

/**
 * @brief Allocate and zero-initialize an array of elements.
 * @param [in] nmemb  the number of elements
 * @param [in] size   the size of each element in bytes
 * @return            pointer to the allocated memory, or NULL on failure
 **/
extern __aosl_api__ void *aosl_calloc_impl (size_t nmemb, size_t size);

/**
 * @brief Resize a previously allocated memory block.
 * @param [in] ptr   pointer to the previously allocated memory (or NULL)
 * @param [in] size  the new size in bytes
 * @return           pointer to the reallocated memory, or NULL on failure
 **/
extern __aosl_api__ void *aosl_realloc_impl (void *ptr, size_t size);

/**
 * @brief Duplicate a null-terminated string.
 * @param [in] s  the string to duplicate
 * @return        pointer to the newly allocated copy, or NULL on failure
 **/
extern __aosl_api__ char *aosl_strdup_impl (const char *s);

/**
 * @brief Free a previously allocated memory block.
 * @param [in] ptr  pointer to the memory to free (NULL is safe)
 **/
extern __aosl_api__ void aosl_free_impl (void *ptr);

#if defined(CONFIG_AOSL_MEM_STAT) && defined(CONFIG_AOSL_MEM_DUMP)
/**
 * @brief Debug version of aosl_malloc_impl with caller tracking.
 * @param [in] size  the number of bytes to allocate
 * @param [in] func  the caller function name (__FUNCTION__)
 * @param [in] line  the caller line number (__LINE__)
 * @return           pointer to the allocated memory, or NULL on failure
 **/
extern __aosl_api__ void *aosl_malloc_impl_dbg (size_t size, const char *func, int line);

/**
 * @brief Debug version of aosl_calloc_impl with caller tracking.
 * @param [in] nmemb  the number of elements
 * @param [in] size   the size of each element in bytes
 * @param [in] func   the caller function name
 * @param [in] line   the caller line number
 * @return            pointer to the allocated memory, or NULL on failure
 **/
extern __aosl_api__ void *aosl_calloc_impl_dbg (size_t nmemb, size_t size, const char *func, int line);

/**
 * @brief Debug version of aosl_realloc_impl with caller tracking.
 * @param [in] ptr   pointer to the previously allocated memory
 * @param [in] size  the new size in bytes
 * @param [in] func  the caller function name
 * @param [in] line  the caller line number
 * @return           pointer to the reallocated memory, or NULL on failure
 **/
extern __aosl_api__ void *aosl_realloc_impl_dbg (void *ptr, size_t size, const char *func, int line);

/**
 * @brief Debug version of aosl_strdup_impl with caller tracking.
 * @param [in] s     the string to duplicate
 * @param [in] func  the caller function name
 * @param [in] line  the caller line number
 * @return           pointer to the newly allocated copy, or NULL on failure
 **/
extern __aosl_api__ char *aosl_strdup_impl_dbg (const char *s, const char *func, int line);

/** @brief Allocate memory. @see aosl_malloc_impl */
#define aosl_malloc(size)         aosl_malloc_impl_dbg(size, __FUNCTION__, __LINE__)
/** @brief Allocate zero-initialized memory. @see aosl_calloc_impl */
#define aosl_calloc(nmemb, size)  aosl_calloc_impl_dbg(nmemb, size, __FUNCTION__, __LINE__)
/** @brief Resize a memory block. @see aosl_realloc_impl */
#define aosl_realloc(ptr, size)   aosl_realloc_impl_dbg(ptr, size, __FUNCTION__, __LINE__)
/** @brief Duplicate a string. @see aosl_strdup_impl */
#define aosl_strdup(s)            aosl_strdup_impl_dbg(s, __FUNCTION__, __LINE__)
/** @brief Free a memory block. @see aosl_free_impl */
#define aosl_free(ptr)            aosl_free_impl(ptr)
#else
/** @brief Allocate memory. @see aosl_malloc_impl */
#define aosl_malloc(size)         aosl_malloc_impl(size)
/** @brief Allocate zero-initialized memory. @see aosl_calloc_impl */
#define aosl_calloc(nmemb, size)  aosl_calloc_impl(nmemb, size)
/** @brief Resize a memory block. @see aosl_realloc_impl */
#define aosl_realloc(ptr, size)   aosl_realloc_impl(ptr, size)
/** @brief Duplicate a string. @see aosl_strdup_impl */
#define aosl_strdup(s)            aosl_strdup_impl(s)
/** @brief Free a memory block. @see aosl_free_impl */
#define aosl_free(ptr)            aosl_free_impl(ptr)
#endif // end CONFIG_AOSL_MEM_DUMP

/**
 * @brief Get the total amount of memory currently allocated via AOSL allocators.
 * @return  the number of bytes currently in use
 **/
extern __aosl_api__ size_t aosl_memused(void);

/**
 * @brief Dump the current memory allocation statistics to the log output.
 **/
extern __aosl_api__ void   aosl_memdump(void);

/**
 * @brief Dump memory allocation statistics into a user-provided buffer.
 * @param [out] cnts  array of 2 ints: cnts[0] receives alloc count, cnts[1] receives free count
 * @param [out] buf   the buffer to write the dump string into
 * @param [in]  len   the size of the buffer in bytes
 * @return            0 on success, <0 on failure
 **/
extern __aosl_api__ int    aosl_memdump_r(int cnts[2], char *buf, int len);

#ifdef __cplusplus
}
#endif



#endif /* __AOSL_MM_H__ */