#include <sockets.h>

#include <api/aosl_mm.h>
#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_iomp.h>

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

void aosl_hal_fdset_set(fd_set_t fdset, int fd)
{
  FD_SET(fd, (fd_set *)fdset);
}

void aosl_hal_fdset_clr(fd_set_t fdset, int fd)
{
  FD_CLR(fd, (fd_set *)fdset);
}

int aosl_hal_fdset_isset(fd_set_t fdset, int fd)
{
  return FD_ISSET(fd, (fd_set *)fdset);
}

int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds, fd_set_t exceptfds, int timeout)
{
  int err;
  struct timeval tv, *ptv;
  if (timeout < 0) {
    ptv = NULL;
  } else {
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    ptv = &tv;
  }

  err = select(nfds, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)exceptfds, ptv);
  if (err < 0) {
    err = aosl_hal_errno_convert(errno);
    return err;
  }

  return err;
}
