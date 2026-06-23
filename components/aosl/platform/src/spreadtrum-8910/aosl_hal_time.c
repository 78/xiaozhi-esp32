#include "os_api.h"
#include <stdio.h>

#include <hal/aosl_hal_time.h>

uint64_t aosl_hal_get_tick_ms(void)
{
  return (uint64_t)SCI_GetTickCount();
}

uint64_t aosl_hal_get_time_ms(void)
{
  return (uint64_t)SCI_GetTickCount();
}

int aosl_hal_get_time_str(char *buf, int len)
{
  uint32_t now;

  if (NULL == buf || 0 == len) {
    return -1;
  }

  now = SCI_GetTickCount();
  snprintf(buf, len, "%u", now);
  return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
  if (0 == ms) {
    ms = 1;
  }

  SCI_Sleep(ms);
}
