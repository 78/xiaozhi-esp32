/***************************************************************************
 * Module:	utils hal definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_UTILS_H__
#define __AOSL_HAL_UTILS_H__

#include <hal/aosl_hal_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief get device unique ID, generates at least 32 random characters
 * @param [out] buf buffer to store unique ID string
 * @param [in] buf_sz size of buffer, should > 1
 * @return 0 on success, < 0 on error
 */
int aosl_hal_get_uuid (char buf [], int buf_sz);

/**
 * @brief get OS version string
 * @param [out] buf buffer to store OS version string
 * @param [in] buf_sz size of buffer, should > 1
 * @return 0 on success, < 0 on error
 */
int aosl_hal_os_version (char buf [], int buf_sz);

#if AOSL_HAL_HAVE_HWRNG
/**
 * @brief Fill buffer with hardware random bytes (entropy source).
 *        This function should be implemented by the platform HAL when
 *        a hardware RNG or OS-level entropy source is available
 *        (e.g. /dev/urandom on Linux, esp_random() on ESP32).
 * @param [out] buf buffer to fill with random bytes
 * @param [in] len number of random bytes to generate
 * @return 0 on success, < 0 on error
 */
int aosl_hal_rand_bytes (void *buf, int len);
#endif

#ifdef __cplusplus
}
#endif

#endif