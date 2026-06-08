#include <fcntl.h>
#include <unistd.h>
#include <lwip/sockets.h>

#include <hal/aosl_hal_iomp.h>
#include <api/aosl_mm.h>

fd_set_t aosl_hal_fdset_create()
{
	fd_set *fds = aosl_malloc(sizeof(fd_set));
	return fds;
}

void aosl_hal_fdset_destroy(fd_set_t fdset)
{
	if (fdset) {
		aosl_free(fdset);
	}
}

void aosl_hal_fdset_zero(fd_set_t fdset)
{
	FD_ZERO((fd_set *)fdset);
}

void aosl_hal_fdset_set(fd_set_t fdset, aosl_fd_t fd)
{
	FD_SET(fd, (fd_set *)fdset);
}

void aosl_hal_fdset_clr(fd_set_t fdset, aosl_fd_t fd)
{
	FD_CLR(fd, (fd_set *)fdset);
}

int aosl_hal_fdset_isset(fd_set_t fdset, aosl_fd_t fd)
{
	return FD_ISSET(fd, (fd_set *)fdset);
}

int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds,
                    fd_set_t exceptfds, intptr_t timeout)
{
  struct timeval tv;
  struct timeval *tv_ptr = NULL;

  if (timeout >= 0) {
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    tv_ptr = &tv;
  }

  return select(nfds, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)exceptfds, tv_ptr);
}

int aosl_hal_set_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return -1;
  }

  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int aosl_hal_unset_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return -1;
  }

  return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}
