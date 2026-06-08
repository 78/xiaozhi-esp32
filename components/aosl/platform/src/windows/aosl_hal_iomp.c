#include <winsock2.h>
#include <ws2tcpip.h>

#include <api/aosl_alloca.h>
#include <api/aosl_log.h>
#include <api/aosl_mm.h>
#include <hal/aosl_hal_iomp.h>

int aosl_hal_poll(aosl_poll_event_t fds[], int nfds, int timeout_ms) {
  int i;
  int err;
  WSAPOLLFD *pollfds;

  if (!fds || nfds <= 0) {
    return -1;
  }

  pollfds = (WSAPOLLFD *)aosl_alloca(sizeof(WSAPOLLFD) * (size_t)nfds);
  if (!pollfds) {
    return -1;
  }

  for (i = 0; i < nfds; ++i) {
    if (aosl_fd_invalid(fds[i].fd)) {
      pollfds[i].fd = INVALID_SOCKET;
    } else {
      pollfds[i].fd = (SOCKET)fds[i].fd;
    }
    pollfds[i].events = 0;
    pollfds[i].revents = 0;
    if (pollfds[i].fd == INVALID_SOCKET) {
      continue;
    }

    if (fds[i].events & AOSL_POLLIN) {
      pollfds[i].events |= POLLRDNORM;
    }
    if (fds[i].events & AOSL_POLLOUT) {
      pollfds[i].events |= POLLWRNORM;
    }
  }

  err = WSAPoll(pollfds, (ULONG)nfds, timeout_ms);
  if (err == SOCKET_ERROR) {
    int orig_errno = WSAGetLastError();
    err = aosl_hal_errno_convert(orig_errno);
    if (err == AOSL_HAL_RET_EHAL) {
      AOSL_LOG_ERR("WSAPoll errno convert: %d -> %d", orig_errno, err);
    }
    return err;
  }

  for (i = 0; i < nfds; ++i) {
    fds[i].revents = 0;
    if (pollfds[i].revents & (POLLRDNORM | POLLRDBAND | POLLIN)) {
      fds[i].revents |= AOSL_POLLIN;
    }
    if (pollfds[i].revents & (POLLWRNORM | POLLOUT)) {
      fds[i].revents |= AOSL_POLLOUT;
    }
    if (pollfds[i].revents & POLLERR) {
      fds[i].revents |= AOSL_POLLERR;
    }
    if (pollfds[i].revents & POLLHUP) {
      fds[i].revents |= AOSL_POLLHUP;
    }
  }

  return err;
}

fd_set_t aosl_hal_fdset_create(void) {
  fd_set *fds = (fd_set *)aosl_malloc(sizeof(fd_set));
  if (fds) {
    FD_ZERO(fds);
  }
  return (fd_set_t)fds;
}

void aosl_hal_fdset_destroy(fd_set_t fdset) {
  if (fdset) {
    aosl_free(fdset);
  }
}

void aosl_hal_fdset_zero(fd_set_t fdset) { FD_ZERO((fd_set *)fdset); }

void aosl_hal_fdset_set(fd_set_t fdset, aosl_fd_t fd) {
  if (!aosl_fd_invalid(fd)) {
    FD_SET((SOCKET)fd, (fd_set *)fdset);
  }
}

void aosl_hal_fdset_clr(fd_set_t fdset, aosl_fd_t fd) {
  if (!aosl_fd_invalid(fd)) {
    FD_CLR((SOCKET)fd, (fd_set *)fdset);
  }
}

int aosl_hal_fdset_isset(fd_set_t fdset, aosl_fd_t fd) {
  if (aosl_fd_invalid(fd)) {
    return 0;
  }
  return FD_ISSET((SOCKET)fd, (fd_set *)fdset);
}

int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds,
                    fd_set_t exceptfds, int timeout_ms) {
  int err;
  struct timeval tv;
  struct timeval *ptv = NULL;

  (void)nfds;

  if (timeout_ms >= 0) {
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ptv = &tv;
  }

  err =
    select(0, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)exceptfds, ptv);
  if (err == SOCKET_ERROR) {
    int orig_errno = WSAGetLastError();
    err = aosl_hal_errno_convert(orig_errno);
    if (err == AOSL_HAL_RET_EHAL) {
      AOSL_LOG_ERR("select errno convert: %d -> %d", orig_errno, err);
    }
    return err;
  }

  return err;
}
