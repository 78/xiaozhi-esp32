/***************************************************************************
 * Module:	panic
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <kernel/kernel.h>
#include <api/aosl_types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/thread.h>

void bug_slowpath (const char *file, int line, void *caller, const char *fmt, ...)
{
	k_thread_t this;
	char thread_name [64] = {0};
	va_list args;

	this = k_thread_self ();
	strcpy (thread_name, "thread");
	if (aosl_hal_thread_get_name (thread_name, sizeof thread_name) != 0)
		strcpy (thread_name, "thread");

	aosl_log (AOSL_LOG_EMERG, "------------[ cut here ]------------\n");
	aosl_log (AOSL_LOG_EMERG, "BUG(thread-%s/%p): %s:%d, caller=%p\n", thread_name, (void *)(uintptr_t)this, file, line, caller);

	va_start (args, fmt);
	aosl_vlog (AOSL_LOG_EMERG, fmt, args);
	va_end (args);

	abort ();
	*(int *)123 = 456;
	exit (-1);
}

__export_in_so__ void aosl_printf_fmt12 aosl_panic (const char *fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	aosl_vlog (AOSL_LOG_EMERG, fmt, args);
	va_end (args);

	abort ();
}
