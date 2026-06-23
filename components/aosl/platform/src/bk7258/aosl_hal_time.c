#include <os/os.h>
#include <unistd.h>
#include <stdio.h>

#include <hal/aosl_hal_time.h>

uint64_t aosl_hal_get_tick_ms(void)
{
  return (uint64_t)rtos_get_time();
}

uint64_t aosl_hal_get_time_ms(void)
{
  return (uint64_t)rtos_get_time();
}

int aosl_hal_get_time_str(char *buf, int len)
{
  uint32_t now;

  if (NULL == buf || 0 == len) {
    return -1;
  }

  now = rtos_get_time();
  snprintf(buf, len, "%u", now);
  return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
  if (0 == ms) {
    ms = 1;
  }

  rtos_delay_milliseconds(ms);
}
