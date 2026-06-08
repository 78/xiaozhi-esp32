#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_system.h"
#include "esp_random.h"
#include "esp_mac.h"

#include <hal/aosl_hal_time.h>

int aosl_hal_get_uuid (char buf [], int buf_sz)
{
	if (buf_sz <= 1) {
		return -1;
	}

	uint8_t mac[6];
	uint32_t m;
	uint32_t ts;
	uint32_t r1, r2;
	uint32_t seed;
	
	/* Get MAC address */
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	
	/* Get current timestamp */
	ts =(uint32_t)aosl_hal_get_tick_ms();
	
	/* Use MAC address and timestamp as random seed */
	m = ((uint32_t)mac[0] << 24) | ((uint32_t)mac[1] << 16) | 
	       ((uint32_t)mac[2] << 8) | (uint32_t)mac[3];
	m ^= ((uint32_t)mac[4] << 8) | (uint32_t)mac[5];
	seed = m ^ ts;
	
	/* Use ESP32 hardware random number generator and mix with seed for enhanced randomness */
	r1 = esp_random() ^ seed;
	r2 = esp_random() ^ (seed >> 16);
	
	/* Combine MAC(12 chars) + TS(8 chars) + R1(8 chars) + R2(4 chars) = 32 chars */
	snprintf(buf, buf_sz, "%08x%08x%08x%08x", (unsigned int)m, (unsigned int)ts,
					 (unsigned int)r1, (unsigned int)r2);
	
	return 0;
}

int aosl_hal_os_version (char buf [], int buf_sz)
{
	if (buf_sz <= 1) {
		return -1;
	}
	snprintf(buf, buf_sz, "%s", "esp32-s3");
	return 0;
}

int aosl_hal_rand_bytes (void *buf, int len)
{
	int off = 0;
	uint32_t rnd;

	if (buf == NULL || len <= 0)
		return -1;

	/* Use ESP32 hardware RNG via esp_random() */
	while (off < len) {
		rnd = esp_random ();
		if (len - off >= (int)sizeof (uint32_t)) {
			memcpy ((char *)buf + off, &rnd, sizeof (uint32_t));
			off += sizeof (uint32_t);
		} else {
			memcpy ((char *)buf + off, &rnd, len - off);
			off = len;
		}
	}

	return 0;
}
