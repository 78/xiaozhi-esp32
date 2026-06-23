/***************************************************************************
 * Module		:		AOSL log definition header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_LOG_H__
#define __AOSL_LOG_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>

/*
 * priorities/facilities are encoded into a single 32-bit quantity, where the
 * bottom 3 bits are the priority (0-7) and the top 28 bits are the facility
 * (0-big number).  Both the priorities and the facilities map roughly
 * one-to-one to strings in the syslogd(8) source code.  This mapping is
 * included in this file.
 *
 * priorities (these are ordered)
 */
#define	AOSL_LOG_EMERG    0	/* system is unusable */
#define	AOSL_LOG_ALERT    1	/* action must be taken immediately */
#define	AOSL_LOG_CRIT     2	/* critical conditions */
#define	AOSL_LOG_ERROR    3	/* error conditions */
#define	AOSL_LOG_WARNING  4	/* warning conditions */
#define	AOSL_LOG_NOTICE   5	/* normal but significant condition */
#define	AOSL_LOG_INFO     6	/* informational */
#define	AOSL_LOG_DEBUG    7	/* debug-level messages */

#ifdef __cplusplus
extern "C" {
#endif


#ifndef _MSC_VER
#define aosl_printf_fmt(a, b) __attribute__ ((format (printf, a, b)))
#define aosl_scanf_fmt(a, b) __attribute__ ((format (scanf, a, b)))
#else
#define aosl_printf_fmt(a, b)
#define aosl_scanf_fmt(a, b)
#endif

#define aosl_printf_fmt12 aosl_printf_fmt (1, 2)
#define aosl_printf_fmt23 aosl_printf_fmt (2, 3)



/**
 * @brief The log callback function.
 * The log level uses the same values as syslog, these values are used by AOSL library and
 * its' descendents, so please translate the log levels to your own log levels respectively.
 **/
typedef void (*aosl_vlog_t) (int level, const char *fmt, va_list args);

/**
 * @brief The default AOSL log function is syslog, but you can provide your own log function.
 * Pay enough attention please, your own log function MUST conform the same semantics
 * with syslog, and all log levels must have the same meaning with the ones of syslog.
 **/
extern __aosl_api__ void aosl_set_vlog_func (aosl_vlog_t vlog);

/**
 * @brief Get the aosl log level, AOSL_LOG_ERR is the default value.
 * Parameters:
 *     none.
 * Return value:
 *     the current aosl log level.
 **/
extern __aosl_api__ int aosl_get_log_level (void);

/**
 * @brief Set the aosl log level, AOSL_LOG_ERR is the default value.
 * Parameters:
 *     level: the log level you want to set
 * Return value:
 *     none.
 **/
extern __aosl_api__ void aosl_set_log_level (int level);

/**
 * @brief Output a log message with the specified level.
 * @param [in] level  the log level (e.g. AOSL_LOG_DEBUG, AOSL_LOG_ERROR)
 * @param [in] fmt    printf-style format string
 * @param [in] ...    format arguments
 **/
extern __aosl_api__ void aosl_printf_fmt23 aosl_log (int level, const char *fmt, ...);

/**
 * @brief Output a log message with the specified level (va_list version).
 * @param [in] level  the log level
 * @param [in] fmt    printf-style format string
 * @param [in] args   variable argument list
 **/
extern __aosl_api__ void aosl_vlog (int level, const char *fmt, va_list args);

/**
 * @brief Print a formatted message to the default output.
 * @param [in] fmt  printf-style format string
 * @param [in] ...  format arguments
 **/
extern __aosl_api__ void aosl_printf_fmt12 aosl_printf (const char *fmt, ...);

/**
 * @brief Print a formatted message to the default output (va_list version).
 * @param [in] fmt   printf-style format string
 * @param [in] args  variable argument list
 **/
extern __aosl_api__ void aosl_vprintf (const char *fmt, va_list args);

/**
 * @brief Print a formatted panic message and abort the program.
 * This function does not return.
 * @param [in] fmt  printf-style format string
 * @param [in] ...  format arguments
 **/
extern __aosl_api__ void aosl_printf_fmt12 aosl_panic (const char *fmt, ...);


#define AOSL_LOG(level, fmt, ...) aosl_log(level, "[%d][aosl][%s:%u]" fmt "\n", level, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define AOSL_LOG_DBG(fmt, ...) AOSL_LOG(AOSL_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define AOSL_LOG_INF(fmt, ...) AOSL_LOG(AOSL_LOG_INFO, fmt, ##__VA_ARGS__)
#define AOSL_LOG_NTC(fmt, ...) AOSL_LOG(AOSL_LOG_NOTICE, fmt, ##__VA_ARGS__)
#define AOSL_LOG_WRN(fmt, ...) AOSL_LOG(AOSL_LOG_WARNING, fmt, ##__VA_ARGS__)
#define AOSL_LOG_ERR(fmt, ...) AOSL_LOG(AOSL_LOG_ERROR, fmt, ##__VA_ARGS__)
#define AOSL_LOG_CRT(fmt, ...) AOSL_LOG(AOSL_LOG_CRIT, fmt, ##__VA_ARGS__)
#define AOSL_LOG_ALT(fmt, ...) AOSL_LOG(AOSL_LOG_ALERT, fmt, ##__VA_ARGS__)
#define AOSL_LOG_EMG(fmt, ...) AOSL_LOG(AOSL_LOG_EMERG, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif



#endif /* __AOSL_LOG_H__ */