#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "log.h"
#include "utility.h"

uint64_t util_get_time_ms(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		return 0;
	}
	return (((uint64_t)tv.tv_sec * (uint64_t)1000) + tv.tv_usec / 1000);
}

uint64_t util_get_time_us(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		return 0;
	}
	return (((uint64_t)tv.tv_sec * (uint64_t)1000000) + tv.tv_usec);
}

void util_sleep_ms(int64_t ms)
{
	if (ms > 0) {
		usleep(ms * 1000);
	}
}

void util_sleep_us(int64_t us)
{
	if (us > 0) {
		usleep(us);
	}
}

// get a string from file, return null if file does not exist
char *util_get_string_from_file(const char *path)
{
	FILE *f = fopen(path, "rb");

	if (!f) {
		LOGW("Failed to open %s\n", path);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *string = malloc(fsize + 1);
	if (fread(string, 1, fsize, f) != fsize) {
        fclose(f);
        free(string);
		return NULL;
	}
	fclose(f);

	string[fsize] = 0;
	return string;
}