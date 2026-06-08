/***************************************************************************
 * Module:	AOSL utilities definition file.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_UTILS_H__
#define __AOSL_UTILS_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_mm.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief get device unique ID, generates at least 32 random characters
 * @param [out] buf buffer to store unique ID string
 * @param [in] buf_sz size of buffer, should > 1
 * @return 0 on success, < 0 on error
 */
extern __aosl_api__ int aosl_get_uuid (char buf [], size_t buf_sz);

/**
 * @brief get OS version string
 * @param [out] buf buffer to store OS version string
 * @param [in] buf_sz size of buffer, should > 1
 * @return 0 on success, < 0 on error
 */
extern __aosl_api__ int aosl_os_version (char buf [], size_t buf_sz);

/**
 * @brief Check whether the platform provides a hardware random number
 *        generator (entropy source) via the HAL layer.
 * @return 1 if hardware RNG is available, 0 otherwise
 */
extern __aosl_api__ int aosl_hwrng_available (void);

/**
 * @brief Fill buffer with random bytes from the platform hardware RNG.
 *        Falls back to failure if the HAL does not provide a hardware RNG.
 *        Use aosl_hwrng_available() to check availability before calling.
 * @param [out] buf buffer to fill with random bytes
 * @param [in] len number of random bytes to generate
 * @return 0 on success, < 0 on error (including when hardware RNG is unavailable)
 */
extern __aosl_api__ int aosl_rand_bytes (void *buf, size_t len);

#ifdef __cplusplus
}
#endif


#endif /* __AOSL_UTILS_H__ */