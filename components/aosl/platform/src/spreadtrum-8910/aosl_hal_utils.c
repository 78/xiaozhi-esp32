#include <stdio.h>

#include <hal/aosl_hal_time.h>

extern uint32_t TM_GetTotalSeconds (void);
extern uint32_t SLEEP_GetCur32kNum (void);
int aosl_hal_get_uuid(char buf[], int buf_sz)
{
  uint32_t tick_now, ts_sec;
  uint32_t t1, t2;

  tick_now = (uint32_t)aosl_hal_get_tick_ms ();
  ts_sec = TM_GetTotalSeconds ();

  t1 = SLEEP_GetCur32kNum ();
  aosl_hal_msleep(1);
  t2 = SLEEP_GetCur32kNum ();

  snprintf (buf, buf_sz, "%08x%08x%08x%08x", tick_now, ts_sec, t1, t2);

  return 0;
}

int aosl_hal_os_version (char buf [], int buf_sz)
{
  snprintf(buf, buf_sz, "%s", "spreadtrum 8910");
  return 0;
}
