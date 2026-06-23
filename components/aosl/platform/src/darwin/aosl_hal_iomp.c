/*
 * Darwin/iOS HAL I/O multiplexing
 * Uses kqueue instead of Linux epoll.
 * poll() and select() are POSIX-compatible and work the same as Linux.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/errno.h>
#include <hal/aosl_hal_iomp.h>
#include <api/aosl_log.h>
#include <api/aosl_mm.h>
#include <api/aosl_alloca.h>

int aosl_hal_epoll_create()
{
	return kqueue();
}

int aosl_hal_epoll_destroy(int kqfd)
{
	return close(kqfd);
}

int aosl_hal_epoll_ctl(int kqfd, aosl_epoll_op_e op, aosl_fd_t fd, aosl_poll_event_t *ev)
{
	struct kevent changes[2];
	int nchanges = 0;

	if (op != AOSL_POLL_CTL_DEL && ev == NULL) {
		AOSL_LOG_ERR("invalid ev");
		return -1;
	}

	switch (op) {
	case AOSL_POLL_CTL_ADD: {
		if (ev->events & AOSL_POLLIN) {
			EV_SET(&changes[nchanges], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
			if (ev->events & AOSL_POLLET)
				changes[nchanges].flags |= EV_CLEAR;
			nchanges++;
		}
		if (ev->events & AOSL_POLLOUT) {
			EV_SET(&changes[nchanges], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
			if (ev->events & AOSL_POLLET)
				changes[nchanges].flags |= EV_CLEAR;
			nchanges++;
		}
		break;
	}
	case AOSL_POLL_CTL_MOD: {
		/* Delete existing filters first, then re-add */
		struct kevent del_evts[2];
		EV_SET(&del_evts[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		EV_SET(&del_evts[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		/* Ignore errors from delete (filter may not exist) */
		kevent(kqfd, del_evts, 2, NULL, 0, &(struct timespec){0, 0});

		if (ev->events & AOSL_POLLIN) {
			EV_SET(&changes[nchanges], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
			if (ev->events & AOSL_POLLET)
				changes[nchanges].flags |= EV_CLEAR;
			nchanges++;
		}
		if (ev->events & AOSL_POLLOUT) {
			EV_SET(&changes[nchanges], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
			if (ev->events & AOSL_POLLET)
				changes[nchanges].flags |= EV_CLEAR;
			nchanges++;
		}
		break;
	}
	case AOSL_POLL_CTL_DEL: {
		EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		nchanges = 2;
		break;
	}
	default:
		AOSL_LOG_ERR("invalid op %d", op);
		return -1;
	}

	if (nchanges > 0) {
		int ret = kevent(kqfd, changes, nchanges, NULL, 0, NULL);
		if (ret < 0 && op != AOSL_POLL_CTL_DEL) {
			return -1;
		}
	}
	return 0;
}

int aosl_hal_epoll_wait(int kqfd, aosl_poll_event_t *evlist, int maxevents, int timeout_ms)
{
	struct kevent *kevents = aosl_alloca(maxevents * sizeof(struct kevent));
	if (!kevents) return -1;

	struct timespec ts, *pts = NULL;
	if (timeout_ms >= 0) {
		ts.tv_sec = timeout_ms / 1000;
		ts.tv_nsec = (timeout_ms % 1000) * 1000000;
		pts = &ts;
	}

	int nev = kevent(kqfd, NULL, 0, kevents, maxevents, pts);
	if (nev < 0) {
		int orig_errno = errno;
		int err = aosl_hal_errno_convert(orig_errno);
		if (err == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("kevent errno convert: %d -> %d", orig_errno, err);
		}
		return err;
	}

	for (int i = 0; i < nev; i++) {
		evlist[i].fd = (int)kevents[i].ident;
		evlist[i].events = 0;
		if (kevents[i].filter == EVFILT_READ)
			evlist[i].events |= AOSL_POLLIN;
		if (kevents[i].filter == EVFILT_WRITE)
			evlist[i].events |= AOSL_POLLOUT;
		if (kevents[i].flags & EV_ERROR)
			evlist[i].events |= AOSL_POLLERR;
		if (kevents[i].flags & EV_EOF)
			evlist[i].events |= AOSL_POLLHUP;
	}

	return nev;
}

int aosl_hal_poll(aosl_poll_event_t fds[], int nfds, int timeout_ms)
{
	if (!fds || !nfds) return -1;

	struct pollfd *n_fds = aosl_alloca(nfds * sizeof(struct pollfd));

	for (int i = 0; i < nfds; i++) {
		n_fds[i].fd = fds[i].fd;
		n_fds[i].revents = 0;
		n_fds[i].events = 0;
		if (fds[i].events & AOSL_POLLIN)  n_fds[i].events |= POLLIN;
		if (fds[i].events & AOSL_POLLOUT) n_fds[i].events |= POLLOUT;
	}

	int err = poll(n_fds, nfds, timeout_ms);
	if (err < 0) {
		int orig_errno = errno;
		err = aosl_hal_errno_convert(orig_errno);
		if (err == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("poll errno convert: %d -> %d", orig_errno, err);
		}
		return err;
	}

	for (int i = 0; i < nfds; i++) {
		if (n_fds[i].revents & POLLIN)  fds[i].revents |= AOSL_POLLIN;
		if (n_fds[i].revents & POLLOUT) fds[i].revents |= AOSL_POLLOUT;
	}
	return err;
}

fd_set_t aosl_hal_fdset_create()       { fd_set *fds = aosl_malloc(sizeof(fd_set)); return fds; }
void aosl_hal_fdset_destroy(fd_set_t s){ if (s) aosl_free(s); }
void aosl_hal_fdset_zero(fd_set_t s)   { FD_ZERO((fd_set *)s); }
void aosl_hal_fdset_set(fd_set_t s, aosl_fd_t fd)  { FD_SET(fd, (fd_set *)s); }
void aosl_hal_fdset_clr(fd_set_t s, aosl_fd_t fd)  { FD_CLR(fd, (fd_set *)s); }
int  aosl_hal_fdset_isset(fd_set_t s, aosl_fd_t fd){ return FD_ISSET(fd, (fd_set *)s); }

int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds, fd_set_t exceptfds, int timeout_ms)
{
	struct timeval tv, *ptv;
	if (timeout_ms < 0) {
		ptv = NULL;
	} else {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
		ptv = &tv;
	}

	int err = select(nfds, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)exceptfds, ptv);
	if (err < 0) {
		int orig_errno = errno;
		err = aosl_hal_errno_convert(orig_errno);
		if (err == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("select errno convert: %d -> %d", orig_errno, err);
		}
	}
	return err;
}
