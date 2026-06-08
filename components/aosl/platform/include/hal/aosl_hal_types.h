/***************************************************************************
 * Module:	HAL common type definitions.
 *
 * Copyright (c) 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_TYPES_H__
#define __AOSL_HAL_TYPES_H__

#include <stdint.h>

#ifndef __inline__
#if defined(_MSC_VER)
#define __inline__ __inline
#else
#define __inline__ inline
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
typedef uintptr_t aosl_fd_t;
#define AOSL_INVALID_FD ((aosl_fd_t)UINTPTR_MAX)
#else
typedef int aosl_fd_t;
#define AOSL_INVALID_FD ((aosl_fd_t)-1)
#endif

static __inline__ int aosl_fd_invalid(aosl_fd_t fd)
{
	return fd == AOSL_INVALID_FD;
}

#ifdef __cplusplus
}
#endif

#endif /* __AOSL_HAL_TYPES_H__ */
