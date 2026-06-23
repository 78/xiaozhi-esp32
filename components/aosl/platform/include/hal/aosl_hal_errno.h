/***************************************************************************
 * Module:	errno hal definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_ERRNO_H__
#define __AOSL_HAL_ERRNO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define AOSL_HAL_RET_SUCCESS          0
#define AOSL_HAL_RET_FAILURE         -1
#define AOSL_HAL_RET_EHAL            -2000
#define AOSL_HAL_RET_EAGAIN          -2001
#define AOSL_HAL_RET_EINTR           -2002
#define AOSL_HAL_RET_EINPROGRESS     -2003

/**
 * @brief Convert standard errno to AOSL HAL error codes
 * @param [in] errnum system errno value
 * @return Corresponding AOSL HAL error number
 */
int aosl_hal_errno_convert(int errnum);

#ifdef __cplusplus
}
#endif

#endif