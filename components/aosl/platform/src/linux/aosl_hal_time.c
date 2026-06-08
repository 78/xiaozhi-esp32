#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h> /* just for perror */

#include <hal/aosl_hal_time.h>

uint64_t aosl_hal_get_tick_ms (void)
{
	struct timespec ts;
	if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0) {
		perror ("retrieve the time info");
		return 0;
	}

	return (((uint64_t)ts.tv_sec * (uint64_t)1000) + ts.tv_nsec / 1000000);
}

uint64_t aosl_hal_get_time_ms (void)
{
	struct timeval tv;
	if (gettimeofday (&tv, NULL) < 0)
		return 0;

	return (((uint64_t)tv.tv_sec * (uint64_t)1000) + tv.tv_usec / 1000);
}

//yyyy-mm-dd hh:mm:ss.xxx
int aosl_hal_get_time_str(char *buf, int len)
{
	struct tm time_tm;
	uint64_t now_ms;
	time_t now_sec;
	char time_str[24];

	if (!buf || !len) {
		return -1;
	}

	now_ms = aosl_hal_get_time_ms();
	now_sec = now_ms / 1000;

	strftime(time_str, sizeof(time_str), "%F %T", localtime_r(&now_sec, &time_tm));
	sprintf(time_str + 19, ".%03u", (uint32_t)(now_ms % 1000));
	snprintf(buf, len, "%s", time_str);
	return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
	usleep (ms * 1000);
}