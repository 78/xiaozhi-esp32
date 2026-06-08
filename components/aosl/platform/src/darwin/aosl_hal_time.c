/* Darwin/iOS HAL time - uses mach_absolute_time for monotonic clock */
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <mach/mach_time.h>

#include <hal/aosl_hal_time.h>

static mach_timebase_info_data_t s_timebase_info;

static void ensure_timebase_info(void)
{
	if (s_timebase_info.denom == 0) {
		mach_timebase_info(&s_timebase_info);
	}
}

uint64_t aosl_hal_get_tick_ms(void)
{
	ensure_timebase_info();
	uint64_t abs_time = mach_absolute_time();
	/* Convert to nanoseconds then to milliseconds */
	uint64_t ns = abs_time * s_timebase_info.numer / s_timebase_info.denom;
	return ns / 1000000;
}

uint64_t aosl_hal_get_time_ms(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0)
		return 0;
	return (((uint64_t)tv.tv_sec * (uint64_t)1000) + tv.tv_usec / 1000);
}

int aosl_hal_get_time_str(char *buf, int len)
{
	struct tm time_tm;
	uint64_t now_ms;
	time_t now_sec;
	char time_str[24];

	if (!buf || !len) return -1;

	now_ms = aosl_hal_get_time_ms();
	now_sec = now_ms / 1000;

	strftime(time_str, sizeof(time_str), "%F %T", localtime_r(&now_sec, &time_tm));
	sprintf(time_str + 19, ".%03u", (uint32_t)(now_ms % 1000));
	snprintf(buf, len, "%s", time_str);
	return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
	usleep(ms * 1000);
}
