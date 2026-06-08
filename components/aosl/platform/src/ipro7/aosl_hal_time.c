#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <ipro_osal.h>

#include <hal/aosl_hal_time.h>

uint64_t aosl_hal_get_tick_ms(void)
{
  return (uint64_t)ipro_osal_get_time_ms();
}

uint64_t aosl_hal_get_time_ms(void)
{
  return (uint64_t)ipro_osal_get_time_ms();
}

// Format: yyyy-mm-dd hh:mm:ss.xxx
int aosl_hal_get_time_str(char *buf, int len)
{
  struct tm time_tm;
  struct timeval tv;
  time_t now_sec;
  char time_str[24];

  if (NULL == buf || 0 == len) {
    return -1;
  }

  // Get current time
  if (gettimeofday(&tv, NULL) < 0) {
    // Fallback to tick milliseconds if real-time clock not available
    uint32_t now_ms = ipro_osal_get_time_ms();
    snprintf(buf, len, "%u", now_ms);
    return 0;
  }

  now_sec = tv.tv_sec;
  
  // Format: yyyy-mm-dd hh:mm:ss
  if (localtime_r(&now_sec, &time_tm) == NULL) {
    // Fallback if localtime_r fails
    snprintf(buf, len, "%llu", (unsigned long long)(tv.tv_sec * 1000 + tv.tv_usec / 1000));
    return 0;
  }
  
  strftime(time_str, sizeof(time_str), "%F %T", &time_tm);
  
  // Append milliseconds: .xxx
  snprintf(time_str + 19, 5, ".%03u", (uint32_t)(tv.tv_usec / 1000));
  
  snprintf(buf, len, "%s", time_str);
  return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
  if (0 == ms) {
    ms = 1;
  }

  ipro_osal_delay_ms(ms);
}
