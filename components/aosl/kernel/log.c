/***************************************************************************
 * Module:	log
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/log.h>
#include <hal/aosl_hal_log.h>

#define UNUSED(expr) (void)(expr)

static void ____default_vlog (int level, const char *fmt, va_list args)
{
	UNUSED(level);
	aosl_hal_printf(fmt, args);
}

static aosl_vlog_t vlog_func_p = ____default_vlog;
static int aosl_log_level = AOSL_LOG_ERROR;

__export_in_so__ void aosl_set_vlog_func (aosl_vlog_t vlog)
{
	if (vlog != NULL) {
		vlog_func_p = vlog;
	} else {
		vlog_func_p = ____default_vlog;
	}
}

__export_in_so__ int aosl_get_log_level (void)
{
	return aosl_log_level;
}

__export_in_so__ void aosl_set_log_level (int level)
{
	if (level >= AOSL_LOG_EMERG && level <= AOSL_LOG_DEBUG)
		aosl_log_level = level;
}

static inline void ____vlog (int level, const char *fmt, va_list args)
{
	if (vlog_func_p != NULL && level <= aosl_log_level) {
		vlog_func_p (level, fmt, args);
	}
}

__export_in_so__ void aosl_printf_fmt23 aosl_log (int level, const char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	____vlog (level, fmt, args);
	va_end (args);
}

__export_in_so__ void aosl_vlog (int level, const char *fmt, va_list args)
{
	____vlog (level, fmt, args);
}

__export_in_so__ void aosl_printf_fmt12 aosl_printf (const char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	____vlog (aosl_log_level, fmt, args);
	va_end (args);
}

__export_in_so__ void aosl_vprintf (const char *fmt, va_list args)
{
	____vlog (aosl_log_level, fmt, args);
}
