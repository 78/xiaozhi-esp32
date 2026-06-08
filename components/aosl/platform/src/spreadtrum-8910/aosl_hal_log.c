#include <stdio.h>
#include <RTOS/export/inc/os_api.h>

#include <hal/aosl_hal_log.h>

int aosl_hal_printf(const char *format, va_list args)
{
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  SCI_TRACE_LOW("%s", buffer);
  return 0;
}
