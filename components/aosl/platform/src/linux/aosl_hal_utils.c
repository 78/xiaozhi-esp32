#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "stdio.h"

int aosl_hal_get_uuid (char buf [], int buf_sz)
{
	char uuid_buf [256];
	int fd;
	int err;
	int s, d;

	if (buf_sz <= 1) {
		return -1;
	}

	fd = open ("/proc/sys/kernel/random/uuid", O_RDONLY);
	if (fd < 0) {
		err = -1;
	} else {
		err = read (fd, uuid_buf, sizeof uuid_buf);
		if (err < 0)
			err = -1;

		close (fd);

		if (err < 0) {
			err = -1;
		}
	}

	for (s = 0, d = 0; s < (int)err; s++) {
		if ((size_t)d >= buf_sz - 1)
			break;

		if (uuid_buf [s] != '-' && uuid_buf [s] != '\n' && uuid_buf [s] != '\r') {
			buf [d] = uuid_buf [s];
			d++;
		}
	}

	buf [d] = '\0';

	if (err < 0)
		return -1;

	return 0;
}

int aosl_hal_os_version (char buf [], int buf_sz)
{
	int fd;
	int err;

	if (buf_sz <= 1) {
		return -1;
	}

	fd = open ("/proc/version", O_RDONLY);
	if (fd < 0) {
		err = -1;
	} else {
		err = read (fd, buf, buf_sz - 1);
		if (err < 0)
			err = -1;

		close (fd);

		if (err < 0) {
			err = -1;
		}
	}

	if (err < 0) {
		buf [0] = '\0';
	} else {
		buf [err] = '\0';
	}

	if (err < 0)
		return -1;

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
