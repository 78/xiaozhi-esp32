/***************************************************************************
 * Module:	error
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __KERNEL_ERR_H__
#define __KERNEL_ERR_H__

#include <hal/aosl_hal_errno.h>
#include <api/aosl_errno.h>
#include <kernel/compiler.h>
#include <kernel/types.h>

/*
 * Kernel pointers have redundant information, so we can use a
 * scheme where we can return either an error code or a dentry
 * pointer with the same return value.
 *
 * This should be a per-architecture thing, to allow different
 * error and pointer decisions.
 */
#define MAX_ERRNO	4095

#define IS_ERR_VALUE(x) ((uintptr_t)(intptr_t)(x) >= (uintptr_t)(intptr_t)-MAX_ERRNO)

static inline void * ERR_PTR(intptr_t error)
{
	return (void *) error;
}

static inline intptr_t PTR_ERR(const void *ptr)
{
	return (intptr_t) ptr;
}

static inline intptr_t IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((uintptr_t)ptr);
}

static inline intptr_t IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((uintptr_t)ptr);
}


#define aosl_set_error(err) do { aosl_errno = -(err); } while (0)

static __inline__ int aosl_hal_set_error(int err)
{
	if (err < 0) {
		int newerr = 0;
		switch (err) {
			case AOSL_HAL_RET_EINTR:
				newerr = AOSL_EINTR;
				break;
			case AOSL_HAL_RET_EAGAIN:
				newerr = AOSL_EAGAIN;
				break;
			case AOSL_HAL_RET_EINPROGRESS:
				newerr = AOSL_EINPROGRESS;
				break;
			default:
				newerr = AOSL_EHAL;
				break;
		}
		aosl_set_error(-newerr);
		return -newerr;
	}

	return 0;
}

#define return_err(err) do { \
		intptr_t ____$err = (intptr_t)(err); \
		if (IS_ERR_VALUE ((uintptr_t)____$err)) { \
			aosl_set_error (____$err); \
			return -1; \
		} \
		return ____$err; \
	} while (0)

#define return_errno(err) do { \
		intptr_t ____$err = (intptr_t)(err); \
		if (IS_ERR_VALUE ((uintptr_t)____$err)) \
			return -____$err; \
		return ____$err; \
	} while (0)

#define return_ptr_err(ptr_err) do { \
		void *____$ptr_err = (void *)(intptr_t)(ptr_err); \
		if (IS_ERR_VALUE ((uintptr_t)____$ptr_err)) { \
			aosl_set_error ((intptr_t)____$ptr_err); \
			return NULL; \
		} \
			\
		/** \
		 * for returning ptr cases, returns NULL not always \
		 * indicates an error, so setting errno to 0 for no \
		 * error cases to tell the caller this is returning \
		 * NULL with no error. \
		 **/ \
		if (!____$ptr_err) { \
			aosl_set_error (0); \
			return NULL; \
		} \
		return ____$ptr_err; \
	} while (0)


#endif /* __KERNEL_ERR_H__ */
