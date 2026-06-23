/***************************************************************************
 * Module:	Multiplex queue header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __MP_QUEUE_H__
#define __MP_QUEUE_H__

#include <api/aosl_types.h>

#include <kernel/list.h>
#include <api/aosl_mpq.h>
#include <api/aosl_ref.h>
#include <api/aosl_mpq_fd.h>

#include <kernel/osmp.h>
#include <kernel/timer.h>
#include <kernel/iofd.h>

#include <kernel/atomic.h>
#include <kernel/thread.h>


extern aosl_perf_f_t ____sys_perf_f;

/* Big enough */
#define MPQ_DATA_LEN_MAX 8192
#define ARGC_TYPE_DATA_LEN (0x80000000)

struct q_func_obj {
	struct q_func_obj *next;

	aosl_ts_t queued_ts;

	k_sync_t *sync_obj;
	aosl_mpq_t done_qid;
	aosl_ref_t ref;
	const char *f_name;
	aosl_mpq_func_argv_t f;
	uintptr_t argc;
	uintptr_t *argv;
};

struct q_wait_entry {
	struct q_wait_entry *next;
	k_sync_t sync;
};

struct refobj_stack_node {
	aosl_refobj_t robj;
	struct refobj_stack_node *prev;
};

struct resume_calls {
	atomic_t usage;
	uint32_t task_count;
	struct aosl_list_head list;
};

struct mpq_stack {
	aosl_stack_id_t id;
	struct aosl_list_head *prepare_calls;
	aosl_stack_id_t err_stack_id;
	int task_exec_err;
	struct resume_calls *resume_calls;
	uint32_t prepare_calls_count;
	uint32_t task_exec_count;
};

static __inline__ void mpq_stack_init (struct mpq_stack *stk, aosl_stack_id_t stack_id)
{
	stk->id = stack_id;
	stk->prepare_calls = NULL;
	stk->err_stack_id = AOSL_STACK_INVALID;
	stk->task_exec_err = 0;
	stk->resume_calls = NULL;
	stk->prepare_calls_count = 0;
	stk->task_exec_count = 0;
}

static __inline__ void mpq_stack_fini (struct mpq_stack *stk)
{
	struct aosl_list_head *prepare_calls = stk->prepare_calls;
	(void)prepare_calls;
	stk->prepare_calls = NULL;
	//free_prepare_calls (prepare_calls, NULL);
	stk->err_stack_id = AOSL_STACK_INVALID;
	stk->task_exec_err = 0;
	if (stk->resume_calls != NULL) {
		//resume_calls_put (stk->resume_calls, NULL);
		stk->resume_calls = NULL;
	}
	stk->prepare_calls_count = 0;
	stk->task_exec_count = 0;
}

typedef enum wakeup_type {
	WAKEUP_TYPE_NONE = -1,
	WAKEUP_TYPE_PIPE,
	WAKEUP_TYPE_SOCKET,
	WAKEUP_TYPE_SIGNAL,
	WAKEUP_TYPE_COUNT,
} wakeup_type_e;

struct wakeup_signal {
	wakeup_type_e type;
	aosl_fd_t piper;     // pipe or socket for read
	aosl_fd_t pipew;     // pipe or socket for write
	int activated;       // whether actived
	aosl_event_t  event; // event for signal
};

struct mp_queue {
	const char *q_name;
	atomic_t usage;

	aosl_mpq_t qid;

	aosl_fd_t efd;
	int need_kicking;
	k_thread_t thrd;
	struct wakeup_signal sigp;

	int terminated;
	int exiting;
	int q_flags;
	int q_max;

	/**
	 * The IPv6 prefix for converting an IPv4 address.
	 * Putting this member here is really ugly, but it
	 * is the simplest way, so just keep it here for
	 * now.
	 **/
	void *ipv6_prefix_96;

	k_lock_t lock;
	k_cond_t wait_q;
	int wait_q_count;

	struct q_func_obj *head;
	struct q_func_obj *tail;
	atomic_t count;
	atomic_t kick_q_count;

	aosl_mpq_t run_func_done_qid;
	struct refobj_stack_node *run_func_refobj;
	uintptr_t run_func_argc;
	uintptr_t *run_func_argv;

	void *q_arg;

	struct mpq_stack q_stack_base;
	struct mpq_stack *q_stack_curr;

	uint64_t exec_funcs_count;
	uint64_t exec_timers_count;
	uint64_t exec_fds_count;

	aosl_ts_t last_idle_ts;
	aosl_ts_t last_wake_ts;

	aosl_ts_t last_load_us;
	aosl_ts_t last_idle_us;

	struct aosl_list_head iofds;
	size_t iofd_count;

	struct aosl_list_head timers;
	size_t timer_count;
	struct timer_base timer_base;

	struct q_wait_entry *destroy_wait_head;
	struct q_wait_entry *destroy_wait_tail;
};

static inline void ____q_get (struct mp_queue *q)
{
	atomic_inc (&q->usage);
}

static inline void ____q_put (struct mp_queue *q)
{
	atomic_dec (&q->usage);
}

extern void __mpq_add_wait (struct mp_queue *q, struct q_wait_entry *wait);
extern int __mpq_destroy_wait (struct q_wait_entry *wait);

extern struct mp_queue *__mpq_create (int flags, int pri, int stack_size, int max, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);
extern struct mp_queue *__mpq_get (aosl_mpq_t mpq_id);
extern void __mpq_put (struct mp_queue *q);
extern struct mp_queue *__mpq_get_or_this (aosl_mpq_t mpq_obj_id);
extern void __mpq_put_or_this (struct mp_queue *q);
extern struct mp_queue *__get_or_create_current (void);

extern int __mpq_queue (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);
extern int __mpq_queue_args (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);
extern int __mpq_queue_argv (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);
extern int __mpq_queue_data (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

typedef int (*mpq_queue_args_t) (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);
typedef int (*mpq_queue_argv_t) (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);
typedef int (*mpq_queue_data_t) (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

extern int __mpq_call (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);
extern int __mpq_call_args (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);
extern int __mpq_call_argv (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);
extern int __mpq_call_data (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

typedef int (*mpq_call_args_t) (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);
typedef int (*mpq_call_argv_t) (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);
typedef int (*mpq_call_data_t) (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

extern int __mpq_queue_no_fail_argv (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);
extern int __mpq_queue_no_fail_data (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

extern void q_invoke_f (struct mp_queue *q, aosl_mpq_t done_qid, aosl_refobj_t robj, const char *f_name, void *f, const aosl_ts_t *queued_ts_p, uintptr_t argc, uintptr_t *argv);

extern void __mpq_destroy (struct mp_queue *q);

extern void os_drain_sigp (struct mp_queue *q);

extern struct mp_queue *__get_this_mpq (void);
#define THIS_MPQ() __get_this_mpq ()

static __inline__ aosl_mpq_t this_mpq_id (void)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q != NULL)
		return q->qid;

	return AOSL_MPQ_INVALID;
}

extern struct refobj *mpq_invoke_refobj_get (aosl_ref_t ref, int *locked);
extern void mpq_invoke_refobj_put (struct refobj *robj, int locked);

extern int refobj_on_this_q (aosl_refobj_t robj);

extern int mpq_queue_no_fail_argv (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);

extern void mp_kick_q (struct mp_queue *q);
extern void os_mp_kick (struct mp_queue *q);


#define MPQ_ARGC_MAX AOSL_VAR_ARGS_MAX


#endif /* __MP_QUEUE_H__ */
