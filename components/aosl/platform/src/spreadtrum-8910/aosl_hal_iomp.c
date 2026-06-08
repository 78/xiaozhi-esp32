#include <DAPS/export/inc/tcpip6/socket_types.h>
#include <DAPS/export/inc/tcpip6/socket_api.h>
#include <api/aosl_mm.h>

#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_iomp.h>


fd_set_t aosl_hal_fdset_create()
{
  sci_fd_set *fds = aosl_malloc(sizeof(sci_fd_set));
  return (fd_set_t)fds;
}

void aosl_hal_fdset_destroy(fd_set_t fdset)
{
  if (fdset) {
    aosl_free(fdset);
  }
}

void aosl_hal_fdset_zero(fd_set_t fdset)
{
  SCI_FD_ZERO((sci_fd_set *)fdset);
}

void aosl_hal_fdset_set(fd_set_t fdset, aosl_fd_t fd)
{
  SCI_FD_SET((TCPIP_SOCKET_T)fd, (sci_fd_set *)fdset);
}

void aosl_hal_fdset_clr(fd_set_t fdset, aosl_fd_t fd)
{
  SCI_FD_CLR((TCPIP_SOCKET_T)fd, (sci_fd_set *)fdset);
}

int aosl_hal_fdset_isset(fd_set_t fdset, aosl_fd_t fd)
{
  return SCI_FD_ISSET((TCPIP_SOCKET_T)fd, (sci_fd_set *)fdset);
}

int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds, fd_set_t exceptfds, int timeout)
{
  int err;
  long to = 0;
  if (timeout > 0) {
    to = timeout / 100;
    if (to == 0) {
      to = 1;
    }
  }

  err = sci_sock_select((sci_fd_set *)readfds, (sci_fd_set *)writefds, (sci_fd_set *)exceptfds, to);
  if (err < 0) {
    err = aosl_hal_errno_convert(err);
    return err;
  }

  return err;
}
