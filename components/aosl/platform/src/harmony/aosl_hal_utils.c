#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

int aosl_hal_get_uuid (char buf [], int buf_sz)
{
	int fd;
	int err;
	int s, d;
	unsigned char rand_bytes[16];

	if (buf_sz <= 1) {
		return -1;
	}

	/* HarmonyOS NEXT does not have /proc/sys/kernel/random/uuid.
	 * Generate a UUID from /dev/urandom instead. */
	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	err = 0;
	int off = 0;
	while (off < (int)sizeof(rand_bytes)) {
		int rd = read(fd, rand_bytes + off, sizeof(rand_bytes) - off);
		if (rd < 0) {
			if (errno == EINTR)
				continue;
			err = -1;
			break;
		}
		off += rd;
	}
	close(fd);

	if (err < 0) {
		return -1;
	}

	/* Format as hex string (no dashes) */
	static const char hex[] = "0123456789abcdef";
	for (s = 0, d = 0; s < (int)sizeof(rand_bytes) && d < buf_sz - 1; s++) {
		buf[d++] = hex[(rand_bytes[s] >> 4) & 0x0f];
		if (d < buf_sz - 1)
			buf[d++] = hex[rand_bytes[s] & 0x0f];
	}
	buf[d] = '\0';

	return 0;
}

int aosl_hal_os_version (char buf [], int buf_sz)
{
	struct utsname uts;

	if (buf_sz <= 1) {
		return -1;
	}

	/* HarmonyOS NEXT does not have /proc/version.
	 * Use uname() to retrieve system information. */
	if (uname(&uts) != 0) {
		buf[0] = '\0';
		return -1;
	}

	snprintf(buf, buf_sz, "%s %s %s %s",
	         uts.sysname, uts.release, uts.version, uts.machine);
	return 0;
}

int aosl_hal_rand_bytes (void *buf, int len)
{
	int fd;
	int rd;
	int off = 0;

	if (buf == NULL || len <= 0)
		return -1;

	fd = open ("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return -1;

	while (off < len) {
		rd = read (fd, (char *)buf + off, len - off);
		if (rd < 0) {
			if (errno == EINTR)
				continue;
			close (fd);
			return -1;
		}
		off += rd;
	}

	close (fd);
	return 0;
}
