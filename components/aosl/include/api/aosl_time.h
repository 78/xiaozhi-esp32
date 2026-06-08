/***************************************************************************
 * Module:	Time relative utilities header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_API_TIME_H__
#define __AOSL_API_TIME_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The AOSL timestamp type */
typedef uint64_t aosl_ts_t;


/**
 * @brief Get the monotonic tick count in platform-native resolution.
 * @return  the current tick value
 **/
extern __aosl_api__ aosl_ts_t aosl_tick_now (void);

/**
 * @brief Get the monotonic tick count in milliseconds.
 * @return  the current tick in milliseconds
 **/
extern __aosl_api__ aosl_ts_t aosl_tick_ms (void);

/**
 * @brief Get the monotonic tick count in microseconds.
 * @return  the current tick in microseconds
 **/
extern __aosl_api__ aosl_ts_t aosl_tick_us (void);

/**
 * @brief Get the current wall-clock time in seconds since epoch.
 * @return  the current time in seconds
 **/
extern __aosl_api__ aosl_ts_t aosl_time_sec (void);

/**
 * @brief Get the current wall-clock time in milliseconds since epoch.
 * @return  the current time in milliseconds
 **/
extern __aosl_api__ aosl_ts_t aosl_time_ms (void);

/**
 * @brief Sleep for the specified number of milliseconds.
 * @param [in] ms  the duration to sleep in milliseconds
 **/
extern __aosl_api__ void aosl_msleep (uint64_t ms);

/**
 * @brief Format the current time as a human-readable string.
 * @param [out] buf  the buffer to write the time string into
 * @param [in]  len  the size of the buffer in bytes
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_time_str(char *buf, int len);

#ifdef __cplusplus
}
#endif


#endif /* __AOSL_API_TIME_H__ */
