/*
 * Darwin/iOS HAL utils
 *
 * Key differences from Linux:
 * - No /proc/sys/kernel/random/uuid -> use arc4random or SecRandomCopyBytes
 * - No /proc/version -> use sysctl for OS version
 * - /dev/urandom exists on macOS but use arc4random_buf on iOS for simplicity
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int aosl_hal_get_uuid(char buf[], int buf_sz)
{
	if (buf_sz <= 1) return -1;

	/*
	 * Generate a random UUID using arc4random.
	 * Format: 32 hex chars (no dashes).
	 */
	uint8_t bytes[16];
	arc4random_buf(bytes, sizeof(bytes));

	/* Set version 4 and variant bits */
	bytes[6] = (bytes[6] & 0x0F) | 0x40;
	bytes[8] = (bytes[8] & 0x3F) | 0x80;

	int d = 0;
	for (int i = 0; i < 16 && d < buf_sz - 1; i++) {
		int written = snprintf(buf + d, buf_sz - d, "%02x", bytes[i]);
		if (written < 0) break;
		d += written;
	}
	buf[d] = '\0';
	return 0;
}

int aosl_hal_os_version(char buf[], int buf_sz)
{
	if (buf_sz <= 1) return -1;

	char ostype[64] = {0};
	char osrelease[64] = {0};
	size_t len;

	len = sizeof(ostype);
	sysctlbyname("kern.ostype", ostype, &len, NULL, 0);

	len = sizeof(osrelease);
	sysctlbyname("kern.osrelease", osrelease, &len, NULL, 0);

	snprintf(buf, buf_sz, "%s %s", ostype, osrelease);
	return 0;
}

int aosl_hal_rand_bytes(void *buf, int len)
{
	if (buf == NULL || len <= 0) return -1;
	arc4random_buf(buf, len);
	return 0;
}
