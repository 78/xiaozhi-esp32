#include <hal/aosl_hal_iomp.h>
#include <hal/aosl_hal_memory.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>


fd_set_t aosl_hal_fdset_create()
{
	fd_set *fds = aosl_hal_malloc(sizeof(fd_set));
	if (!fds) {
		return NULL;
	}
	FD_ZERO(fds);
	return fds;
}

void aosl_hal_fdset_destroy(fd_set_t fdset)
{
	if (fdset) {
		aosl_hal_free(fdset);
	}
}

void aosl_hal_fdset_zero(fd_set_t fdset)
{
	if (!fdset) {
		return;
	}
	FD_ZERO((fd_set *)fdset);
}

void aosl_hal_fdset_set(fd_set_t fdset, aosl_fd_t fd)
{
	if (!fdset) {
		return;
	}
	FD_SET(fd, (fd_set *)fdset);
}

void aosl_hal_fdset_clr(fd_set_t fdset, aosl_fd_t fd)
{
	if (!fdset) {
		return;
	}
	FD_CLR(fd, (fd_set *)fdset);
}

int aosl_hal_fdset_isset(fd_set_t fdset, aosl_fd_t fd)
{
	if (!fdset) {
		return 0;
	}
	return FD_ISSET(fd, (fd_set *)fdset);
}

int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds, fd_set_t exceptfds, int timeout_ms)
{
	int err;
	struct timeval tv, *ptv;
	if (timeout_ms < 0) {
		ptv = NULL;
	} else {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
		ptv = &tv;
	}

	err = lwip_select(nfds, readfds ? (fd_set *)readfds : NULL, 
	                  writefds ? (fd_set *)writefds : NULL, 
	                  exceptfds ? (fd_set *)exceptfds : NULL, ptv);
	if (err < 0) {
		int orig_errno = errno;
		err = aosl_hal_errno_convert(orig_errno);
		if (err == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("select errno convert: %d -> %d", orig_errno, err);
		}
		return err;
	}

	return err;
}
