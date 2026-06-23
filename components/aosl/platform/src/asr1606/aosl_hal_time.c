#include <stdio.h>

#include <ql_rtos.h>
#include <hal/aosl_hal_time.h>

/* ASR1606 ThreadX tick = 5ms, ql_rtos_get_systicks() returns tick count */

uint64_t aosl_hal_get_tick_ms(void)
{
  return (uint64_t)ql_rtos_get_systicks() * 5;
}

uint64_t aosl_hal_get_time_ms(void)
{
  /* No RTC wall-clock easily available, use monotonic tick */
  return (uint64_t)ql_rtos_get_systicks() * 5;
}

int aosl_hal_get_time_str(char *buf, int len)
{
  uint64_t now;

  if (NULL == buf || 0 == len) {
    return -1;
  }

  now = aosl_hal_get_tick_ms();
  snprintf(buf, len, "%llu", now);
  return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
  if (0 == ms) {
    return;  /* 0ms = return immediately, don't sleep a full tick */
  }

  ql_rtos_task_sleep_ms((u32)ms);
}
