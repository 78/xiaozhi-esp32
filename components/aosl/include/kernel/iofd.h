/***************************************************************************
 * Module		:		Multiplex queue iofd header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __IOFD_H__
#define __IOFD_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_mpq_fd.h>
#include <api/aosl_mpq_timer.h>
#include <kernel/atomic.h>
#include <kernel/list.h>
#include <kernel/fileobj.h>

typedef void (*iofd_data_t) (void *data, size_t len, uintptr_t argc, uintptr_t argv [], const void *extra);
typedef isize_t (*iofd_post_process_t) (void *data, size_t len, uintptr_t argc, uintptr_t argv []);

#define FD_MAX_PACKET_SIZE_MIN 1024 /* 1KB is small enough for one packet */
#define FD_MAX_PACKET_SIZE_MAX (4 << 20) /* 4MB is big enough for one packet */
#define FD_MAX_EXTRA_BYTES (4096) /* 4KB is big enough for the extra bytes */

#define FD_MAX_WBUF_SIZE (128 << 10) /* 128KB for the writing buffer size is big enough */

typedef struct w_buffer_node {
	struct w_buffer_node *next;
	void *w_data;
	void *w_tail;
	size_t w_extra_size;
} w_buffer_t;

typedef struct {
	w_buffer_t *head;
	w_buffer_t *tail;
	uintptr_t count;
	size_t total_len;
} w_queue_t;

extern void w_queue_init (w_queue_t *q);

static __inline__ void w_queue_add (w_queue_t *q, w_buffer_t *b)
{
	b->next = NULL;
	if (q->tail != NULL) {
		q->tail->next = b;
	} else {
		q->head = b;
	}
	q->tail = b;
	q->count++;
	q->total_len += (char *)b->w_tail - (char *)(b + 1);
}

static __inline__ w_buffer_t *w_queue_remove_head (w_queue_t *q)
{
	if (q->head != NULL) {
		w_buffer_t *b = q->head;
		q->head = b->next;
		if (q->head == NULL)
			q->tail = NULL;

		q->count--;
		q->total_len -= (char *)b->w_tail - (char *)(b + 1);
		b->next = NULL;
		return b;
	}

	return NULL;
}

static __inline__ size_t w_queue_space (w_queue_t *q)
{
	if (q->total_len < FD_MAX_WBUF_SIZE)
		return (size_t)(FD_MAX_WBUF_SIZE - q->total_len);

	return 0;
}

struct iofd;

typedef int (*iofd_get_fd_t) (aosl_fd_t *fd_p, struct iofd *f);
typedef int (*iofd_put_fd_t) (aosl_fd_t fd, struct iofd *f);

struct iofd {
	/* MUST BE THE FIRST MEMBER */
	struct file_obj fobj;

	struct aosl_list_head node; /* node for multiplex queue iofds list */

#define IOFD_NOT_READY (1 << 8)
#define IOFD_SOCK_LISTEN (1 << 9)
#define IOFD_READ_RETURN_0 (1 << 10)
#define IOFD_NO_INIT_READ (1 << 11)

	uint32_t flags; // IOFD_xxx above and aosl_poll_type_e

	aosl_mpq_t q;
	aosl_timer_t timer;

	size_t max_pkt_size;
	void *r_head;
	void *r_data;
	void *r_tail;
	size_t r_extra_size;

	w_queue_t w_q;

	aosl_fd_read_t read_f;
	aosl_fd_write_t write_f;
	aosl_check_packet_t chk_pkt_f;
	iofd_post_process_t post_f;

	iofd_put_fd_t put_fd_f;

	iofd_data_t data_f;
	aosl_fd_event_t event_f;
	uintptr_t argc;
	uintptr_t argv [0];
};

#define iofd_fobj(iofd) (&(iofd)->fobj)

static __inline__ void __iofd_get (struct iofd *f)
{
	__fget (iofd_fobj (f));
}

static __inline__ struct iofd *iofd_get (aosl_fd_t fd)
{
	return (struct iofd *)fget (fd);
}

static __inline__ void iofd_put (struct iofd *f)
{
	fput (iofd_fobj (f));
}

struct mp_queue;

extern void mpq_init_iofds (struct mp_queue *q);
extern int make_fd_nb_clex (aosl_fd_t fd);
extern void mpq_fini_iofds (struct mp_queue *q);

extern void f_event_and_close (struct mp_queue *q, struct iofd *f, int iofd_err);


/**
 * According to the real test result, the cost of a simplest
 * syscall(such as getpid) is almost equivalent to a memory
 * copy of 104 bytes, so define the low weight memcpy size
 * to 96 bytes here.
 **/
#define MAX_LW_COPY_SIZE 96
#define MIN_SYSCALL_SIZE (1024)

static inline int __iofd_better_move_buffer (struct iofd *f)
{
	size_t buff_size;

	if (f->chk_pkt_f == NULL)
		return 1;

	/* the chk_pkt_f is not NULL, so the buffer size is 2 max packet size */
	buff_size = f->max_pkt_size * 2;

	/* the left space for reading is too small */
	if (buff_size - ((char *)f->r_tail - (char *)f->r_head) < MIN_SYSCALL_SIZE)
		return 1;

	/* the left space is smaller than a max packet, and the left data len is small enough */
	if ((char *)f->r_data - (char *)f->r_head > (isize_t)f->max_pkt_size && (char *)f->r_tail - (char *)f->r_data < MIN_SYSCALL_SIZE)
		return 1;

	/* the received data is small enough for a moving */
	if ((char *)f->r_tail - (char *)f->r_data <= MAX_LW_COPY_SIZE && (char *)f->r_data - (char *)f->r_head >= (isize_t)(f->max_pkt_size / 2))
		return 1;

	return 0;
}

int __mpq_add_fd_argv (struct mp_queue *q, aosl_fd_t fd, int not_ready_timeo, size_t max_pkt_size, size_t extra_bytes,
						uint32_t flags, aosl_fd_read_t read_f, aosl_fd_write_t write_f, aosl_check_packet_t chk_pkt_f,
	iofd_post_process_t post_f, aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, uintptr_t argv [], ...);
extern int __mpq_del_fd (struct mp_queue *q, aosl_fd_t fd);

extern int __iofd_read_data (struct mp_queue *q, struct iofd *f);
extern int __iofd_write_data (struct mp_queue *q, struct iofd *f);

extern void iofd_init (void);
extern void iofd_fini (void);
extern int __iofd_close (aosl_fd_t fd);
extern int __close_fd (aosl_fd_t fd);

#endif /* __IOFD_H__ */
