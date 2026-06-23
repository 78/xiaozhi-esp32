/***************************************************************************
 * Module:	OS dependent relative functionals implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <hal/aosl_hal_iomp.h>
#if defined(AOSL_HAL_HAVE_EPOLL) && AOSL_HAL_HAVE_EPOLL == 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <api/aosl_types.h>
#include <kernel/compiler.h>
#include <kernel/err.h>
#include <api/aosl_time.h>
#include <hal/aosl_hal_errno.h>
#include <kernel/mp_queue.h>

int os_mp_init_epoll (struct mp_queue *q)
{
	q->efd = aosl_hal_epoll_create();
	if (aosl_fd_invalid (q->efd)) {
		q->efd = AOSL_INVALID_FD;
		return aosl_hal_set_error(AOSL_HAL_RET_EHAL);
	}

	return 0;
}

void os_mp_fini_epoll (struct mp_queue *q)
{
	aosl_hal_epoll_destroy(q->efd);
	q->efd = AOSL_INVALID_FD;
}

int os_activate_sigp_epoll (struct mp_queue *q)
{
	aosl_poll_event_t event;
	event.events = AOSL_POLLIN;
	event.fd = q->sigp.piper;
	int err = aosl_hal_epoll_ctl (q->efd, AOSL_POLL_CTL_ADD, q->sigp.piper, &event);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return 0;
}

int os_deactivate_sigp_epoll (struct mp_queue *q)
{
	int err = aosl_hal_epoll_ctl (q->efd, AOSL_POLL_CTL_DEL, q->sigp.piper, NULL);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return 0;
}

int os_add_event_fd_epoll (struct mp_queue *q, struct iofd *f)
{
	aosl_poll_event_t event = {0};
	int err;

	event.fd = iofd_fobj (f)->fd;
	event.events |= AOSL_POLLET;

	if (f->read_f != NULL)
		event.events |= AOSL_POLLIN;

	if (f->write_f != NULL)
		event.events |= AOSL_POLLOUT;

	err = aosl_hal_epoll_ctl (q->efd, AOSL_POLL_CTL_ADD, event.fd, &event);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}

	return err;
}

int os_del_event_fd_epoll (struct mp_queue *q, struct iofd *f)
{
	int err = aosl_hal_epoll_ctl (q->efd, AOSL_POLL_CTL_DEL, iofd_fobj (f)->fd, NULL);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}

	return err;
}

int os_mp_wait_epoll (struct mp_queue *q, aosl_poll_event_t *events, int maxevents, intptr_t timeo)
{
	int err;
	uint64_t time_stamp = 0;

	if (timeo > 0)
		time_stamp = aosl_tick_now ();

__again:
	if (timeo > 0) {
		uint64_t now = aosl_tick_now ();
		timeo -= (intptr_t)(now - time_stamp);
		time_stamp = now;
		if (timeo < 0)
			timeo = 0;
	}

	err = aosl_hal_epoll_wait (q->efd, events, maxevents, timeo);
	if (err < 0 && (err == AOSL_HAL_RET_EINTR)) {
		goto __again;
	}
	return err;
}

void os_mp_dispatch_epoll (struct mp_queue *q, aosl_poll_event_t *events, int events_count)
{
	int i;
	for (i = 0; i < events_count; i++) {
		struct iofd *f;
		aosl_poll_event_t *ev = &events [i];

		if (ev->fd == q->sigp.piper) {
			os_drain_sigp (q);
			continue;
		}

		f = iofd_get (ev->fd);
		if (f == NULL) {
			/**
			 * This is the case that we have gotten more than one events via a single
			 * syscall, and the subsequent io fd is closed by the callback function of
			 * a prior io fd. We should try our best to avoid these kinds of senario,
			 * but some program may do as this according to some special logic.
			 * So, just ignore these outdated events read by the prior syscall.
			 **/
			continue;
		}

		if (ev->events & AOSL_POLLERR) {
			/* Close the fd when error or hup */
			f_event_and_close (q, f, AOSL_IOFD_ERROR);
			goto __put_f;
		}

		if (ev->events & AOSL_POLLOUT) {
			if (__iofd_write_data (q, f) < 0)
				goto __put_f;
		}

		if (ev->events & AOSL_POLLIN) {
			if (__iofd_read_data (q, f) < 0)
				goto __put_f;
		}

		if (ev->events & AOSL_POLLHUP) {
			/* Close the fd when error or hup */
			f_event_and_close (q, f, AOSL_IOFD_HUP);
			goto __put_f;
		}

__put_f:
		iofd_put (f);
	}
}

#endif
