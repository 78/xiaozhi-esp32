/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_NONE       0x0             /* Page may not be accessed */
#define PROT_READ       (1 << 0)        /* Page may be read */
#define PROT_WRITE      (1 << 1)        /* Page may be written */

#define MAP_SHARED      (1 << 0)        /* Share this mapping */

/**
 * @brief This is only used for video device to map video buffer to user side.
 *
 * @param addr   Video ignores this parameter, it's recommended to fixedly set NULL
 * @param length Mapped buffer length should <= video buffer length
 * @param prot   Video ignores this parameter, it's recommended to fixedly set (PROT_READ | PROT_WRITE)
 * @param flags  Video ignores this parameter, it's recommended to fixedly set MAP_SHARED
 * @param fd     Video device file description
 * @param offset Video buffer offset
 *
 * @return
 *      - Mapped video buffer pointer on success
 *      - NULL if failed
 */
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);

/**
 * @brief Free mapped video buffer
 *
 * @param addr   Mapped video buffer pointer
 * @param length Mapped buffer length should <= video buffer length
 *
 * @return
 *      - 0 on success
 *      - -1 if failed
 */
int munmap(void *addr, size_t length);

#ifdef __cplusplus
}
#endif
