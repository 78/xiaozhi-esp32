/***************************************************************************
 * Module:	AOSL common definitions header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_DEFS_H__
#define __AOSL_DEFS_H__


/**
 * @brief Stringify a macro argument without expansion.
 * @param [in] x  the token to stringify
 */
#define aosl_stringify_1(x) #x

/**
 * @brief Stringify a macro argument with full expansion.
 * @param [in] x  the macro or token to expand and stringify
 */
#define aosl_stringify(x) aosl_stringify_1(x)

/**
 * @brief Compile-time assertion macro (C99 compatible)
 * 
 * This macro provides compile-time assertion functionality that works with C99.
 * If the condition is false, compilation will fail with an error about negative array size.
 * 
 * Usage: aosl_static_assert(sizeof(int) == 4, int_size_check);
 * 
 * @param [in] condition The condition to check at compile time
 * @param [in] name A unique identifier for this assertion (must be a valid C identifier)
 */
#define aosl_static_assert(condition, name) \
	typedef char aosl_static_assert_##name[(condition) ? 1 : -1]


#ifdef __cplusplus
extern "C" {
#endif



#ifndef container_of
#if defined (__GNUC__)
#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (void *)(ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})
#else
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type,member)))
#endif
#endif


/**
 * @brief Return the minimum of two values.
 * @param [in] x  the first value
 * @param [in] y  the second value
 */
#define aosl_min(x, y) ((x) < (y) ? (x) : (y))

/**
 * @brief Return the maximum of two values.
 * @param [in] x  the first value
 * @param [in] y  the second value
 */
#define aosl_max(x, y) ((x) > (y) ? (x) : (y))

/**
 * @brief Return the minimum of three values.
 * @param [in] x  the first value
 * @param [in] y  the second value
 * @param [in] z  the third value
 */
#define aosl_min3(x, y, z) aosl_min(aosl_min(x, y), z)

/**
 * @brief Return the maximum of three values.
 * @param [in] x  the first value
 * @param [in] y  the second value
 * @param [in] z  the third value
 */
#define aosl_max3(x, y, z) aosl_max(aosl_max(x, y), z)

/**
 * @brief Clamp a value to the range [lo, hi].
 * @param [in] val  the value to clamp
 * @param [in] lo   the lower bound
 * @param [in] hi   the upper bound
 */
#define aosl_clamp(val, lo, hi) aosl_min(aosl_max(val, lo), hi)


/* I think 64 args is big enough */
#define AOSL_VAR_ARGS_MAX 64

//#define BUILD_TARGET_SHARED

#if defined (__GNUC__) && defined(BUILD_TARGET_SHARED)
#define __export_in_so__ __attribute__ ((visibility ("default")))
#elif defined(_MSC_VER) && defined(BUILD_TARGET_SHARED) && defined(AGORA_BUILDING_API)
#define __export_in_so__ __declspec (dllexport)
#elif defined(_MSC_VER) && defined(BUILD_TARGET_SHARED) && !defined(AGORA_BUILDING_API)
#define __export_in_so__ __declspec (dllimport)
#else
#define __export_in_so__
#endif


#if defined(_MSC_VER) && defined(BUILD_TARGET_SHARED) && defined(AGORA_BUILDING_API)
#define __aosl_api__ __declspec (dllexport)
#elif defined(_MSC_VER) && defined(BUILD_TARGET_SHARED) && !defined(AGORA_BUILDING_API)
#define __aosl_api__ __declspec (dllimport)
#else
#define __aosl_api__
#endif

#if defined(__USE_GLOBAL_RODATA__)
#define __GLOBAL_RODATA__ __attribute__((section(".rodata")))
#else
#define __GLOBAL_RODATA__
#endif
#ifdef __cplusplus
}
#endif

#endif /* __AOSL_DEFS_H__ */
