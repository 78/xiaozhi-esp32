#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h> /* just for perror */

#include "FreeRTOS.h"
#include "task.h"

uint64_t aosl_hal_get_tick_ms(void)
{
	return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

uint64_t aosl_hal_get_time_ms(void)
{
	return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

int aosl_hal_get_time_str(char *buf, int len)
{
	uint64_t now_ms;

	if (!buf || !len) {
		return -1;
	}

	now_ms = aosl_hal_get_time_ms();
	snprintf(buf, len, "%llu", now_ms);
	return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
	vTaskDelay((ms + portTICK_PERIOD_MS / 2) / portTICK_PERIOD_MS);
}