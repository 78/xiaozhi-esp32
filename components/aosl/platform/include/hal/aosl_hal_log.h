/***************************************************************************
 * Module:	log hal definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_LOG_H__
#define __AOSL_HAL_LOG_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief print formatted output
 * @param format format string
 * @param args variable arguments list
 * @return number of characters printed, or -1 on error
 */
int aosl_hal_printf(const char *format, va_list args);

#ifdef __cplusplus
}
#endif

#endif