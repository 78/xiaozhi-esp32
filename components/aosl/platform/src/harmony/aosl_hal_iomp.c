#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/select.h>
#include <errno.h>
#include <hal/aosl_hal_iomp.h>
#include <api/aosl_log.h>
#include <api/aosl_mm.h>
#include <api/aosl_alloca.h>


int aosl_hal_epoll_create()
{
	return epoll_create(1);
}

int aosl_hal_epoll_destroy(int epfd)
{
	return close(epfd);
}

int aosl_hal_epoll_ctl(int epfd, aosl_epoll_op_e op, aosl_fd_t fd, aosl_poll_event_t *ev)
{
	int n_op = 0;
	struct epoll_event n_event = {0};

	if (op != AOSL_POLL_CTL_DEL && ev == NULL) {
		AOSL_LOG_ERR("invalid ev");
		return -1;
	}

	switch (op) {
		case AOSL_POLL_CTL_ADD:
			n_op = EPOLL_CTL_ADD;
			break;
		case AOSL_POLL_CTL_MOD:
			n_op = EPOLL_CTL_MOD;
			break;
		case AOSL_POLL_CTL_DEL:
			n_op = EPOLL_CTL_DEL;
			break;
		default:
			AOSL_LOG_ERR("invalid op %d", op);
			return -1;
	}

	if (ev) {
		// set events
		if (ev->events & AOSL_POLLIN)
			n_event.events |= EPOLLIN;
		if (ev->events & AOSL_POLLOUT)
			n_event.events |= EPOLLOUT;
		if (ev->events & AOSL_POLLERR)
			n_event.events |= EPOLLERR;
		if (ev->events & AOSL_POLLHUP)
			n_event.events |= EPOLLHUP;
		if (ev->events & AOSL_POLLET)
			n_event.events |= EPOLLET;

		// set data
		n_event.data.fd = fd;
	}

	return epoll_ctl (epfd, n_op, fd, &n_event);
}

int aosl_hal_epoll_wait(int epfd, aosl_poll_event_t *evlist, int maxevents, int timeout_ms)
{
	int err = 0;
	struct epoll_event *n_events = aosl_alloca(maxevents * sizeof(struct epoll_event));
	if (!n_events) {
		return -1;
	}

	err = epoll_wait(epfd, n_events, maxevents, timeout_ms);
	if (err < 0) {
		int orig_errno = errno;
		err = aosl_hal_errno_convert(orig_errno);
		if (err == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("epoll_wait errno convert: %d -> %d", orig_errno, err);
		}
		goto __tag_out;
	}

	for (int i = 0; i < err; i++) {
		evlist[i].fd = n_events[i].data.fd;
		if (n_events[i].events & EPOLLIN)
			evlist[i].events |= AOSL_POLLIN;
		if (n_events[i].events & EPOLLOUT)
			evlist[i].events |= AOSL_POLLOUT;
		if (n_events[i].events & EPOLLERR)
			evlist[i].events |= AOSL_POLLERR;
		if (n_events[i].events & EPOLLHUP)
			evlist[i].events |= AOSL_POLLHUP;
	}

__tag_out:
	return err;
}

int aosl_hal_poll(aosl_poll_event_t fds[], int nfds, int timeout_ms)
{
	if (!fds || !nfds) {
		return -1;
	}

	int err = 0;
	struct pollfd *n_fds = aosl_alloca(nfds * sizeof(struct pollfd));

	for (int i = 0; i < nfds; i++) {
		n_fds[i].fd = fds[i].fd;
		n_fds[i].revents = 0;
		if (fds[i].events & AOSL_POLLIN)
			n_fds[i].events |= POLLIN;
		if (fds[i].events & AOSL_POLLOUT)
			n_fds[i].events |= POLLOUT;
	}

	err = poll(n_fds, nfds, timeout_ms);
	if (err < 0) {
		int orig_errno = errno;
		err = aosl_hal_errno_convert(orig_errno);
		if (err == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("poll errno convert: %d -> %d", orig_errno, err);
		}
		return err;
	}

	for (int i = 0; i < nfds; i++) {
		if ((n_fds[i].revents & POLLIN)) {
			fds[i].revents |= AOSL_POLLIN;
		}
		if ((n_fds[i].revents & POLLOUT)) {
			fds[i].revents |= AOSL_POLLOUT;
		}
	}

	return err;
}

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

	err = select(nfds, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)exceptfds, ptv);
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
