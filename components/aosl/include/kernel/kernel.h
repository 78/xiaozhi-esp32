/***************************************************************************
 * Module:	kernel
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_KERNEL_H__
#define __AOSL_KERNEL_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <kernel/compiler.h>
#include <kernel/bug.h>
#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_mm.h>

#define AOSL__ALIGN_MASK(x, mask)	(((uintptr_t)(x) + (mask)) & ((intptr_t)~(mask)))
#define AOSL_I_ALIGN(x, a)		AOSL__ALIGN_MASK(x, (a) - 1)
#define AOSL_I_ALIGN_PTR(x)		AOSL_I_ALIGN(x, sizeof (void *))

#define AOSL_P_ALIGN(p, a)		((void *)AOSL_I_ALIGN((uintptr_t)(p), (a)))
#define AOSL_P_ALIGN_PTR(x)			AOSL_P_ALIGN(x, sizeof (void *))
#define AOSL_IS_ALIGNED(x, a)		(((uintptr_t)(x) & ((a) - 1)) == 0)
#define AOSL_IS_ALIGNED_PTR(x)		(((uintptr_t)(x) & (sizeof (void *) - 1)) == 0)

#endif /* __AOSL_KERNEL_H__ */