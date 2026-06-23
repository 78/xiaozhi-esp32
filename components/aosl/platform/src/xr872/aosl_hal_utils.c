#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "efpg/efpg.h"
#include "kernel/os/os.h"
#include "kernel/os/os_time.h"
#include <hal/aosl_hal_time.h>
#include "driver/chip/hal_crypto.h"
int aosl_hal_get_uuid (char buf [], int buf_sz)
{
	if (buf_sz <= 1) {
		return -1;
	}
#if 0
	uint64_t chip64;
	uint8_t chipid[16];
	char uuid[33] = {0}; // 32 characters for hex string + null terminator
	if(efpg_read(EFPG_FIELD_CHIPID, chipid) != 0) {
		// If reading chip ID fails, fallback to using timestamp and random values
		chip64 = (uint64_t) aosl_hal_get_tick_ms ();
		memcpy(chipid, &chip64, sizeof(chipid));
	}
	
	for (int i = 0; i < 16; i++) {
		snprintf(uuid + strlen(uuid), sizeof(uuid) - strlen(uuid), "%02x", chipid[i]);
	}
	snprintf(buf, buf_sz, "%s", uuid);
#else

	uint8_t bytes[16];
	HAL_PRNG_Generate(bytes, sizeof(bytes));
	char uuid[33] = {0}; // 32 characters for hex string + null terminator
	for (int i = 0; i < 16; i++) {
		snprintf(uuid + strlen(uuid), sizeof(uuid) - strlen(uuid), "%02x", bytes[i]);
	}
	snprintf(buf, buf_sz, "%s", uuid);
#endif
	return 0;
}

int aosl_hal_os_version (char buf [], int buf_sz)
{
	if (buf_sz <= 1) {
		return -1;
	}
	snprintf(buf, buf_sz, "%s", "xr872-at");
	return 0;
}