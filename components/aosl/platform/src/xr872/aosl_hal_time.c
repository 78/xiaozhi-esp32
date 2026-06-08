#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h> /* just for perror */

#include "FreeRTOS.h"
#include "task.h"
#include "kernel/os/os_time.h"

uint64_t aosl_hal_get_tick_ms(void)
{
	struct timespec ts;
	long ms = OS_TicksToMSecs(OS_GetTicks());
	ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
	return (((uint64_t)ts.tv_sec * (uint64_t)1000) + ts.tv_nsec / 1000000);
	// return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

uint64_t aosl_hal_get_time_ms(void)
{
	struct timeval tv;
	if (gettimeofday (&tv, NULL) < 0)
		return 0;

	return (((uint64_t)tv.tv_sec * (uint64_t)1000) + tv.tv_usec / 1000);
	// return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

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
	sprintf(time_str + 19, ".%03d", (int)(now_ms % 1000));
	snprintf(buf, len, "%s", time_str);
	return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
	OS_MSleep(ms);
}