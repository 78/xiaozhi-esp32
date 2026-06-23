#include <stdio.h>
#include <stdarg.h>

#include <hal/aosl_hal_log.h>

int aosl_hal_printf(const char *format, va_list args)
{
  return vprintf(format, args);
}
