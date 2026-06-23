/***************************************************************************
 * Module:	OS dependent relative functionals implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <api/aosl_types.h>
#include <kernel/err.h>
#include <api/aosl_time.h>
#include <api/aosl_atomic.h>
#include <kernel/mp_queue.h>
#include <kernel/iofd.h>
#include <kernel/osmp.h>

#if   defined(AOSL_HAL_HAVE_EPOLL)  && AOSL_HAL_HAVE_EPOLL  == 1
extern int os_mp_init_epoll (struct mp_queue *q);
extern void os_mp_fini_epoll (struct mp_queue *q);
extern int os_activate_sigp_epoll (struct mp_queue *q);
extern int os_deactivate_sigp_epoll (struct mp_queue *q);
extern int os_add_event_fd_epoll (struct mp_queue *q, struct iofd *f);
extern int os_del_event_fd_epoll (struct mp_queue *q, struct iofd *f);
extern int os_mp_wait_epoll (struct mp_queue *q, aosl_poll_event_t *events, int maxevents, intptr_t timeo);
extern void os_mp_dispatch_epoll (struct mp_queue *q, aosl_poll_event_t *events, int events_count);

#define os_mp_init_pub          os_mp_init_epoll
#define os_mp_fini_pub          os_mp_fini_epoll
#define os_add_event_fd_pub     os_add_event_fd_epoll
#define os_del_event_fd_pub     os_del_event_fd_epoll
#define os_activate_sigp        os_activate_sigp_epoll
#define os_deactivate_sigp      os_deactivate_sigp_epoll
#define os_mp_wait              os_mp_wait_epoll
#define os_mp_dispatch          os_mp_dispatch_epoll
#elif defined(AOSL_HAL_HAVE_POLL)   && AOSL_HAL_HAVE_POLL   == 1
extern int os_mp_init_poll (struct mp_queue *q);
extern void os_mp_fini_poll (struct mp_queue *q);
extern int os_activate_sigp_poll (struct mp_queue *q);
extern int os_deactivate_sigp_poll (struct mp_queue *q);
extern int os_add_event_fd_poll (struct mp_queue *q, struct iofd *f);
extern int os_del_event_fd_poll (struct mp_queue *q, struct iofd *f);
extern int os_mp_wait_poll (struct mp_queue *q, aosl_poll_event_t *events, int maxevents, intptr_t timeo);
extern void os_mp_dispatch_poll (struct mp_queue *q, aosl_poll_event_t *events, int events_count);

#define os_mp_init_pub          os_mp_init_poll
#define os_mp_fini_pub          os_mp_fini_poll
#define os_add_event_fd_pub     os_add_event_fd_poll
#define os_del_event_fd_pub     os_del_event_fd_poll
#define os_activate_sigp        os_activate_sigp_poll
#define os_deactivate_sigp      os_deactivate_sigp_poll
#define os_mp_wait              os_mp_wait_poll
#define os_mp_dispatch          os_mp_dispatch_poll
#elif defined(AOSL_HAL_HAVE_SELECT) && AOSL_HAL_HAVE_SELECT == 1
extern int os_mp_init_select (struct mp_queue *q);
extern void os_mp_fini_select (struct mp_queue *q);
extern int os_activate_sigp_select (struct mp_queue *q);
extern int os_deactivate_sigp_select (struct mp_queue *q);
extern int os_add_event_fd_select (struct mp_queue *q, struct iofd *f);
extern int os_del_event_fd_select (struct mp_queue *q, struct iofd *f);
extern int os_mp_wait_select (struct mp_queue *q, aosl_poll_event_t *events, int maxevents, intptr_t timeo);
extern void os_mp_dispatch_select (struct mp_queue *q, aosl_poll_event_t *events, int events_count);

#define os_mp_init_pub          os_mp_init_select
#define os_mp_fini_pub          os_mp_fini_select
#define os_add_event_fd_pub     os_add_event_fd_select
#define os_del_event_fd_pub     os_del_event_fd_select
#define os_activate_sigp        os_activate_sigp_select
#define os_deactivate_sigp      os_deactivate_sigp_select
#define os_mp_wait              os_mp_wait_select
#define os_mp_dispatch          os_mp_dispatch_select
#else
#error "No iomp implementation in hal"
#endif

static __inline__ void __update_load_time (struct mp_queue *q)
{
	aosl_ts_t tick_now_us = aosl_tick_us ();
	q->last_load_us = tick_now_us - q->last_wake_ts;
	q->last_idle_ts = tick_now_us;
}

static __inline__ void __update_idle_time (struct mp_queue *q)
{
	aosl_ts_t tick_now_us = aosl_tick_us ();
	q->last_idle_us = tick_now_us - q->last_idle_ts;
	q->last_wake_ts = tick_now_us;
}

static __inline__ int __os_event_wait(struct mp_queue *q, intptr_t timeo)
{
	q->need_kicking = 1;
	__update_load_time (q);
	k_event_timedwait(q->sigp.event, timeo);
	__update_idle_time (q);
	q->need_kicking = 0;
	return 0;
}

static __inline__ int __os_iomp_wait(struct mp_queue *q, intptr_t timeo)
{
	int err = 0;
	aosl_poll_event_t events [64] = {0};

	q->need_kicking = 1;
	__update_load_time (q);
	err = os_mp_wait (q, events, sizeof events / sizeof events [0], timeo);
	__update_idle_time (q);
	q->need_kicking = 0;
	os_mp_dispatch (q, events, err);
	return err;
}

int os_poll_dispatch (struct mp_queue *q, intptr_t timeo)
{
	int err = 0;
	/**
	 * Enter idle state only when satisfy the following conditions:
	 * 1. not terminated
	 * 2. some thread kicked us OR we have iofds OR waiting time != 0
	 * Otherwise, no need to invoke the waiting function at all.
	 **/
	if (!q->terminated && (atomic_read (&q->kick_q_count) > 0 || q->iofd_count > 0 || timeo != 0)) {
		q->need_kicking = 1; /* Tell the world we need kicking */

		/**
		 * 1. Write memory barrier to make sure the writing op of
		 *    need_kicking is visible globally now;
		 * 2. Make sure the loading instruction of terminated is
		 *    after it was written;
		 **/
		aosl_mb ();

		if (q->terminated) {
			/**
			 * After set need_kicking to 1, if we were told to terminate
			 * now, then just return 0 here, do nothing else (no need to
			 * set the need_kicking back to 0 too).
			 **/
			aosl_msleep(5);
			return 0;
		}

		/**
		 * The checking code of the queued functions count must
		 * be put after we told the world we need kicking.
		 * If the queued functions count > 0, then do not wait
		 * via setting timeo to 0, and just check the fd event
		 * for this case.
		 **/
		if (atomic_read (&q->count) > 0) {
			/**
			 * Tell the world that no need to kick us at the
			 * the first time, because we will not sleep for
			 * these cases (either return 0 or timeo = 0).
			 **/
			q->need_kicking = 0;

			if (atomic_read (&q->kick_q_count) == 0 && q->iofd_count == 0) {
				/**
				 * If we have queued functions unprocessed, and
				 * nobody kicked us & we have no any fd, then
				 * just return 0 here, no need to do following
				 * other checkings.
				 **/
				aosl_msleep(5);
				return 0;
			}

			/**
			 * If the queued functions count > 0, then do not
			 * wait really, just check the kickings and fds.
			 **/
			timeo = 0;
		}

		if (q->q_flags & AOSL_MPQ_FLAG_SIGP_EVENT) {
			err = __os_event_wait(q, timeo);
		} else {
			err = __os_iomp_wait(q, timeo);
		}
	}

	return err;
}

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#else
#include <hal/aosl_hal_socket.h>
#include <api/aosl_log.h>
#include <kernel/net.h>
#define DEFAULT_SOCKET_PORT  12543
static int os_socket_pipe(aosl_fd_t pipefd[2])
{
	static int port = DEFAULT_SOCKET_PORT;
	port = ((port + 1) % 100) + DEFAULT_SOCKET_PORT;

	// server
	aosl_fd_t servfd = aosl_hal_sk_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, 0);
  if (aosl_fd_invalid(servfd)) {
    AOSL_LOG_ERR("socket error");
    return -1;
  }
	aosl_sockaddr_in_t servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AOSL_AF_INET;
	k_inet_pton(AOSL_AF_INET, "127.0.0.1", &servaddr.sin_addr.s_addr);
	servaddr.sin_port = aosl_htons(port);

	if (aosl_hal_sk_bind(servfd, (aosl_sockaddr_t*)&servaddr) < 0) {
		AOSL_LOG_ERR("bind error");
		aosl_hal_sk_close(servfd);
		return -1;
	}

	// client
	aosl_fd_t connfd = aosl_hal_sk_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, 0);
	if (aosl_fd_invalid(connfd)) {
		AOSL_LOG_ERR("socket error");
		return -1;
	}
	if (aosl_hal_sk_connect(connfd, (aosl_sockaddr_t*)&servaddr) < 0) {
			AOSL_LOG_ERR("connecting error");
			aosl_hal_sk_close(connfd);
			aosl_hal_sk_close(servfd);
			return -1;
	}

	pipefd[0] = servfd;
	pipefd[1] = connfd;
	return 0;
}
#endif

static int os_pipe(aosl_fd_t pipefd[2], int *type)
{
#if defined(__linux__) || defined(__APPLE__)
	*type = WAKEUP_TYPE_PIPE;
	return pipe (pipefd);
#else
	*type = WAKEUP_TYPE_SOCKET;
	return os_socket_pipe(pipefd);
#endif
}

static void os_fini_sigp (struct mp_queue *q)
{
	// for pipe
	if (q->sigp.activated) {
		os_deactivate_sigp (q);
		q->sigp.activated = 0;
	}
	if (!aosl_fd_invalid(q->sigp.piper)) {
		aosl_hal_sk_close (q->sigp.piper);
		q->sigp.piper = AOSL_INVALID_FD;
	}
	if (!aosl_fd_invalid(q->sigp.pipew)) {
		aosl_hal_sk_close (q->sigp.pipew);
		q->sigp.pipew = AOSL_INVALID_FD;
	}

	// for event
	if (q->sigp.event) {
		aosl_event_destroy(q->sigp.event);
		q->sigp.event = NULL;
	}
}

static int os_init_sigp_pipe (struct mp_queue *q)
{
	// init
	int err;
	aosl_fd_t fds [2];
	int type;

	err = os_pipe(fds, &type);
	if (err < 0) {
		goto __sigp_fini;
	}
	q->sigp.type = type;
	q->sigp.piper = fds [0];
	q->sigp.pipew = fds [1];

	err = make_fd_nb_clex (fds [0]);
	if (err < 0)
		goto __sigp_fini;
	err = make_fd_nb_clex (fds [1]);
	if (err < 0)
		goto __sigp_fini;

	err = os_activate_sigp (q);
	if (err < 0) {
		goto __sigp_fini;
	}

	q->sigp.activated = 1;

	return 0;

__sigp_fini:
	os_fini_sigp(q);
	return err;
}

static int os_init_sigp_event (struct mp_queue *q)
{
	q->sigp.event = aosl_event_create();
	if (q->sigp.event == NULL) {
		return -1;
	}
	return 0;
}

static int os_init_sigp (struct mp_queue *q)
{
	int ret = 0;

	if (!q) {
		return -1;
	}

	// init sigp to invalid
	q->sigp.type = WAKEUP_TYPE_NONE;
	q->sigp.piper = AOSL_INVALID_FD;
	q->sigp.pipew = AOSL_INVALID_FD;
	q->sigp.activated = 0;
	q->sigp.event = NULL;

	if (q->q_flags & AOSL_MPQ_FLAG_SIGP_EVENT) {
		ret = os_init_sigp_event (q);
	} else {
		ret = os_init_sigp_pipe (q);
	}

	return ret;
}

extern int os_mp_init (struct mp_queue *q)
{
	if (os_mp_init_pub(q) != 0) {
		return -1;
	}

	if (os_init_sigp(q) != 0) {
		os_mp_fini_pub(q);
		return -1;
	}

	return 0;
}

extern void os_mp_fini (struct mp_queue *q)
{
	os_fini_sigp(q);
	os_mp_fini_pub(q);
}

int os_add_event_fd (struct mp_queue *q, struct iofd *f)
{
	return os_add_event_fd_pub(q, f);
}

int os_del_event_fd (struct mp_queue *q, struct iofd *f)
{
	return os_del_event_fd_pub(q, f);
}
