/***************************************************************************
 * Module:	compile
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __KERNEL_COMPILER_H__
#define __KERNEL_COMPILER_H__

#ifndef likely
#define likely(x) (x)
#endif

#ifndef unlikely
#define unlikely(x) (x)
#endif

#ifndef FuncReturnAddress
#if defined(_MSC_VER)
#include <intrin.h>
#define FuncReturnAddress() _ReturnAddress()
#else
#define FuncReturnAddress() __builtin_return_address(0)
#endif
#endif

#endif /* __KERNEL_COMPILER_H__ */
