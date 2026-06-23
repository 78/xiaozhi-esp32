/***************************************************************************
 * Module:	Multiplex queue pool implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <kernel/kernel.h>
#include <api/aosl_types.h>
#include <api/aosl_alloca.h>
#include <api/aosl_mm.h>
#include <api/aosl_time.h>
#include <api/aosl_mpqp.h>

#include <kernel/thread.h>
#include <kernel/mp_queue.h>
#include <kernel/err.h>
#include <kernel/refobj.h>

#define UNUSED(expr) (void)(expr)

struct pool_entry {
	struct mp_queue *q;
	uint32_t usage;
};

struct mpq_pool {
	int pool_size;

	k_lock_t lock;
	struct pool_entry *pool_entries;
	int q_count;

	int q_pri;
	int q_max;
	int q_max_idles;
	int q_flags;
	int q_stack_size;
	char qp_name [THREAD_NAME_LEN];
	char q_name [THREAD_NAME_LEN];

	aosl_mpq_init_t q_init;
	aosl_mpq_fini_t q_fini;
	void *q_arg;
};

static struct mpq_pool *cpu_pool = NULL;
static struct mpq_pool *gpu_pool = NULL;
static struct mpq_pool *gen_pool = NULL;
static struct mpq_pool *ltw_pool = NULL;

static __inline__ struct mp_queue *__mpqp_find_best_q_locked (struct mpq_pool *qp)
{
	int i;
	struct mp_queue *best = NULL;

	for (i = 0; i < qp->q_count; i++) {
		struct pool_entry *entry = &qp->pool_entries [i];
		struct mp_queue *q = entry->q;
		if (best == NULL || atomic_read (&q->count) < atomic_read (&best->count))
			best = q;
	}

	return best;
}

static __inline__ struct pool_entry *__mpqp_find_best_alloc_entry_locked (struct mpq_pool *qp)
{
	int i;
	struct pool_entry *best = NULL;

	for (i = 0; i < qp->q_count; i++) {
		struct pool_entry *entry = &qp->pool_entries [i];
		if (best == NULL || entry->usage < best->usage)
			best = entry;
	}

	return best;
}

static __inline__ struct pool_entry *__mpqp_find_entry_with_qid_locked (struct mpq_pool *qp, aosl_mpq_t qid)
{
	int i;
	for (i = 0; i < qp->q_count; i++) {
		struct pool_entry *entry = &qp->pool_entries [i];
		if (entry->q->qid == qid)
			return entry;
	}

	return NULL;
}

struct mpqp_q_idle_stat {
	int idle_counter;
	uint64_t last_exec_funcs_count;
};

static void __mpqp_q_check_shrink (aosl_timer_t timer, const aosl_ts_t *now_p, uintptr_t argc, uintptr_t argv [])
{
	struct mpqp_q_idle_stat *idle_stat_p = (struct mpqp_q_idle_stat *)argv [0];
	struct mpq_pool *qp = (struct mpq_pool *)argv [1];
	struct mp_queue *this_q = THIS_MPQ ();
	int idle_counter;

	UNUSED (timer);
	UNUSED (now_p);
	UNUSED (argc);

	if (qp->q_max_idles < 0) {
		/* should not happen */
		abort ();
	}

	if (this_q->exec_funcs_count == idle_stat_p->last_exec_funcs_count && atomic_read (&this_q->count) == 0) {
		idle_stat_p->idle_counter++;
	} else {
		idle_stat_p->idle_counter = 0;
	}

	idle_stat_p->last_exec_funcs_count = this_q->exec_funcs_count;

	idle_counter = idle_stat_p->idle_counter;

	/**
	 * 1. Shrink all after idle 2 * q_max_idles seconds;
	 * 2. Shrink one after idle q_max_idles seconds;
	 **/
	if (idle_counter >= (2 * qp->q_max_idles)) {
		aosl_mpqp_shrink_all ((aosl_mpqp_t)qp, 1);
	} else if (idle_counter >= qp->q_max_idles) {
		aosl_mpqp_shrink ((aosl_mpqp_t)qp, 1);
	}
}

static void __mpqp_q_check_timer_dtor (uintptr_t argc, uintptr_t argv [])
{
	struct mpqp_q_idle_stat *idle_stat_p = (struct mpqp_q_idle_stat *)argv [0];
	UNUSED (argc);
	aosl_free (idle_stat_p);
}

static int __mpqp_q_init (void *arg)
{
	struct mpq_pool *qp = (struct mpq_pool *)arg;

	if (qp->q_max_idles > 0) {
		struct mpqp_q_idle_stat *idle_stat_p;
		aosl_timer_t timer;

		idle_stat_p = (struct mpqp_q_idle_stat *)aosl_malloc (sizeof (struct mpqp_q_idle_stat));
		if (idle_stat_p == NULL)
			return -1;

		idle_stat_p->idle_counter = 0;
		idle_stat_p->last_exec_funcs_count = 0;
		timer = aosl_mpq_set_timer (1000, __mpqp_q_check_shrink, __mpqp_q_check_timer_dtor, 2, idle_stat_p, qp);
		if (aosl_mpq_timer_invalid (timer)) {
			aosl_free (idle_stat_p);
			return -1;
		}
	}

	if (qp->q_init != NULL)
		return qp->q_init (qp->q_arg);

	return 0;
}

static void __mpqp_q_fini (void *arg)
{
	struct mpq_pool *qp = (struct mpq_pool *)arg;

	if (qp->q_fini != NULL)
		qp->q_fini (qp->q_arg);
}

static __inline__ struct mp_queue *__pool_create_mpq (struct mpq_pool *qp, const char *q_name)
{
	return __mpq_create (qp->q_flags | AOSL_MPQ_FLAG_DESTROY_NOT_ALLOWED, qp->q_pri, qp->q_stack_size, qp->q_max, q_name, __mpqp_q_init, __mpqp_q_fini, (void *)qp);
}

static struct pool_entry *__pool_create_add_mpq_locked (struct mpq_pool *qp)
{
	struct mp_queue *q;
	struct pool_entry *entry;

	// Limit the print length of qp_name to prevent compiler errors
	snprintf (qp->q_name, sizeof(qp->q_name), "%.11s.%d", qp->qp_name, qp->q_count);
	q = __pool_create_mpq (qp, qp->q_name);
	if (q == NULL) {
		int err = aosl_errno;
		return ERR_PTR (-err);
	}

	entry = &qp->pool_entries [qp->q_count];
	BUG_ON (entry->q != NULL || entry->usage != 0);
	entry->q = q;
	entry->usage = 1;
	qp->q_count++;
	return entry;
}

static struct mp_queue *__mpqp_best_q_get (struct mpq_pool *qp)
{
	struct mp_queue *q;
	k_lock_lock (&qp->lock);
	q = __mpqp_find_best_q_locked (qp);
	if (q == NULL || (atomic_read (&q->count) > 0 && qp->q_count < qp->pool_size)) {
		struct pool_entry *entry = __pool_create_add_mpq_locked (qp);
		if (IS_ERR_OR_NULL (entry)) {
			if (q == NULL)
				q = ERR_PTR (PTR_ERR (entry));
		} else {
			q = entry->q;
		}
	}

	if (!IS_ERR_OR_NULL (q)) {
		____q_get (q); /* Some other thread might shrink the pool after we released the lock, so... */
		atomic_inc (&q->count);
	}
	k_lock_unlock (&qp->lock);
	return q;
}

aosl_mpq_t genp_best_q_get (void)
{
	struct mp_queue *q;

	q = __mpqp_best_q_get (gen_pool);
	if (!IS_ERR_OR_NULL (q))
		return q->qid;

	return AOSL_MPQ_INVALID;
}

static struct pool_entry *__mpqp_best_entry_get_or_alloc (struct mpq_pool *qp)
{
	struct pool_entry *entry;
	k_lock_lock (&qp->lock);
	entry = __mpqp_find_best_alloc_entry_locked (qp);
	if (entry == NULL || (entry->usage > 1 && qp->q_count < qp->pool_size)) {
		struct pool_entry *new_entry = __pool_create_add_mpq_locked (qp);
		if (IS_ERR_OR_NULL (new_entry)) {
			if (entry == NULL)
				entry = ERR_PTR (PTR_ERR (new_entry));
		} else {
			entry = new_entry;
		}
	}

	if (!IS_ERR_OR_NULL (entry))
		entry->usage++;

	k_lock_unlock (&qp->lock);
	return entry;
}

static void __mpqp_best_q_put (struct mp_queue *q)
{
	atomic_dec (&q->count);
	____q_put (q);
}

int mpqp_best_q_put (aosl_mpq_t qid)
{
	struct mp_queue *q = __mpq_get (qid);
	if (q != NULL) {
		__mpqp_best_q_put (q);
		__mpq_put (q);
		return 0;
	}

	return -1;
}

static int __mpqp_best_q_queue (struct mpq_pool *qp, void *q_fn, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, void *f, ...)
{
	struct mp_queue *q;
	int err;
	va_list args;
	uintptr_t argc;
	size_t len;

	q = __mpqp_best_q_get (qp);
	if (!IS_ERR_OR_NULL (q)) {
		va_start (args, f);
		if (q_fn == (void *)__mpq_queue_args) {
			va_list *args_p;

			argc = va_arg (args, uintptr_t);
			args_p = va_arg (args, va_list *);
			err = ((mpq_queue_args_t)q_fn) (q, done_qid, ref, f_name, f, argc, *args_p);
		} else if (q_fn == (void *)__mpq_queue_argv || q_fn == (void *)__mpq_queue_no_fail_argv) {
			uintptr_t *argv;

			argc = va_arg (args, uintptr_t);
			argv = va_arg (args, uintptr_t *);
			err = ((mpq_queue_argv_t)q_fn) (q, done_qid, ref, f_name, f, argc, argv);
		} else if (q_fn == (void *)__mpq_queue_data || q_fn == (void *)__mpq_queue_no_fail_data) {
			void *data;

			len = va_arg (args, size_t);
			data = va_arg (args, void *);
			err = ((mpq_queue_data_t)q_fn) (q, done_qid, ref, f_name, f, len, data);
		} else {
			aosl_errno = AOSL_EINVAL;
			err = -1;
		}
		va_end (args);
	} else {
		aosl_errno = -PTR_ERR (q);
		err = -1;
	}

	/**
	 * If there's no error, then we return the selected queue id,
	 * then we can use the same queue next time if we want.
	 **/
	if (err >= 0)
		err = q->qid;

	if (!IS_ERR_OR_NULL (q))
		__mpqp_best_q_put (q);

	return err;
}

static int __mpqp_best_q_call (struct mpq_pool *qp, void *q_fn, aosl_ref_t ref, const char *f_name, void *f, ...)
{
	struct mp_queue *q;
	int err;
	va_list args;
	uintptr_t argc;
	size_t len;

	q = __mpqp_best_q_get (qp);
	if (!IS_ERR_OR_NULL (q)) {
		va_start (args, f);
		if (q_fn == (void *)__mpq_call_args) {
			va_list *args_p;

			argc = va_arg (args, uintptr_t);
			args_p = va_arg (args, va_list *);
			err = ((mpq_call_args_t)q_fn) (q, ref, f_name, f, argc, *args_p);
		} else if (q_fn == (void *)__mpq_call_argv) {
			uintptr_t *argv;

			argc = va_arg (args, uintptr_t);
			argv = va_arg (args, uintptr_t *);
			err = ((mpq_call_argv_t)q_fn) (q, ref, f_name, f, argc, argv);
		} else {
			void *data;

			len = va_arg (args, size_t);
			data = va_arg (args, void *);
			err = ((mpq_call_data_t)q_fn) (q, ref, f_name, f, len, data);
		}
		va_end (args);
	} else {
		aosl_errno = -PTR_ERR (q);
		err = -1;
	}

	/**
	 * If there's no error, then we return the selected queue id,
	 * then we can use the same queue next time if we want.
	 **/
	if (err >= 0)
		err = q->qid;

	if (!IS_ERR_OR_NULL (q))
		__mpqp_best_q_put (q);

	return err;
}

/**
 * GNU compiler does not allow 'inline' when uses 'va_list' local variable, error is:
 * error: function 'XXX' can never be inlined because it uses variable argument lists
 **/
static int __mpqp_best_q_queue_args (struct mpq_pool *qp, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	va_list __args;
	int err;

	/**
	 * KEEP IN MIND all the time, we must not pass the address of a 'va_list' arg
	 * of a function to another function, because 'va_list' is not a normal type
	 * it is a compiler special type, and may be various across compilers.
	 * Otherwise, we may encounter crashes.
	 **/
	va_copy (__args, args);
	err = __mpqp_best_q_queue (qp, __mpq_queue_args, done_qid, ref, f_name, f, argc, &__args);
	va_end (__args);
	return err;
}

/**
 * GNU compiler does not allow 'inline' when uses 'va_list' local variable, error is:
 * error: function 'XXX' can never be inlined because it uses variable argument lists
 **/
static int __mpqp_best_q_call_args (struct mpq_pool *qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	va_list __args;
	int err;

	/**
	 * KEEP IN MIND all the time, we must not pass the address of a 'va_list' arg
	 * of a function to another function, because 'va_list' is not a normal type
	 * it is a compiler special type, and may be various across compilers.
	 * Otherwise, we may encounter crashes.
	 **/
	va_copy (__args, args);
	err = __mpqp_best_q_call (qp, __mpq_call_args, ref, f_name, f, argc, &__args);
	va_end (__args);
	return err;
}

static __inline__ int __mpqp_best_q_queue_argv (struct mpq_pool *qp, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __mpqp_best_q_queue (qp, __mpq_queue_argv, done_qid, ref, f_name, f, argc, argv);
}

static __inline__ int __mpqp_best_q_queue_no_fail_argv (struct mpq_pool *qp, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __mpqp_best_q_queue (qp, __mpq_queue_no_fail_argv, done_qid, ref, f_name, f, argc, argv);
}

static __inline__ int __mpqp_best_q_call_argv (struct mpq_pool *qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __mpqp_best_q_call (qp, __mpq_call_argv, ref, f_name, f, argc, argv);
}

static __inline__ int __mpqp_best_q_queue_data (struct mpq_pool *qp, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __mpqp_best_q_queue (qp, __mpq_queue_data, done_qid, ref, f_name, f, len, data);
}

static __inline__ int __mpqp_best_q_call_data (struct mpq_pool *qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __mpqp_best_q_call (qp, __mpq_call_data, ref, f_name, f, len, data);
}

aosl_mpq_t genp_queue_no_fail_argv (aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	return __mpqp_best_q_queue_no_fail_argv (gen_pool, dq, ref, f_name, f, argc, argv);
}



/**
 * Create a multiplex queue pool.
 * Parameter:
 *   pool_size: specify the pool size, must > 0 and <= MPQP_MAX_SIZE
 *          pri: the priority of the mpq in this mpq pool, 0 for system default
 *          max: specify the max queue size, must > 0 and <= MPQ_MAX_SIZE
 *    max_idles: specify the max idle count before shrinking the pool:
 *                                <0: no max idle count
 *               ==0 || > 0x7fffffff: invalid
 *                      other values: the max idle count
 *            init: the initialize callback function
 *            fini: the finalize callback function
 *           arg: the parameter passed to init callback
 * Return value:
 *     the queue object just created, NULL when failed.
 **/
static struct mpq_pool *__mpqp_create (int pool_size, int pri, int stack_size, int max, int max_idles, int flags, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg)
{
	struct mpq_pool *qp;
	int i;

	if ((pool_size < 1 || pool_size > MPQP_MAX_SIZE) || (max < 1 || max > MPQ_MAX_SIZE) || (max_idles == 0 || max_idles > (0x7fffffff / 3))) {
		aosl_errno = AOSL_EINVAL;
		return NULL;
	}

	qp = (struct mpq_pool *)aosl_malloc (sizeof (struct mpq_pool));
	if (qp == NULL) {
		aosl_errno = AOSL_ENOMEM;
		return NULL;
	}

	qp->pool_entries = (struct pool_entry *)aosl_malloc (sizeof (struct pool_entry) * pool_size);
	if (qp->pool_entries == NULL) {
		aosl_free (qp);
		aosl_errno = AOSL_ENOMEM;
		return NULL;
	}

	for (i = 0; i < pool_size; i++) {
		qp->pool_entries [i].q = NULL;
		qp->pool_entries [i].usage = 0;
	}

	qp->pool_size = pool_size;
	k_lock_init (&qp->lock);
	qp->q_count = 0;

	qp->q_pri = pri;
	qp->q_max = max;
	qp->q_max_idles = max_idles;
	qp->q_flags = flags;
	qp->q_stack_size = stack_size;

	if (name != NULL) {
		snprintf (qp->qp_name, THREAD_NAME_LEN, "%s", name);
	} else {
		qp->qp_name [0] = '\0';
	}

	qp->q_init = init;
	qp->q_fini = fini;
	qp->q_arg = arg;
	return qp;
}

__export_in_so__ aosl_mpqp_t aosl_mpqp_create (int pool_size, int pri, int stack_size, int max, int max_idles, int flags, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg)
{
	return (aosl_mpqp_t)__mpqp_create (pool_size, pri, stack_size, max, max_idles, flags, name, init, fini, arg);
}

static int __mpqp_create_cpu_pool (void)
{
	int cpus;

	cpus = k_processors_count ();
	if (cpus < 1 || cpus > MPQP_MAX_SIZE)
		cpus = 1;

	cpu_pool = __mpqp_create (cpus, AOSL_THRD_PRI_HIGH, 0, 10000, -1, 0, "CPUP", NULL, NULL, NULL);
	if (cpu_pool == NULL)
		return -1;

	return 0;
}

static int __mpqp_create_gpu_pool (void)
{
	int cpus;

	cpus = k_processors_count ();
	if (cpus < 1 || cpus > MPQP_MAX_SIZE)
		cpus = 1;

	gpu_pool = __mpqp_create (cpus, AOSL_THRD_PRI_HIGH, 0, 10000, -1, 0, "GPUP", NULL, NULL, NULL);
	if (gpu_pool == NULL)
		return -1;

	return 0;
}

static int __mpqp_create_gen_pool (void)
{
	int cpus;

	cpus = k_processors_count ();
	if (cpus < 1 || cpus > MPQP_MAX_SIZE)
		cpus = 1;

	/**
	 * 2 times of CPU count for the GEN pool due to the operations
	 * running in this pool are blockable.
	 **/
	gen_pool = __mpqp_create (cpus * 2, AOSL_THRD_PRI_HIGH, 0, 10000, -1, 0, "GENP", NULL, NULL, NULL);
	if (gen_pool == NULL)
		return -1;

	return 0;
}

#ifdef CONFIG_LTWP_SIZE
#define LTWP_SIZE CONFIG_LTWP_SIZE
#else
#if defined(__linux__) || defined(__APPLE__)
#define LTWP_SIZE 2
#else
#define LTWP_SIZE 1
#endif
#endif // CONFIG_LTWP_SIZE

// max idles time (s)
#define LTWP_MAX_IDLES 5

static int __mpqp_create_ltw_pool (void)
{
	int ltwp_size = LTWP_SIZE;
	int stack_size = 4 << 10;
	/**
	 * The LTWP size should not be relative with the CPU count,
	 * it can only be determined by the concurrent long time
	 * waiting operations count.
	 * We define this value as following:
	 *     8: for 32bit system
	 *    64: for 64bit system
	 **/
	if (ltwp_size < 1 || ltwp_size > 64)
		ltwp_size = 32;

	ltw_pool = __mpqp_create (ltwp_size, AOSL_THRD_PRI_DEFAULT, stack_size, 100, LTWP_MAX_IDLES, AOSL_MPQ_FLAG_SIGP_EVENT, "LTWP", NULL, NULL, NULL);
	if (ltw_pool == NULL)
		return -1;

	return 0;
}

void k_mpqp_init (void)
{
	if (__mpqp_create_ltw_pool () < 0)
		abort ();
}

/**
 * Queue a function to the pool with args for invoking by the target thread which monitoring
 * the corresponding queue object.
 * Parameter:
 *     qp: the queue pool object
 *     f: the function
 *     argc: the args count
 *     ...: variable args
 * Return value:
 *     Queued target mpq queue id, checking the result with aosl_mpq_invalid:
 *     invalid qid: indicates error, check errno for detail
 *       valid qid: successful, returns the queue id of selected queue.
 **/
__export_in_so__ aosl_mpq_t aosl_mpqp_queue (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	int err;
	va_list args;

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	va_start (args, argc);
	err = __mpqp_best_q_queue_args ((struct mpq_pool *)qp, dq, ref, f_name, f, argc, args);
	va_end (args);

	return err;
}

/* The synchronous version, the target f must have been invoked when this function returns */
__export_in_so__ aosl_mpq_t aosl_mpqp_call (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	int err;
	va_list args;

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	va_start (args, argc);
	err = __mpqp_best_q_call_args ((struct mpq_pool *)qp, ref, f_name, f, argc, args);
	va_end (args);

	return err;
}

/**
 * The same as 'aosl_mpqp_queue' except this function takes a 'va_list' arg for the
 * variable args rather than '...'.
 **/
__export_in_so__ aosl_mpq_t aosl_mpqp_queue_args (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	return __mpqp_best_q_queue_args ((struct mpq_pool *)qp, dq, ref, f_name, f, argc, args);
}

/* The synchronous version, the target f must have been invoked when this function returns */
__export_in_so__ aosl_mpq_t aosl_mpqp_call_args (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	return __mpqp_best_q_call_args ((struct mpq_pool *)qp, ref, f_name, f, argc, args);
}

/**
 * The same as 'aosl_mpqp_queue' except this function takes a 'uintptr_t *argv' arg for the
 * variable args rather than '...'.
 **/
__export_in_so__ aosl_mpq_t aosl_mpqp_queue_argv (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	return __mpqp_best_q_queue_argv ((struct mpq_pool *)qp, dq, ref, f_name, f, argc, argv);
}

/* The synchronous version, the target f must have been invoked when this function returns */
__export_in_so__ aosl_mpq_t aosl_mpqp_call_argv (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	return __mpqp_best_q_call_argv ((struct mpq_pool *)qp, ref, f_name, f, argc, argv);
}

__export_in_so__ aosl_mpq_t aosl_mpqp_queue_data (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __mpqp_best_q_queue_data ((struct mpq_pool *)qp, dq, ref, f_name, f, len, data);
}

/* The synchronous version, the target f must have been invoked when this function returns */
__export_in_so__ aosl_mpq_t aosl_mpqp_call_data (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __mpqp_best_q_call_data ((struct mpq_pool *)qp, ref, f_name, f, len, data);
}


static void ____each_pool_promise_f (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	atomic_t *count_p = (atomic_t *)argv [0];
	if (atomic_dec_and_test (count_p)) {
		aosl_mpq_t done_qid = (aosl_mpq_t)argv [1];
		const char *f_name = (const char *)argv [2];
		aosl_mpq_func_argv_t f = (aosl_mpq_func_argv_t)argv [3];
		struct mp_queue *q = THIS_MPQ ();

		aosl_free (count_p);

		q_invoke_f (q, done_qid, robj, f_name, f, queued_ts_p, argc - 4, &argv [4]);

		/* Checking free only, make sure not freeing more than once */
		if (!aosl_mpq_invalid (done_qid) && !aosl_is_free_only (robj)) {
			struct mp_queue *done_q = __mpq_get (done_qid);
			if (done_q != NULL) {
				struct refobj *ref_obj = (struct refobj *)robj;
				aosl_ref_t ref = (ref_obj != NULL) ? ref_obj->obj_id : -1;
				__mpq_queue_no_fail_argv (done_q, AOSL_MPQ_INVALID, ref, f_name, f, argc - 4, &argv [4]);
				__mpq_put (done_q);
			} else {
				q_invoke_f (q, -1, AOSL_FREE_ONLY_OBJ /* free only */, f_name, f, queued_ts_p, argc - 4, &argv [4]);
			}
		}

		if (f_name != NULL)
			aosl_free ((void *)f_name);
	}
}

static int __mpqp_pool_tail_queue_argv (struct mpq_pool *qp, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	atomic_t *count_p;
	struct mp_queue *q;
	int i;
	uintptr_t *this_argv;
	int err = 0;

	count_p = (atomic_t *)aosl_malloc (sizeof (atomic_t));
	if (count_p == NULL)
		return -1;

	k_lock_lock (&qp->lock);
	if (qp->q_count == 0) {
		char q_name [THREAD_NAME_LEN + 12];
		struct pool_entry *entry;

		snprintf (q_name, sizeof(q_name), "%s.%d", qp->qp_name, qp->q_count);
		q = __pool_create_mpq (qp, q_name);
		if (q == NULL) {
			err = -aosl_errno;
			aosl_free (count_p);
			goto __err_unlock;
		}

		entry = &qp->pool_entries [qp->q_count];
		BUG_ON (entry->q != NULL || entry->usage != 0);
		entry->q = q;
		entry->usage++;
		qp->q_count++;
	}

	atomic_set (count_p, qp->q_count);

	this_argv = aosl_alloca (sizeof (uintptr_t) * (4 + argc));
	this_argv [0] = (uintptr_t)count_p; /* the atomic count variable */
	this_argv [1] = (uintptr_t)done_qid; /* the done callback mpq id */
	this_argv [2] = (uintptr_t)aosl_strdup (f_name); /* the target function name */
	this_argv [3] = (uintptr_t)f; /* the target function */

	for (i = 0; i < (int)argc; i++)
		this_argv [4 + i] = argv [i];

	for (i = 0; i < qp->q_count; i++) {
		struct pool_entry *entry = &qp->pool_entries [i];
		q = entry->q;
		__mpq_queue_no_fail_argv (q, AOSL_MPQ_INVALID, ref, NULL, ____each_pool_promise_f, 4 + argc, this_argv);
	}

__err_unlock:
	k_lock_unlock (&qp->lock);

	if (err < 0) {
		aosl_errno = -err;
		return -1;
	}

	return 0;
}

static int __mpqp_pool_tail_queue_args (struct mpq_pool *qp, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	uintptr_t *argv = NULL;

	if (argc > 0) {
		uintptr_t l;
		argv = aosl_alloca (sizeof (uintptr_t) * argc);
		for (l = 0; l < argc; l++)
			argv [l] = va_arg (args, uintptr_t);
	}

	return __mpqp_pool_tail_queue_argv (qp, done_qid, ref, f_name, f, argc, argv);
}

/**
 * Queue a function to the pool and promise all queues have done
 * the previous queued jobs when invoking the this queued func.
 * Parameter:
 *     qp: the queue pool object
 *     dq: the done queue id, -1 for no done notification
 *     f: the function
 *     argc: the args count
 *     ...: variable args
 * Return value:
 *     <0: indicates error, check errno for detail
 *     0: successful.
 **/
__export_in_so__ int aosl_mpqp_pool_tail_queue (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	int err;
	va_list args;

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	va_start (args, argc);
	err = __mpqp_pool_tail_queue_args ((struct mpq_pool *)qp, dq, ref, f_name, f, argc, args);
	va_end (args);

	return err;
}

__export_in_so__ int aosl_mpqp_pool_tail_queue_args (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	return __mpqp_pool_tail_queue_args ((struct mpq_pool *)qp, dq, ref, f_name, f, argc, args);
}

__export_in_so__ int aosl_mpqp_pool_tail_queue_argv (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t argv [])
{
	return __mpqp_pool_tail_queue_argv ((struct mpq_pool *)qp, dq, ref, f_name, f, argc, argv);
}

/**
 * Shrink a multiplex queue pool object by 1.
 * Parameter:
 *        qp: the queue pool object to be shrinked
 *        wait: whether to wait for the q exit
 **/
static int __mpqp_shrink (struct mpq_pool *qp, int wait)
{
	struct mp_queue *q = NULL;
	struct q_wait_entry wait_entry;
	int should_wait = wait;

	if (!qp) {
		return -1;
	}

	k_lock_lock (&qp->lock);
	if (qp->q_count > 1) {
		struct pool_entry *entry = &qp->pool_entries [qp->q_count - 1];
		BUG_ON (entry->usage == 0);
		if (entry->usage == 1) {
			// We only allow shrinking the pool entry when the usage count is 1, and not permit if it was alloc-ed.
			q = entry->q;
			entry->q = NULL;
			entry->usage = 0;
			qp->q_count--;
			if (wait && THIS_MPQ() == q) {
				should_wait = 0;
			}
		}
	}
	k_lock_unlock (&qp->lock);

	if (q != NULL) {
		if (should_wait) {
			__mpq_add_wait (q, &wait_entry);
		}
		____q_get (q);
		__mpq_destroy (q);
		____q_put (q);

		if (should_wait) {
			__mpq_destroy_wait (&wait_entry);
		}

		return 0;
	}

	aosl_errno = AOSL_EPERM;
	return -1;
}

static void __mpqp_shrink_all (struct mpq_pool *qp, int wait)
{
	int q_count = 0;
	int this_idx = -1;
	struct mp_queue *this_q = NULL;
	struct q_wait_entry *wait_entries = NULL;
	int i;

	if (!qp) {
		return;
	}

	k_lock_lock (&qp->lock);
	q_count = qp->q_count;
	if (q_count > 0) {
		if (wait) {
			this_q = THIS_MPQ();
			wait_entries = aosl_alloca (sizeof (struct q_wait_entry) * q_count);
			memset(wait_entries, 0, sizeof (struct q_wait_entry) * q_count);
		}

		for (i = 0; i < q_count; i++) {
			struct pool_entry *entry = &qp->pool_entries [i];
			struct mp_queue *q = entry->q;

			/**
			 * Force to reset the pool entry, no need to consider
			 * the entry is alloc-ed cases, once we destroyed the
			 * target q, then any operations on the q would fail.
			 **/
			entry->q = NULL;
			entry->usage = 0;

			if (q != NULL) {
				if (wait) {
					if (this_q == q) {
						this_idx = i; // not wait in self q
					} else {
						__mpq_add_wait (q, &wait_entries [i]);
					}
				}

				____q_get (q);
				__mpq_destroy (q);
				____q_put (q);
			}
		}

		qp->q_count = 0;
	}
	k_lock_unlock (&qp->lock);

	if (wait_entries != NULL) {
		for (i = 0; i < q_count; i++) {
			if (i == this_idx) continue;
			__mpq_destroy_wait (&wait_entries [i]);
		}
	}
}

void mpqp_shrink_pools (void)
{
	__mpqp_shrink_all (cpu_pool, 1);
	__mpqp_shrink_all (gpu_pool, 1);
	__mpqp_shrink_all (gen_pool, 1);
	__mpqp_shrink_all (ltw_pool, 1);
}

/**
 * Shrink a multiplex queue pool object by 1.
 * Parameter:
 *        qp: the queue pool object to be shrinked
 **/
__export_in_so__ int aosl_mpqp_shrink (aosl_mpqp_t qp, int wait)
{
	return __mpqp_shrink ((struct mpq_pool *)qp, wait);
}

/**
 * Shrink all MPQs in a multiplex queue pool object.
 * Parameter:
 *        qp: the queue pool object to be shrinked
 **/
__export_in_so__ void aosl_mpqp_shrink_all (aosl_mpqp_t qp, int wait)
{
	__mpqp_shrink_all ((struct mpq_pool *)qp, wait);
}

__export_in_so__ aosl_mpqp_t aosl_cpup (void)
{
	return (aosl_mpqp_t)cpu_pool;
}

__export_in_so__ aosl_mpqp_t aosl_gpup (void)
{
	return (aosl_mpqp_t)gpu_pool;
}

__export_in_so__ aosl_mpqp_t aosl_genp (void)
{
	return (aosl_mpqp_t)gen_pool;
}

__export_in_so__ aosl_mpqp_t aosl_ltwp (void)
{
	return (aosl_mpqp_t)ltw_pool;
}

__export_in_so__ aosl_mpq_t aosl_mpq_alloc (void)
{
	struct pool_entry *entry = __mpqp_best_entry_get_or_alloc (gen_pool);
	if (IS_ERR_OR_NULL (entry)) {
		aosl_errno = (int)-PTR_ERR (entry);
		return -1;
	}

	return entry->q->qid;
}

__export_in_so__ int aosl_mpq_free (aosl_mpq_t qid)
{
	struct pool_entry *entry;

	k_lock_lock (&gen_pool->lock);
	entry = __mpqp_find_entry_with_qid_locked (gen_pool, qid);
	if (entry != NULL) {
		if (entry->usage > 1) {
			entry->usage--;
		} else {
			entry = ERR_PTR (-AOSL_EPERM);
		}
	} else {
		entry = ERR_PTR (-AOSL_EINVAL);
	}
	k_lock_unlock (&gen_pool->lock);

	if (IS_ERR (entry)) {
		aosl_errno = PTR_ERR (entry);
		return -1;
	}

	return 0;
}

/**
 * Destroy a multiplex queue pool object.
 * Parameter:
 *        qp: the queue pool object to be destroyed
 **/
__export_in_so__ void aosl_mpqp_destroy (aosl_mpqp_t qpobj, int wait)
{
	struct mpq_pool *qp = (struct mpq_pool *)qpobj;
	__mpqp_shrink_all (qp, wait);
	aosl_free (qp->pool_entries);
	k_lock_destroy (&qp->lock);
	aosl_free (qp);
}

void k_mpqp_fini (void)
{
	aosl_mpqp_destroy (ltw_pool, 1);
	ltw_pool = NULL;
}