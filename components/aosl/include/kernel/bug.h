/***************************************************************************
 * Module:	bug
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_BUG_H__
#define __AOSL_BUG_H__

#include <stdio.h>
#include <stdlib.h>

#include <api/aosl_types.h>
#include <kernel/compiler.h>
#include <kernel/log.h>

extern void bug_slowpath (const char *file, int line, void *caller, const char *fmt, ...);
#define BUG(...) bug_slowpath (__FILE__, __LINE__, FuncReturnAddress (), __VA_ARGS__)

#define BUG_ON(x) if (unlikely (x)) BUG (NULL)


#define panic(...) \
	do { \
		char __buf [512]; \
		snprintf (__buf, sizeof __buf, __VAR_ARGS__); \
		aosl_printf ("PANIC @%s(%d): %s", __FILE__, __LINE__, __buf); \
		exit (-1); \
	} while(0)



#endif	/* __BUG_H__ */
