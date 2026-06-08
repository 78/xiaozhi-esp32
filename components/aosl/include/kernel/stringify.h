/***************************************************************************
 * Module:	stringify
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __KERNEL_STRINGIFY_H__
#define __KERNEL_STRINGIFY_H__

#define __stringify_1(x)	#x
#define __stringify(...)	__stringify_1(__VA_ARGS__)

#endif	/* !__KERNEL_STRINGIFY_H__ */