/***************************************************************************
 * Module:	poll MP relative functionals implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <hal/aosl_hal_iomp.h>
#include <hal/aosl_hal_errno.h>
#if defined(AOSL_HAL_HAVE_POLL) && AOSL_HAL_HAVE_POLL == 1


#include <api/aosl_types.h>
#include <kernel/err.h>
#include <api/aosl_time.h>
#include <api/aosl_alloca.h>
#include <kernel/mp_queue.h>


int os_mp_init_poll (struct mp_queue *q)
{
	return 0;
}

void os_mp_fini_poll (struct mp_queue *q)
{
}

int os_activate_sigp_poll (struct mp_queue *q)
{
	return 0;
}

int os_deactivate_sigp_poll (struct mp_queue *q)
{
	return 0;
}

int os_add_event_fd_poll (struct mp_queue *q, struct iofd *f)
{
	if (f->read_f != NULL)
		f->flags |= AOSL_POLLIN;

	if (f->write_f != NULL)
		f->flags |= AOSL_POLLOUT;

	return 0;
}

int os_del_event_fd_poll (struct mp_queue *q, struct iofd *f)
{
	return 0;
}

#define POLL_MAX_FDS 1024

int os_mp_wait_poll (struct mp_queue *q, aosl_poll_event_t *events, int maxevents, intptr_t timeo)
{
	int err;
	uint64_t time_stamp = 0;
	aosl_poll_event_t *fds;
	aosl_poll_event_t *pfd;
	size_t fds_count;
	struct iofd *f = NULL;

	if (q->iofd_count > POLL_MAX_FDS)
		return -AOSL_ENFILE;

	fds_count = q->iofd_count;
	fds_count++;
	fds = (aosl_poll_event_t *)aosl_alloca (sizeof (aosl_poll_event_t) * fds_count);

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

	pfd = fds;
	pfd->fd = q->sigp.piper;
	pfd->events = AOSL_POLLIN;
	pfd->revents = 0;
	pfd++;

	aosl_list_for_each_entry_t (struct iofd, f, &q->iofds, node) {
		pfd->fd = iofd_fobj (f)->fd;
		pfd->events = 0;
		pfd->revents = 0;

		if (f->flags & AOSL_POLLIN)
			pfd->events |= AOSL_POLLIN;

		if (f->flags & AOSL_POLLOUT)
			pfd->events |= AOSL_POLLOUT;

		pfd++;
	}

	err = aosl_hal_poll (fds, fds_count, timeo);
	if (err < 0 && (err == AOSL_HAL_RET_EINTR))
		goto __again;

	if (err > 0) {
		int i;
		for (i = 0; i < maxevents; i++) {
			events [i].fd = AOSL_INVALID_FD;
			events [i].events = 0;
		}

		pfd = fds;
		i = 0;

		BUG_ON (pfd->fd != q->sigp.piper);

		if (pfd->revents & AOSL_POLLIN) {
			events [i].fd = q->sigp.piper;
			events [i].events = AOSL_POLLIN;
			i++;
		}

		pfd++;

		aosl_list_for_each_entry_t (struct iofd, f, &q->iofds, node) {
			BUG_ON (pfd->fd != iofd_fobj (f)->fd);

			if (pfd->revents & AOSL_POLLIN) {
				f->flags &= ~AOSL_POLLIN;
				events [i].events |= AOSL_POLLIN;
			}

			if (pfd->revents & AOSL_POLLOUT) {
				f->flags &= ~AOSL_POLLOUT;
				events [i].events |= AOSL_POLLOUT;
			}

			if (events [i].events != 0) {
				events [i].fd = iofd_fobj (f)->fd;
				i++;
			}

			if (i >= err || i >= maxevents)
				break;

			pfd++;
		}

		return i;
	}

	return err;
}

void os_mp_dispatch_poll (struct mp_queue *q, aosl_poll_event_t *events, int events_count)
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
