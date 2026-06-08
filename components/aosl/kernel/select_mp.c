/***************************************************************************
 * Module:	select MP relative functionals implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <hal/aosl_hal_iomp.h>
#if defined(AOSL_HAL_HAVE_SELECT) && AOSL_HAL_HAVE_SELECT == 1


#include <api/aosl_types.h>
#include <kernel/err.h>
#include <api/aosl_time.h>
#include <kernel/mp_queue.h>

#define UNUSED(expr) (void)(expr)

int os_mp_init_select (struct mp_queue *q)
{
	UNUSED (q);
	return 0;
}

void os_mp_fini_select (struct mp_queue *q)
{
	UNUSED (q);
}

int os_activate_sigp_select (struct mp_queue *q)
{
	UNUSED (q);
	return 0;
}

int os_deactivate_sigp_select (struct mp_queue *q)
{
	UNUSED (q);
	return 0;
}

int os_add_event_fd_select (struct mp_queue *q, struct iofd *f)
{
	UNUSED (q);

	if (f->read_f != NULL)
		f->flags |= AOSL_POLLIN;

	if (f->write_f != NULL)
		f->flags |= AOSL_POLLOUT;

	return 0;
}

int os_del_event_fd_select (struct mp_queue *q, struct iofd *f)
{
	UNUSED (q);
	UNUSED (f);

	return 0;
}

int os_mp_wait_select (struct mp_queue *q, aosl_poll_event_t *events, int maxevents, intptr_t timeo)
{
	int err;
	uint64_t time_stamp = 0;
	struct iofd *f = NULL;
	int maxfd;
	fd_set_t readfds = aosl_hal_fdset_create();
	fd_set_t writefds = aosl_hal_fdset_create();

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

	aosl_hal_fdset_zero (readfds);
	aosl_hal_fdset_zero (writefds);
	aosl_hal_fdset_set (readfds, q->sigp.piper);
	maxfd = q->sigp.piper;

	aosl_list_for_each_entry_t (struct iofd, f, &q->iofds, node) {
		if (f->flags & AOSL_POLLIN)
			aosl_hal_fdset_set (readfds, iofd_fobj (f)->fd);

		if (f->flags & AOSL_POLLOUT)
			aosl_hal_fdset_set (writefds, iofd_fobj (f)->fd);

		if (iofd_fobj (f)->fd > maxfd)
			maxfd = iofd_fobj (f)->fd;
	}

	err = aosl_hal_select (maxfd + 1, readfds, writefds, NULL, timeo);
	if (err < 0 && (err == AOSL_HAL_RET_EINTR))
		goto __again;

	if (err > 0) {
		int i;
		for (i = 0; i < maxevents; i++) {
			events [i].fd = AOSL_INVALID_FD;
			events [i].events = 0;
		}

		i = 0;
		if (aosl_hal_fdset_isset (readfds, q->sigp.piper)) {
			events [i].fd = q->sigp.piper;
			events [i].events = AOSL_POLLIN;
			i++;
		}

		aosl_list_for_each_entry_t (struct iofd, f, &q->iofds, node) {
			if (aosl_hal_fdset_isset (readfds, iofd_fobj (f)->fd)) {
				f->flags &= ~AOSL_POLLIN;
				events [i].events |= AOSL_POLLIN;
			}

			if (aosl_hal_fdset_isset (writefds, iofd_fobj (f)->fd)) {
				f->flags &= ~AOSL_POLLOUT;
				events [i].events |= AOSL_POLLOUT;
			}

			if (events [i].events != 0) {
				events [i].fd = iofd_fobj (f)->fd;
				i++;
			}

			if (i >= err || i >= maxevents)
				break;
		}

		aosl_hal_fdset_destroy (readfds);
		aosl_hal_fdset_destroy (writefds);
		return i;
	}

	aosl_hal_fdset_destroy (readfds);
	aosl_hal_fdset_destroy (writefds);
	return err;
}

void os_mp_dispatch_select (struct mp_queue *q, aosl_poll_event_t *events, int events_count)
{
	int i;
	for (i = 0; i < events_count; i++) {
		struct iofd *f;
		int event_fd;

		event_fd = events [i].fd;
		if (event_fd == q->sigp.piper) {
			os_drain_sigp (q);
			continue;
		}

		f = iofd_get (event_fd);
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

		if (events [i].events & AOSL_POLLOUT) {
			if (__iofd_write_data (q, f) < 0)
				goto __put_f;
		}

		if (events [i].events & AOSL_POLLIN) {
			if (__iofd_read_data (q, f) < 0)
				goto __put_f;
		}

__put_f:
		iofd_put (f);
	}
}

#endif
