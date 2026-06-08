#include <stdio.h>
#include <stdlib.h>
#include <ipro_osal.h>

#include <hal/aosl_hal_time.h>

static uint32_t random_a4(uint32_t a1, uint32_t a2, uint32_t a3)
{
	uint32_t hash = a1 ^ 0xffffffff;

	for (int i = 0; i < 4; i++) {
		uint8_t byte = (hash >> (8 * i)) & 0xFF;
		byte ^= (a1 >> (8 * i)) & 0xFF;
		byte ^= (a2 >> (8 * (3 - i))) & 0xFF;
		byte ^= (a3 >> (8 * ((i + 1) % 4))) & 0xFF;
		byte = (byte * 0x1B) ^ (byte >> 1);
		hash ^= (byte << (8 * i));
	}

	hash ^= a2;
	hash ^= hash << 13;
	hash ^= a3;
	hash ^= hash >> 17;

	return hash;
}

int aosl_hal_get_uuid(char buf[], int buf_sz)
{
	if (buf_sz <= 1) {
		return -1;
	}
	uint32_t tick;
	uint32_t r1, r2;
	uint32_t s;
	uint32_t m = 0xDEADBEEF;

	/* get system tick */
	tick = ipro_osal_get_time_ms();

	/* get random values */
	r1 = (uint32_t)rand();

	/* get random seed */
	s = random_a4(tick, r1, m);
	srand(s);

	/* get soft random value */
	r2 = (uint32_t)rand();

	snprintf (buf, buf_sz, "%08x%08x%08x%08x", m, tick, r1, r2);
	return 0;
}

int aosl_hal_os_version (char buf [], int buf_sz)
{
	if (buf_sz <= 1) {
		return -1;
	}
	snprintf(buf, buf_sz, "%s", "IPRO7");
	return 0;
}
