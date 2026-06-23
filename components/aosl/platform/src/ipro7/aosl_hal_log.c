/***************************************************************************
 * Module:	AOSL HAL Log - IPRO7 Platform
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL HAL for IPRO SDK.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 ***************************************************************************/
#include <stdio.h>

#include <hal/aosl_hal_log.h>

#ifdef CONFIG_IPRO_LOG_ENABLE
#include "ipro_log.h"

int aosl_hal_printf(const char *format, va_list args)
{
  /* Use IPRO log system with INFO level for AOSL output */
  ipro_log_writev(IPRO_LOG_LEVEL_INFO, "AOSL", format, args);
  return 0;
}

#else
/* Fallback to standard printf if IPRO log is not enabled */
int aosl_hal_printf(const char *format, va_list args)
{
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  printf("%s", buffer);
  return 0;
}
#endif /* CONFIG_IPRO_LOG_ENABLE */
