/***************************************************************************
 * Module:	Multiplex queue iofd implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <api/aosl_alloca.h>
#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_mm.h>
#include <api/aosl_mpq_net.h>
#include <kernel/kernel.h>
#include <kernel/err.h>
#include <kernel/iofd.h>
#include <kernel/mp_queue.h>
#include <hal/aosl_hal_socket.h>

#define UNUSED(expr) (void)(expr)

void iofd_init (void)
{
	if (!AOSL_IS_ALIGNED_PTR (sizeof (struct iofd)) || !AOSL_IS_ALIGNED_PTR (sizeof (w_buffer_t)))
		abort ();
}

void iofd_fini (void)
{
}

static void iofd_destructor (void *obj)
{
	struct iofd *f = (struct iofd *)obj;
	w_buffer_t *node;
	while ((node = w_queue_remove_head (&f->w_q)) != NULL)
		aosl_free (node);
}

int make_fd_nb_clex (aosl_fd_t fd)
{
	int err = aosl_hal_sk_set_nonblock (fd);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}

	return 0;
}

void mpq_init_iofds (struct mp_queue *q)
{
	aosl_list_head_init (&q->iofds);
	q->iofd_count = 0;
}


static struct aosl_list_head __free_iofds = AOSL_LIST_HEAD_INIT (__free_iofds);

static int __q_add_fd (struct mp_queue *q, struct iofd *f)
{
	int err = os_add_event_fd (q, f);
	if (err < 0)
		return err;

	aosl_list_add_tail (&f->node, &q->iofds);
	q->iofd_count++;
	return 0;
}

int __iofd_read_data (struct mp_queue *q, struct iofd *f)
{
	size_t buff_size = (f->chk_pkt_f != NULL) ? (f->max_pkt_size * 2) : f->max_pkt_size;
	void *extra_bytes = (f->r_extra_size > 0) ? (char *)f->r_head + buff_size : NULL;

	for (;;) {
		isize_t err;

		for (;;) {
			isize_t data_len;

			err = (char *)f->r_tail - (char *)f->r_data;
			if (f->chk_pkt_f != NULL && err > 0) {
				err = f->chk_pkt_f (f->r_data, (char *)f->r_tail - (char *)f->r_data, f->argc, f->argv);
				if (aosl_fd_invalid (iofd_fobj (f)->fd)) {
					/**
					 * If the io fd has been closed in the callback function provided by user
					 * due to some logic, then just give up here, no further processing needed.
					 **/
					goto ____out;
				}

				if (err < 0 || err > ((char *)f->r_tail - (char *)f->r_data)) {
					f_event_and_close (q, f, (err < 0) ? err : -AOSL_EINVAL);

					if (err > 0)
						err = -AOSL_EINVAL;

					return (int)err;
				}
			}

			/**
			 * We consider the 'read' function returns 0 bytes is a special case:
			 * 1. for a bytes stream based IO fd(whose chk_pkt_f is non-NULL), this
			 *    indicates a session finish, so report this to the application;
			 * 2. for a packet based IO fd(whose chk_pkt_f is NULL), this indicates
			 *    we just received a packet with zero-length, so report this to
			 *    the application just as normal, and then continue the 'read'
			 *    operations;
			 **/
			if (err > 0 || (f->flags & IOFD_READ_RETURN_0)) {
				if (f->post_f != NULL) {
					data_len = f->post_f (f->r_data, err, f->argc, f->argv);
					if (aosl_fd_invalid (iofd_fobj (f)->fd)) {
						/**
						 * If the io fd has been closed in the callback function provided by user
						 * due to some logic, then just give up here, no further processing needed.
						 **/
						goto ____out;
					}
				} else {
					data_len = err;
				}

				if (data_len >= 0) {
					f->data_f (f->r_data, data_len, f->argc, f->argv, extra_bytes);
					mpq_stack_fini (q->q_stack_curr);
					if (aosl_fd_invalid (iofd_fobj (f)->fd)) {
						/**
						 * If the io fd has been closed in the callback function provided by user
						 * due to some logic, then just give up here, no further processing needed.
						 **/
						goto ____out;
					}
				}

				f->r_data = (char *)f->r_data + err;
			}

			if (f->flags & IOFD_READ_RETURN_0) {
				if (f->chk_pkt_f != NULL) {
					/**
					 * If 'read' function returns 0 bytes, and this is a stream
					 * based IO fd(whose chk_pkt_f is non-NULL), then there's no
					 * need to do more 'read' operation again, so just return.
					 **/
					return 0;
				}

				/**
				 * If 'read' function returns 0 bytes, and this is not a stream
				 * based IO fd(whose chk_pkt_f is NULL), then just indicates we
				 * received a 0 length packet, so clear the flags, and break out
				 * to do more 'read' operations, until got a -AOSL_EAGAIN.
				 **/
				f->flags &= ~IOFD_READ_RETURN_0;
				break;
			}

			if (err == 0) {
				/**
				 * There are 2 cases run to here:
				 * 1. common no data case;
				 * 2. partial data of a packet for stream based IO case;
				 **/
				break;
			}
		}

		if (__iofd_better_move_buffer (f)) {
			size_t len = (char *)f->r_tail - (char *)f->r_data;
			if (len > 0)
				memmove (f->r_head, f->r_data, len);

			f->r_data = f->r_head;
			f->r_tail = (char *)f->r_data + len;
		}

		err = f->read_f (iofd_fobj (f)->fd, f->r_tail, buff_size - ((char *)f->r_tail - (char *)f->r_head), f->r_extra_size, f->argc, f->argv);
		f->flags |= AOSL_POLLIN;
		if (err < 0) {
			if (err != -AOSL_EAGAIN) {
				f_event_and_close (q, f, err);
				return err;
			}
			break;
		}
		if (err == 0) {
			f->flags |= IOFD_READ_RETURN_0;
		} else {
			f->r_tail = (char *)f->r_tail + err;
		}
	}

____out:
	return 0;
}

void w_queue_init (w_queue_t *q)
{
	q->head = NULL;
	q->tail = NULL;
	q->count = 0;
	q->total_len = 0;
}

int __iofd_write_data (struct mp_queue *q, struct iofd *f)
{
	if (f->flags & IOFD_NOT_READY) {
		f->flags &= ~IOFD_NOT_READY;
		if (!aosl_mpq_timer_invalid (f->timer)) {
			aosl_mpq_kill_timer (f->timer);
			f->timer = AOSL_MPQ_TIMER_INVALID;
		}
	}

	while (f->w_q.head != NULL) {
		w_buffer_t *node = f->w_q.head;
		isize_t err;
		err = f->write_f (iofd_fobj (f)->fd, node->w_data, (char *)node->w_tail - (char *)node->w_data, node->w_extra_size, f->argc, f->argv);
		f->flags |= AOSL_POLLOUT;
		if (err < 0) {
			if (err != -AOSL_EAGAIN) {
				f_event_and_close (q, f, err);
				return err;
			}
			return 0;
		}
		node->w_data = (char *)node->w_data + err;
		if (node->w_data < node->w_tail)
			return 0;

		w_queue_remove_head (&f->w_q);
		aosl_free (node);
	}

	if (f->event_f != NULL) {
		f->event_f (iofd_fobj (f)->fd, AOSL_IOFD_READY_FOR_WRITING, f->argc, f->argv);
		mpq_stack_fini (q->q_stack_curr);
	}

	return 0;
}

static isize_t __default_read (aosl_fd_t fd, void *buf, size_t len, size_t extra, uintptr_t argc, uintptr_t argv [])
{
	isize_t err;
	UNUSED (extra);
	UNUSED (argc);
	UNUSED (argv);
	err = aosl_hal_sk_read (fd, buf, len);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return err;
}

static isize_t __default_write (aosl_fd_t fd, const void *buf, size_t len, size_t extra, uintptr_t argc, uintptr_t argv [])
{
	isize_t err;
	UNUSED (extra);
	UNUSED (argc);
	UNUSED (argv);
	err = aosl_hal_sk_write (fd, buf, len);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return err;
}

static void __iofd_not_ready_timeout (aosl_timer_t timer, const aosl_ts_t *now_p, uintptr_t argc, uintptr_t argv [])
{
	struct iofd *f = (struct iofd *)argv [0];
	UNUSED (timer);
	UNUSED (now_p);
	UNUSED (argc);

	__iofd_get (f);
	if (f->flags & IOFD_NOT_READY)
		f_event_and_close (THIS_MPQ (), f, -AOSL_ETIMEDOUT);

	if (!aosl_mpq_timer_invalid (f->timer)) {
		aosl_mpq_kill_timer (f->timer);
		f->timer = AOSL_MPQ_TIMER_INVALID;
	}

	iofd_put (f);
}

int __mpq_add_fd_argv (struct mp_queue *q, aosl_fd_t fd, int not_ready_timeo, size_t max_pkt_size, size_t extra_bytes,
						uint32_t flags, aosl_fd_read_t read_f, aosl_fd_write_t write_f, aosl_check_packet_t chk_pkt_f,
	iofd_post_process_t post_f, aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, uintptr_t argv [], ...)
{
	struct iofd *f;
	size_t argv_size;
	uintptr_t l;
	size_t buff_size;
	int err;

	if (q->q_flags & AOSL_MPQ_FLAG_SIGP_EVENT)
		return -AOSL_EPERM;

	if ((chk_pkt_f != NULL && max_pkt_size < FD_MAX_PACKET_SIZE_MIN) || max_pkt_size > FD_MAX_PACKET_SIZE_MAX)
		return -AOSL_EINVAL;

	if (extra_bytes >= FD_MAX_EXTRA_BYTES)
		return -AOSL_EINVAL;

	if (data_f == NULL)
		return -AOSL_EINVAL;

	if (fd != AOSL_INVALID_FD) {
		f = iofd_get (fd);
		if (f != NULL) {
			iofd_put (f);
			return -AOSL_EEXIST;
		}
	}

	max_pkt_size = AOSL_I_ALIGN_PTR (max_pkt_size);
	argv_size = argc * sizeof (uintptr_t);
	buff_size = (chk_pkt_f != NULL) ? (max_pkt_size * 2) : max_pkt_size;
	f = aosl_malloc (sizeof (struct iofd) + argv_size + buff_size + extra_bytes);
	if (f == NULL)
		return -AOSL_ENOMEM;

	if (read_f == AOSL_DEFAULT_READ_FN)
		read_f = __default_read;

	if (write_f == AOSL_DEFAULT_WRITE_FN)
		write_f = __default_write;

	atomic_set (&iofd_fobj (f)->usage, 1);
	iofd_fobj (f)->mpq_fd = 1;
	iofd_fobj (f)->dtor = iofd_destructor;
	f->flags = flags;
	f->q = q->qid;
	f->max_pkt_size = max_pkt_size;
	f->r_head = (char *)(f + 1) + argv_size;
	f->r_data = f->r_head;
	f->r_tail = f->r_data;
	f->r_extra_size = extra_bytes;

	w_queue_init (&f->w_q);

	f->read_f = read_f;
	f->write_f = write_f;
	f->chk_pkt_f = chk_pkt_f;
	f->post_f = post_f;
	f->put_fd_f = NULL;
	f->data_f = (iofd_data_t)(void *)data_f;
	f->event_f = event_f;
	f->argc = argc;
	for (l = 0; l < argc; l++)
		f->argv [l] = argv [l];

	f->timer = AOSL_MPQ_TIMER_INVALID;

	if (fd == AOSL_INVALID_FD) {
		va_list args;
		iofd_get_fd_t get_fd_f;
		va_start (args, argv);
		get_fd_f = va_arg (args, iofd_get_fd_t);
		f->put_fd_f = va_arg (args, iofd_put_fd_t);
		va_end (args);

		err = get_fd_f (&fd, f);
		if (err < 0 || fd == AOSL_INVALID_FD) {
			aosl_free (f);
			return err;
		}
	}

	iofd_fobj (f)->fd = fd;

#if 0
	/**
	 * ioctl (fd, FIOCLEX, 0) will fail for AF_NETLINK socket
	 * on some Android devices, so do not return error here.
	 **/
	err = make_fd_nb_clex (fd);
	if (err < 0)
		return err;
#else
	make_fd_nb_clex (fd);
#endif

	err = install_fd (fd, iofd_fobj (f));
	if (err < 0) {
		aosl_free (f);
		return err;
	}

	err = __q_add_fd (q, f);
	if (err < 0) {
		remove_fd (iofd_fobj (f));
		aosl_free (f);
		return err;
	}

	if ((f->flags & IOFD_NOT_READY) && not_ready_timeo >= 0)
		f->timer = aosl_mpq_set_oneshot_timer (aosl_tick_now () + not_ready_timeo, __iofd_not_ready_timeout, NULL, 1, f);

	return 0;
}

__export_in_so__ int aosl_mpq_add_fd (aosl_fd_t fd, size_t max_pkt_size, aosl_fd_read_t read_f,
										aosl_fd_write_t write_f, aosl_check_packet_t chk_pkt_f,
							aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t l;
	uintptr_t *argv;

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	q = __get_or_create_current ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);
	va_end (args);

	return_err (__mpq_add_fd_argv (q, fd, -1, max_pkt_size, 0, 0, read_f, write_f, chk_pkt_f, NULL, data_f, event_f, argc, argv));
}

static void ____target_q_add_fd (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	int *err_p = (int *)argv [0];
	aosl_fd_t fd = (aosl_fd_t)argv [1];
	size_t max_pkt_size = (size_t)argv [2];
	aosl_fd_read_t read_f = (aosl_fd_read_t)argv [3];
	aosl_fd_write_t write_f = (aosl_fd_write_t)argv [4];
	aosl_check_packet_t chk_pkt_f = (aosl_check_packet_t)argv [5];
	aosl_fd_data_t data_f = (aosl_fd_data_t)argv [6];
	aosl_fd_event_t event_f = (aosl_fd_event_t)argv [7];

	UNUSED (queued_ts_p);
	UNUSED (robj);

	*err_p = __mpq_add_fd_argv (THIS_MPQ (), fd, -1, max_pkt_size, 0, 0, read_f, write_f, chk_pkt_f, NULL, data_f, event_f, argc - 8, &argv [8]);
}

__export_in_so__ int aosl_mpq_add_fd_on_q (aosl_mpq_t qid, aosl_fd_t fd, size_t max_pkt_size,
				aosl_fd_read_t read_f, aosl_fd_write_t write_f, aosl_check_packet_t chk_pkt_f,
						aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t l;
	uintptr_t *argv;
	int err;

	if (argc > MPQ_ARGC_MAX)
		return_err (-AOSL_E2BIG);

	q = __mpq_get (qid);
	if (q == NULL)
		return_err (-AOSL_EINVAL);

	argv = aosl_alloca (sizeof (uintptr_t) * (8 + argc));
	argv [0] = (uintptr_t)&err;
	argv [1] = (uintptr_t)fd;
	argv [2] = (uintptr_t)max_pkt_size;
	argv [3] = (uintptr_t)read_f;
	argv [4] = (uintptr_t)write_f;
	argv [5] = (uintptr_t)chk_pkt_f;
	argv [6] = (uintptr_t)data_f;
	argv [7] = (uintptr_t)event_f;
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [8 + l] = va_arg (args, uintptr_t);
	va_end (args);

	if (__mpq_call_argv (q, -1, "____target_q_add_fd", ____target_q_add_fd, 8 + argc, argv) < 0)
		err = aosl_errno;

	__mpq_put (q);

	return_err (err);
}

static int __q_del_f (struct mp_queue *q, struct iofd *f)
{
	int err;

	if (f->q != q->qid)
		return -AOSL_EPERM;

	if (!aosl_mpq_timer_invalid (f->timer)) {
		aosl_mpq_kill_timer (f->timer);
		/**
		 * We set the timer to NULL just because the iofd object
		 * might not be freed directly here due to the usage.
		 **/
		f->timer = AOSL_MPQ_TIMER_INVALID;
	}

	err = os_del_event_fd (q, f);
	if (err < 0) {
		AOSL_LOG(AOSL_LOG_WARNING, "os_del_event_fd failed err=%d fd=%d", err, f->fobj.fd);
	}

	err = remove_fd (iofd_fobj (f));
	if (err < 0) {
		AOSL_LOG(AOSL_LOG_ERROR, "remove_fd failed err=%d, fd=%d", err, f->fobj.fd);
		return err;
	}

	aosl_list_del (&f->node);
	q->iofd_count--;
	iofd_put (f); /* decrease the initial usage count */
	return err;
}

int __mpq_del_fd (struct mp_queue *q, aosl_fd_t fd)
{
	struct iofd *f;
	int err;

	f = iofd_get (fd);
	if (f == NULL)
		return -AOSL_ENOENT;

	err = __q_del_f (q, f);
	iofd_put (f);

	return err;
}

static int ____close (aosl_fd_t fd)
{
	int err = aosl_hal_sk_close (fd);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}

	return err;
}

static int __this_q_close_f (struct mp_queue *q, struct iofd *f)
{
	aosl_fd_t fd = iofd_fobj (f)->fd;
	int err;

	__iofd_get (f);
	err = __q_del_f (q, f);
	if (err < 0) {
		iofd_put (f);
		return err;
	}

	if (f->put_fd_f != NULL) {
		err = f->put_fd_f (fd, f);
	} else {
		err = ____close (fd);
	}

	/**
	 * The io fd object might not be freed due to the
	 * reference count holdings, but the fd has been
	 * closed already, so set it to invalid value.
	 **/
	iofd_fobj (f)->fd = AOSL_INVALID_FD;
	iofd_put (f);
	return err;
}

void f_event_and_close (struct mp_queue *q, struct iofd *f, int iofd_err)
{
	if (!aosl_fd_invalid (iofd_fobj (f)->fd)) {
		if (f->event_f != NULL) {
			/**
			 * The callback function specified by event_f has the responsibility
			 * to release the fd relative resources for the error cases.
			 **/
			f->event_f (iofd_fobj (f)->fd, iofd_err, f->argc, f->argv);
		}

		if (!aosl_fd_invalid (iofd_fobj (f)->fd))
			__this_q_close_f (q, f);
	}
}

void mpq_fini_iofds (struct mp_queue *q)
{
	struct iofd *f;
	struct aosl_list_head *node;

	/* free the active fds */
	while ((node = aosl_list_head (&q->iofds))) {
		f = aosl_list_entry (node, struct iofd, node);
		__this_q_close_f (q, f);
	}

	q->iofd_count = 0;
}

int __close_fd (aosl_fd_t fd)
{
	return ____close (fd);
}

static void ____target_q_close (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	int *err_p = (int *)argv [0];
	struct iofd *f = (struct iofd *)argv [1];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	*err_p = __this_q_close_f (THIS_MPQ (), f);
}

int __iofd_close (aosl_fd_t fd)
{
	struct iofd *f;
	struct mp_queue *q;
	struct mp_queue *this_q;
	int err;
	uintptr_t argv [2];

	f = iofd_get (fd);
	if (f == NULL)
		return ____close (fd);

	this_q = THIS_MPQ ();
	if (this_q != NULL && f->q == this_q->qid) {
		q = this_q;
	} else {
		q = __mpq_get_or_this (f->q);
		if (q == NULL) {
			err = -AOSL_EINVAL;
			goto __put_f;
		}
	}

	argv [0] = (uintptr_t)&err;
	argv [1] = (uintptr_t)f;
	if (__mpq_call_argv (q, -1, "____target_q_close", ____target_q_close, 2, argv) < 0)
		err = aosl_errno;

	if (q != this_q)
		__mpq_put_or_this (q);

__put_f:
	iofd_put (f);
	return err;
}

__export_in_so__ int aosl_close (aosl_fd_t fd)
{
	return_err (__iofd_close (fd));
}

static void ____target_q_del_fd (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	int *err_p = (int *)argv [0];
	struct iofd *f = (struct iofd *)argv [1];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	*err_p = __q_del_f (THIS_MPQ (), f);
}

static int __iofd_del (aosl_fd_t fd)
{
	struct iofd *f;
	struct mp_queue *q;
	int err;
	uintptr_t argv [2];

	f = iofd_get (fd);
	if (f == NULL)
		return -AOSL_EBADF;

	q = __mpq_get_or_this (f->q);
	if (q == NULL) {
		err = -AOSL_EINVAL;
		goto __put_f;
	}

	argv [0] = (uintptr_t)&err;
	argv [1] = (uintptr_t)f;
	if (__mpq_call_argv (q, -1, "____target_q_del_fd", ____target_q_del_fd, 2, argv) < 0)
		err = aosl_errno;

	__mpq_put_or_this (q);

__put_f:
	iofd_put (f);
	return err;
}

__export_in_so__ int aosl_mpq_del_fd (aosl_fd_t fd)
{
	return_err (__iofd_del (fd));
}

static isize_t ____write (struct iofd *f, const void *buf, size_t len)
{
	w_buffer_t *node;
	isize_t err;

	if (len > FD_MAX_WBUF_SIZE)
		return -AOSL_EMSGSIZE;

	if (w_queue_space (&f->w_q) < len)
		return -AOSL_EAGAIN;

	if (f->w_q.head != NULL || (f->flags & IOFD_NOT_READY) != 0) {
		err = 0;
		goto __queue_it;
	}

	err = aosl_hal_sk_write (iofd_fobj (f)->fd, buf, len);

	f->flags |= AOSL_POLLOUT;

	if (err <= 0) {
		return aosl_hal_set_error(err);
	}

	if ((size_t)err < len) {
__queue_it:
		node = aosl_malloc (sizeof (w_buffer_t) + AOSL_I_ALIGN_PTR (len - err));
		if (node == NULL)
			return -AOSL_ENOMEM;

		memcpy (node + 1, (char *)buf + err, len - err);
		node->w_data = node + 1;
		node->w_tail = (char *)(node + 1) + (len - err);
		node->w_extra_size = 0;

		w_queue_add (&f->w_q, node);
	}

	return len;
}

static void ____target_q_write (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	isize_t *err_p = (isize_t *)argv [0];
	struct iofd *f = (struct iofd *)argv [1];
	const void *buf = (const void *)argv [2];
	size_t len = (size_t)argv [3];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	*err_p = ____write (f, buf, len);
}

__export_in_so__ isize_t aosl_write (aosl_fd_t fd, const void *buf, size_t len)
{
	struct iofd *f;
	isize_t err = -AOSL_EINVAL;

	f = iofd_get (fd);
	/**
	 * We do not support write operation on a fd if it is not added to
	 * the mpq, please use the system call API directly in these cases.
	 **/
	if (f != NULL) {
		struct mp_queue *q = __mpq_get_or_this (f->q);
		if (q != NULL) {
			uintptr_t argv [4];

			argv [0] = (uintptr_t)&err;
			argv [1] = (uintptr_t)f;
			argv [2] = (uintptr_t)buf;
			argv [3] = (uintptr_t)len;
			if (__mpq_call_argv (q, -1, "____target_q_write", ____target_q_write, 4, argv) < 0)
				err = aosl_errno;

			__mpq_put_or_this (q);
		}

		iofd_put (f);
	}

	return_err (err);
}

__export_in_so__ int aosl_mpq_fd_arg (aosl_fd_t fd, uintptr_t n, uintptr_t *arg)
{
	struct iofd *f;
	int err = -AOSL_EINVAL;

	f = iofd_get (fd);
	if (f != NULL) {
		err = -AOSL_ENOENT;
		if (n < f->argc) {
			if (arg != NULL)
				*arg = f->argv [n];

			err = 0;
		}

		iofd_put (f);
	}

	return_err (err);
}
