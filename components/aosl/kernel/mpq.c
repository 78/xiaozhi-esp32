/***************************************************************************
 * Module:	Multiplex queue implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api/aosl_types.h>
#include <api/aosl_alloca.h>
#include <api/aosl_defs.h>
#include <api/aosl_mm.h>
#include <api/aosl_time.h>
#include <api/aosl_mpq.h>
#include <api/aosl_atomic.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/iofd.h>
#include <kernel/mp_queue.h>

#include <kernel/bitmap.h>
#include <kernel/atomic.h>
#include <kernel/refobj.h>
#include <kernel/err.h>

#define STATIC_MPQ_ID_POOL_SIZE 8

static k_rwlock_t mpq_table_lock;
static bitmap_t *mpq_id_pool_bits = NULL;
static struct mp_queue **mpq_table = NULL;
static int mpq_table_size = 0;

/* 0 is the only invalid life id, so init it to 1 */
static uint16_t __mpqobj_life_id = 1;

static uintptr_t __mpq_count = 0;

#if defined(__linux__) || defined(__APPLE__)
__thread struct mp_queue *__this_q;
struct mp_queue *__get_this_mpq (void)
{
	return __this_q;
}
#else
static k_tls_key_t __this_q_key = -1;
struct mp_queue *__get_this_mpq (void)
{
	return (struct mp_queue *)k_tls_key_get (__this_q_key);
}
#endif

static void mpq_init (void)
{
	k_rwlock_init (&mpq_table_lock);

	mpq_id_pool_bits = bitmap_create(STATIC_MPQ_ID_POOL_SIZE);
	mpq_table = (struct mp_queue **)aosl_malloc_impl (sizeof (struct mp_queue *) * STATIC_MPQ_ID_POOL_SIZE);
	if (!mpq_table || !mpq_id_pool_bits) {
		abort ();
	}
	mpq_table_size = STATIC_MPQ_ID_POOL_SIZE;
	memset (mpq_table, 0, sizeof (struct mp_queue *) * mpq_table_size);

#if !defined(__linux__) && !defined(__APPLE__)
	if (k_tls_key_create (&__this_q_key) != 0)
		abort ();
#endif
}

static void mpq_fini (void)
{
	if (mpq_id_pool_bits) {
		bitmap_destroy (mpq_id_pool_bits);
		mpq_id_pool_bits = NULL;
	}
	if (mpq_table) {
		for (int i = 0; i < mpq_table_size; i++) {
			if (mpq_table [i] != NULL) {
				AOSL_LOG_ERR("[dtor] exist q=%s", mpq_table[i]->q_name);
			}
		}
		aosl_free (mpq_table);
		mpq_table = NULL;
		mpq_table_size = 0;
	}

	k_rwlock_destroy (&mpq_table_lock);
}

#define MPQ_ID_POOL_MAX_SIZE 2048
#define MIN_MPQ_ID 0

static int get_unused_mpq_id (void)
{
	int mpq_id;

	k_rwlock_wrlock (&mpq_table_lock);
	mpq_id = bitmap_find_first_zero_bit (mpq_id_pool_bits);
	if (mpq_id < 0) {
		int new_table_size;
		bitmap_t *new_bits;
		struct mp_queue **new_table;

		if (mpq_table_size >= MPQ_ID_POOL_MAX_SIZE) {
			k_rwlock_wrunlock (&mpq_table_lock);
			return -AOSL_EOVERFLOW;
		}

		new_table_size = mpq_table_size + 8;

		new_bits = bitmap_create (new_table_size);
		if (!new_bits) {
			k_rwlock_wrunlock (&mpq_table_lock);
			return -AOSL_ENOMEM;
		}

		new_table = (struct mp_queue **)aosl_malloc_impl (sizeof (struct mp_queue *) * new_table_size);
		if (!new_table) {
			k_rwlock_wrunlock (&mpq_table_lock);
			bitmap_destroy (new_bits);
			return -AOSL_ENOMEM;
		}

		bitmap_copy (new_bits, mpq_id_pool_bits);
		memcpy (new_table, mpq_table, sizeof (struct mp_queue *) * mpq_table_size);
		memset (new_table + mpq_table_size, 0, (new_table_size - mpq_table_size) * sizeof (struct mp_queue *));

		bitmap_destroy (mpq_id_pool_bits);
		aosl_free (mpq_table);

		mpq_id_pool_bits = new_bits;
		mpq_table = new_table;
		mpq_table_size = new_table_size;

		mpq_id = bitmap_find_first_zero_bit (mpq_id_pool_bits);
		BUG_ON (mpq_id < 0);
	}

	bitmap_set (mpq_id_pool_bits, mpq_id);
	__mpq_count++;
	k_rwlock_wrunlock (&mpq_table_lock);

	return mpq_id + MIN_MPQ_ID;
}

static void __put_unused_mpq_id (int mpq_id)
{
	BUG_ON (mpq_id < 0 || mpq_id >= mpq_table_size);
	k_rwlock_wrlock (&mpq_table_lock);
	__mpq_count--;
	bitmap_clear (mpq_id_pool_bits, mpq_id);
	k_rwlock_wrunlock (&mpq_table_lock);
}

aosl_perf_f_t ____sys_perf_f = NULL;

__export_in_so__ int aosl_perf_set_callback (aosl_perf_f_t perf_f)
{
	int err = -AOSL_EPERM;
	k_rwlock_rdlock (&mpq_table_lock);
	if (__mpq_count == 0) {
		aosl_wmb ();
		____sys_perf_f = perf_f;
		err = 0;
	}
	k_rwlock_rdunlock (&mpq_table_lock);
	return_err (err);
}

static __inline__ aosl_mpq_t make_mpq_obj_id (int16_t mpq_id, uint16_t life_id)
{
	return (aosl_mpq_t)(((uint32_t)life_id << 16) | mpq_id);
}

static __inline__ int16_t get_mpq_id (aosl_mpq_t mpq_obj_id)
{
	return (int16_t)mpq_obj_id;
}

static __inline__ uint16_t get_mpq_life_id (aosl_mpq_t mpq_obj_id)
{
	return (uint16_t)((uint32_t)mpq_obj_id >> 16);
}

static void __mpq_id_install (int mpq_id, struct mp_queue *q)
{
	BUG_ON (mpq_id < MIN_MPQ_ID);
	BUG_ON (mpq_id >= mpq_table_size + MIN_MPQ_ID);

	k_rwlock_wrlock (&mpq_table_lock);
	if (mpq_id - MIN_MPQ_ID < mpq_table_size) {
		if (mpq_table [mpq_id - MIN_MPQ_ID] != NULL)
			abort ();

		mpq_table [mpq_id - MIN_MPQ_ID] = q;
		q->qid = make_mpq_obj_id (mpq_id, __mpqobj_life_id);
		__mpqobj_life_id++;

		/**
		 * 0 is the only invalid life id, so reset it to
		 * 1 if we the life id counter wrapped back.
		 **/
		if (__mpqobj_life_id == 0)
			__mpqobj_life_id = 1;
	}
	k_rwlock_wrunlock (&mpq_table_lock);
}

static int __mpq_id_uninstall (int mpq_id, struct mp_queue *q)
{
	int err;

	BUG_ON (mpq_id < MIN_MPQ_ID);
	mpq_id -= MIN_MPQ_ID;
	BUG_ON (mpq_id >= mpq_table_size);

	k_rwlock_wrlock (&mpq_table_lock);
	if (mpq_table [mpq_id] == q) {
		mpq_table [mpq_id] = NULL;
		err = 0;
	} else {
		err = -AOSL_EINVAL;
	}
	k_rwlock_wrunlock (&mpq_table_lock);
	return err;
}

struct mp_queue *__mpq_get (aosl_mpq_t mpq_obj_id)
{
	int16_t mpq_id = get_mpq_id (mpq_obj_id);
	struct mp_queue *q;

	if (mpq_id < MIN_MPQ_ID)
		return NULL;

	mpq_id -= MIN_MPQ_ID;

	k_rwlock_rdlock (&mpq_table_lock);
	if (mpq_id < mpq_table_size) {
		q = mpq_table [mpq_id];
		if (q != NULL) {
			if (q->qid == mpq_obj_id) {
				____q_get (q);
			} else {
				q = NULL;
			}
		}
	} else {
		q = NULL;
	}
	k_rwlock_rdunlock (&mpq_table_lock);

	return q;
}

void __mpq_put (struct mp_queue *q)
{
	____q_put (q);
}

struct mp_queue *__mpq_get_or_this (aosl_mpq_t mpq_obj_id)
{
	if (!aosl_mpq_invalid (mpq_obj_id)) {
		if (mpq_obj_id == this_mpq_id ())
			return THIS_MPQ ();

		return __mpq_get (mpq_obj_id);
	}

	return NULL;
}

void __mpq_put_or_this (struct mp_queue *q)
{
	if (q != THIS_MPQ ())
		__mpq_put (q);
}

void os_drain_sigp (struct mp_queue *q)
{
	for (;;) {
		char buf [1024];
		int finished = 0;
		isize_t err = aosl_hal_sk_read (q->sigp.piper, buf, sizeof buf);
		if (err > 0) {
			atomic_sub ((int)err, &q->kick_q_count);
		}

		// break when read finished
		if (q->sigp.type == WAKEUP_TYPE_PIPE) {
			finished = err < (isize_t)sizeof(buf);
		} else if (q->sigp.type == WAKEUP_TYPE_SOCKET) {
			finished = err < 1;
		} else {
			finished = 1; // should not enter here
		}
		if (finished) {
			break;
		}
	}
}

void os_mp_kick (struct mp_queue *q)
{
	int ret = 0;
#ifndef __linux__
	/**
	 * 1. lwip_write is not thread safe and this is sovled by
	 * 	  LWIP_TCPIP_CORE_LOCKING feature in lwip 2.x above
	 * 2. Sometimes our customer have to use older lwip version
	 * 	  like 1.4.x and then it's dangerous for os_mp_kick,
	 * 	  which typically causing context switch and causing
	 *    another thread to do os_mp_kick in parallel
	 * 3. We use lock to assure lwip_write in os_mp_kick is atomic
	 *    and exclusive
	 * 4. This workaround can be removed in future when
	 *    LWIP_TCPIP_CORE_LOCKING is extensively supported by our
	 *    customer base
	 * -- Jay Zhang Nov 2th, 2021
	**/
	k_lock_lock (&q->lock);
#endif

	if (q->q_flags & AOSL_MPQ_FLAG_SIGP_EVENT) {
		k_event_pulse(q->sigp.event);
	} else {
		ret = aosl_hal_sk_write (q->sigp.pipew, q, 1) != 1;
	}
	if (ret) {
		atomic_dec (&q->kick_q_count);
	}

#ifndef __linux__
	k_lock_unlock (&q->lock);
#endif
}


void mp_kick_q (struct mp_queue *q)
{
	/**
	 * 1. Write memory barrier to make sure the writing op of
	 *    q->terminated is visible globally now;
	 * 2. Make sure the loading instruction of need_kicking is
	 *    after it was written;
	 **/
	aosl_mb ();
	if (q->need_kicking) {
		atomic_inc (&q->kick_q_count);
		os_mp_kick (q);
	}
}

static __inline__ void __free_fo (struct q_func_obj *fo)
{
	if (fo->f_name != NULL)
		aosl_free ((void *)fo->f_name);

	aosl_free ((void *)fo);
}

#define FO_EXECUTED (void *)(uintptr_t)0x99

static int ____add_f (struct mp_queue *q, int no_fail, int sync, aosl_mpq_t done_qid, aosl_ref_t ref,
								int type_argv, const char *f_name, void *f, size_t len, void *data)
{
	struct q_func_obj *fo;
	k_sync_t sync_obj;
	size_t extra_size;
	struct mp_queue *this_q;
	int err;

	this_q = THIS_MPQ ();
	/**
	 * If this thread is exiting now, we do not allow queue an f
	 * with done qid is us, because the done action would fail
	 * obviously.
	 **/
	if (this_q != NULL && this_q->exiting && done_qid == this_q->qid)
		return -AOSL_EPERM;

	if (sync) {
		extra_size = 0;
	} else {
		extra_size = len;
	}

	fo = (struct q_func_obj *)aosl_malloc (sizeof *fo + extra_size);
	if (fo == NULL) {
		abort ();
		return -AOSL_ENOMEM;
	}

	fo->done_qid = done_qid;
	fo->ref = ref;
	fo->f_name = aosl_strdup (f_name);
	fo->f = (aosl_mpq_func_argv_t)f;
	fo->argc = (uintptr_t)(type_argv ? (len / sizeof (uintptr_t)) : (len | ARGC_TYPE_DATA_LEN));

	if (sync) {
		/**
		 * For the synchronize call cases, we just use the input argv/data, then the
		 * target calling function has a chance to change input argv/data.
		 * Changing argv/data in the target function does not make sense for the call
		 * with '...' and '*_args' cases, only makes sense for call_argv/call_data.
		 **/
		fo->argv = (uintptr_t *)data;

		k_lock_init (&sync_obj.mutex);
		k_cond_init (&sync_obj.cond);
		sync_obj.result = NULL;
		fo->sync_obj = &sync_obj;
	} else {
		fo->argv = (uintptr_t *)(fo + 1);
		if (len > 0)
			memcpy (fo->argv, data, len);

		fo->sync_obj = NULL;
	}

	err = -AOSL_EAGAIN;

	k_lock_lock (&q->lock);
	for (;;) {
		if (no_fail || atomic_read (&q->count) < q->q_max) {
			/* Queue mode, add to tail. */
			fo->next = NULL;
			if (q->tail != NULL) {
				q->tail->next = fo;
			} else {
				q->head = fo;
			}
			q->tail = fo;

			fo->queued_ts = aosl_tick_now ();
			atomic_inc (&q->count);
			k_lock_unlock (&q->lock);

			if (q != this_q) {
				mp_kick_q (q);
			}

			if (sync) {
				k_lock_lock (&sync_obj.mutex);
				/**
				 * Must consider the case of waking up by signal, so
				 * a 'while' rather than 'if' employed here.
				 **/
				while (sync_obj.result != FO_EXECUTED)
					k_cond_wait (&sync_obj.cond, &sync_obj.mutex);
				k_lock_unlock (&sync_obj.mutex);

				k_lock_destroy (&sync_obj.mutex);
				k_cond_destroy (&sync_obj.cond);
			}

			return 0;
		}

		if (q->q_flags & AOSL_MPQ_FLAG_NONBLOCK)
			break;

		/**
		 * The current running mpq thread has been destroyed,
		 * so break out here with AOSL_EINTR. If we do not do this
		 * checking, the system may catch a deadlock scene:
		 * 1. The running this_q is just queuing an fo to q;
		 * 2. The q is just destroying and waiting this_q;
		 * Such as the audio capture q and the main q.
		 **/
		if (this_q != NULL && this_q->terminated) {
			err = -AOSL_EINTR;
			break;
		}

		q->wait_q_count++;
		k_cond_wait (&q->wait_q, &q->lock);
		q->wait_q_count--;
	}

	k_lock_unlock (&q->lock);
	__free_fo (fo);
	return err;
}

int __mpq_queue_no_fail_argv (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	____add_f (q, 1 /* no fail, ignore count */, 0, done_qid, ref, 1, f_name, f, sizeof (uintptr_t) * argc, argv);
	return 0;
}

int __mpq_queue_no_fail_data (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char * f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	____add_f (q, 1 /* no fail, ignore count */, 0, done_qid, ref, 0, f_name, f, len, data);
	return 0;
}

int mpq_queue_no_fail_argv (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	struct mp_queue *q = __mpq_get (tq);
	if (q != NULL) {
		____add_f (q, 1 /* no fail, ignore count */, 0, dq, ref, 1, f_name, f, sizeof (uintptr_t) * argc, argv);
		__mpq_put (q);
		return 0;
	}

	aosl_errno = AOSL_ESRCH;
	return -1;
}

int refobj_on_this_q (aosl_refobj_t robj)
{
	struct mp_queue *this_q = THIS_MPQ ();
	if (this_q != NULL) {
		struct refobj_stack_node *node;
		for (node = this_q->run_func_refobj; node != NULL; node = node->prev) {
			if (node->robj == robj)
				return 1;
		}
	}

	return 0;
}

void q_invoke_f (struct mp_queue *q, aosl_mpq_t done_qid, aosl_refobj_t robj, const char *f_name,
						void *f, const aosl_ts_t *queued_ts_p, uintptr_t argc, uintptr_t *argv)
{
	aosl_ts_t time_stamp = 0;
	aosl_mpq_t prev_qid = q->run_func_done_qid;
	struct refobj_stack_node *prev_refobj = q->run_func_refobj;
	struct refobj_stack_node this_refobj = {
		.robj = robj,
		.prev = prev_refobj
	};
	uintptr_t prev_argc = q->run_func_argc;
	uintptr_t *prev_argv = q->run_func_argv;

	q->run_func_done_qid = done_qid;
	q->run_func_refobj = &this_refobj;
	q->run_func_argc = argc;
	q->run_func_argv = argv;

	if (____sys_perf_f != NULL)
		time_stamp = aosl_tick_us ();

	/**
	 * Please be clear that it has no problem even for the 'len/data' cases,
	 * so, just use this form to invoke the callback function.
	 **/
	((aosl_mpq_func_argv_t)f) (queued_ts_p, robj, (argc & ~ARGC_TYPE_DATA_LEN), argv);

	if (____sys_perf_f != NULL)
		____sys_perf_f (f_name, aosl_is_free_only (robj), (uint32_t)(time_stamp - (*queued_ts_p * 1000)), (uint32_t)(aosl_tick_us () - time_stamp));

	q->run_func_done_qid = prev_qid;
	q->run_func_refobj = prev_refobj;
	q->run_func_argc = prev_argc;
	q->run_func_argv = prev_argv;
}

struct refobj *mpq_invoke_refobj_get (aosl_ref_t ref, int *locked)
{
	if (!aosl_ref_invalid (ref)) {
		struct refobj *robj = refobj_get (ref);
		if (robj != NULL) {
			if (!refobj_is_modify_async (robj)) {
				/**
				 * We only hold the read lock if the type is modify sync.
				 * For the modify async type, we would not hold the read
				 * lock. Some scenarios the destroying of robj might lead
				 * to long time waiting and block the main logic.
				 * So, please be very careful when operating the LTW type
				 * of object, the queued operation to the target mpq can
				 * only access the resources which are valid when the ref
				 * object is still alive.
				 **/
				__refobj_rdlock_raw (robj);
			}

			/**
			 * For the modify async case, we do not hold the rwlock, so
			 * the checking of robj->destroyed is not logic precise, but
			 * doing this checking without holding the lock is harmless
			 * at least, because we have no changing back operation once
			 * the 'destroyed' has been set.
			 **/
			if (refobj_is_destroyed (robj)) {
				if (!refobj_is_modify_async (robj))
					__refobj_rdunlock_raw (robj);

				refobj_put (robj);
				return AOSL_FREE_ONLY_OBJ;
			}

			if (!refobj_is_modify_async (robj))
				*locked = 1;

			return robj;
		}

		return AOSL_FREE_ONLY_OBJ;
	}

	return NULL;
}

void mpq_invoke_refobj_put (struct refobj *robj, int locked)
{
	if (!aosl_is_free_only (robj) && robj != NULL) {
		if (locked)
			__refobj_rdunlock_raw (robj);

		refobj_put (robj);
	}
}

static __inline__ void __invoke_f (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name,
										void *f, const aosl_ts_t *queued_ts_p, uintptr_t argc, uintptr_t *argv)
{
	struct refobj *robj;
	int locked = 0;


	robj = mpq_invoke_refobj_get (ref, &locked);

	q_invoke_f (q, done_qid, robj, f_name, f, queued_ts_p, argc, argv);

	mpq_invoke_refobj_put (robj, locked);

	/* Checking free only, make sure not freeing more than once */
	if (!aosl_mpq_invalid (done_qid) && !aosl_is_free_only (robj)) {
		struct mp_queue *done_q = __mpq_get (done_qid);
		if (done_q != NULL) {
			if (!(argc & ARGC_TYPE_DATA_LEN)) {
				____add_f (done_q, 1 /* no fail, ignore count */, 0, AOSL_MPQ_INVALID, ref, 1, f_name, f, sizeof (uintptr_t) * argc, argv);
			} else {
				____add_f (done_q, 1 /* no fail, ignore count */, 0, AOSL_MPQ_INVALID, ref, 0, f_name, f, (size_t)(argc & ~ARGC_TYPE_DATA_LEN), argv);
			}
			__mpq_put (done_q);
		} else {
			q_invoke_f (q, AOSL_MPQ_INVALID, AOSL_FREE_ONLY_OBJ /* free only */, f_name, f, queued_ts_p, argc, argv);
		}
	}
}

static __inline__ void __process_fo (struct mp_queue *q, struct q_func_obj *fo)
{
	k_sync_t *sync_obj = fo->sync_obj;

	__invoke_f (q, fo->done_qid, fo->ref, fo->f_name, fo->f, &fo->queued_ts, fo->argc, fo->argv);
	mpq_stack_fini (q->q_stack_curr);
	__free_fo (fo);
	/* Decrease the queued count before possible wakeup for sync call */
	atomic_dec (&q->count);

	if (sync_obj != NULL) {
		k_lock_lock (&sync_obj->mutex);
		sync_obj->result = FO_EXECUTED;
		k_cond_signal (&sync_obj->cond);
		k_lock_unlock (&sync_obj->mutex);
	}
}

static int __check_and_call_funcs (struct mp_queue *q)
{
	int count = 0;

	/**
	 * No memory access fence needed here although it is
	 * lockless here, because we hold a lock when writing
	 * this list.
	 **/
	if (q->head != NULL) {
		struct q_func_obj *head;

		k_lock_lock (&q->lock);
		head = q->head;
		q->head = NULL;
		q->tail = NULL;
		k_lock_unlock (&q->lock);

		while (head != NULL) {
			struct q_func_obj *fo = head;
			head = head->next;
			__process_fo (q, fo);
			count++;

			k_lock_lock (&q->lock);
			if (q->wait_q_count > 0)
				k_cond_signal (&q->wait_q);
			k_lock_unlock (&q->lock);
		}
	}

	return count;
}

static __inline__ intptr_t mpq_max_wait_time (struct mp_queue *q)
{
	intptr_t msecs = (intptr_t)-1;
	struct timer_node *timer;
	struct timer_base *base = &q->timer_base;

	timer = base->first;
	if (timer != NULL) {
		msecs = (intptr_t)(timer->expire_time - aosl_tick_now ());
		if (msecs < 0)
			msecs = 0;
	}

	return msecs;
}

static __inline__ void __free_q_obj (struct mp_queue *q)
{
	if (q->q_name != NULL) {
		AOSL_LOG_CRT("q_name=%s exit...", q->q_name);
		aosl_free ((void *)q->q_name);
	}

	aosl_free ((void *)q);
}

static aosl_mpq_t aosl_main_qid = AOSL_MPQ_INVALID;

static void __q_destroy (struct mp_queue *q)
{
	int mpq_id = get_mpq_id (q->qid);

	/* finish the iofds */
	mpq_fini_iofds (q);

	/* finish the timers */
	mpq_fini_timers (q);

	os_mp_fini (q);

	k_lock_destroy (&q->lock);
	k_cond_destroy (&q->wait_q);

	if (q->ipv6_prefix_96 != NULL)
		aosl_free (q->ipv6_prefix_96);

	__put_unused_mpq_id (mpq_id - MIN_MPQ_ID);
	if (q->qid == aosl_main_qid) {
		aosl_main_qid = AOSL_MPQ_INVALID;
		aosl_shrink_resources ();
	}

	__free_q_obj (q);
}

static __inline__ void __set_this_mpq (struct mp_queue *q)
{
#if defined(__linux__) || defined(__APPLE__)
	__this_q = q;
#else
	k_tls_key_set (__this_q_key, q);
#endif
}

static void __mp_queue_poll_loop (struct mp_queue *q)
{
	for (;;) {
		int err;
		intptr_t timeo;

		err = __check_and_call_funcs (q);
		if (err > 0)
			q->exec_funcs_count += err;

		err = __check_and_run_timers (q);
		if (err > 0)
			q->exec_timers_count += err;

		if (q->terminated) {
			q->exiting = 1;
			break;
		}

		timeo = mpq_max_wait_time (q);
		err = os_poll_dispatch (q, timeo);
		if (err < 0)
			abort ();

		if (err > 0)
			q->exec_fds_count += err;
	}
}

#define __MPQ_DESTROY_DONE ((void *)(uintptr_t)456)

static void __q_wait_destroy (struct mp_queue *q, aosl_mpq_fini_t fini, void *arg)
{
	struct q_wait_entry *wait;

	/* Uninstall the qid here anyway */
	__mpq_id_uninstall (get_mpq_id (q->qid), q);

	while (atomic_read (&q->usage) > 1) {
		/* check and call the already queued funcs */
		if (__check_and_call_funcs (q) == 0)
			aosl_msleep (1);
	}

	/**
	 * Check and call the already queued funcs again for the racing
	 * of putting q on other thread and the checking of q->usage.
	 **/
	for (;;) {
		/**
		 * We employ a loop to check the queued functions here for
		 * the cases of the invoked callback functions will queue
		 * new functions to our own queue. Yes, this will take the
		 * risk of dead loop, but this should be the responsibility
		 * of applications to avoid these conditions.
		 **/
		if (__check_and_call_funcs (q) == 0)
			break;
	}

	if (fini != NULL)
		fini (arg);

	__set_this_mpq (NULL);

	/**
	 * No need to hold the q->lock here because nobody could
	 * hold our reference at this time, the usage count is 1
	 * now.
	 **/
	wait = q->destroy_wait_head;

	__q_destroy (q);

	while (wait != NULL) {
		struct q_wait_entry *next = wait->next;

		k_lock_lock (&wait->sync.mutex);
		wait->sync.result = __MPQ_DESTROY_DONE;
		k_cond_broadcast (&wait->sync.cond);
		k_lock_unlock (&wait->sync.mutex);

		wait = next;
	}
}

static struct mp_queue *__q_create (const char *name, int flags, int max)
{
	struct mp_queue *q;
	q = (struct mp_queue *)aosl_malloc (sizeof *q);
	if (q != NULL) {
		int err;
		aosl_ts_t tick_us;

		q->q_name = aosl_strdup (name);
		q->q_flags = flags;
		q->q_max = max;
		q->ipv6_prefix_96 = NULL;
		q->need_kicking = 0;

		if (os_mp_init (q) < 0) {
			err = -aosl_errno;
			goto __free_q;
		}

		mpq_init_iofds (q);
		mpq_init_timers (q);

		q->thrd = k_thread_self ();
		q->terminated = 0;
		q->exiting = 0;

		k_lock_init (&q->lock);
		k_cond_init (&q->wait_q);
		q->wait_q_count = 0;

		q->head = NULL;
		q->tail = NULL;
		atomic_set (&q->count, 0);
		atomic_set (&q->kick_q_count, 0);

		q->run_func_done_qid = AOSL_MPQ_INVALID;
		q->run_func_refobj = NULL;
		q->run_func_argc = 0;
		q->run_func_argv = NULL;

		q->q_arg = NULL;

		mpq_stack_init (&q->q_stack_base, AOSL_STACK_INVALID);
		q->q_stack_curr = &q->q_stack_base;

		q->exec_funcs_count = 0;
		q->exec_timers_count = 0;
		q->exec_fds_count = 0;

		tick_us = aosl_tick_us ();
		q->last_idle_ts = tick_us;
		q->last_wake_ts = tick_us;

		q->last_load_us = 0;
		q->last_idle_us = 0;

		atomic_set (&q->usage, 1);
		q->destroy_wait_head = NULL;
		q->destroy_wait_tail = NULL;

		err = get_unused_mpq_id ();
		if (err < 0)
			goto __err_alloc_mpq_id;

		__mpq_id_install (err, q);

		return q;

__err_alloc_mpq_id:
		k_lock_destroy (&q->lock);
		k_cond_destroy (&q->wait_q);

		os_mp_fini (q);

__free_q:
		__free_q_obj (q);
		aosl_errno = -err;
		return NULL;
	}

	return NULL;
}

struct __mpq_create_args {
	const char *name;
	aosl_mpq_init_t init;
	aosl_mpq_fini_t fini;
	void *arg;
	int flags;
	int q_max;
	k_sync_t *sync;
	int err;
};

static void mpq_thread_entry (void *param)
{
	struct __mpq_create_args *args = (struct __mpq_create_args *)param;
	aosl_mpq_fini_t fini = args->fini;
	void *arg = args->arg;
	struct mp_queue *q = __q_create (args->name, args->flags, args->q_max);
	int err = aosl_errno;

	if (q != NULL) {
		q->q_arg = arg;
		__set_this_mpq (q);

		/**
		 * MUST set the stack base before any potential
		 * call to the user callback functions.
		 **/
		q->q_stack_base.id = (aosl_stack_id_t)&q;
		if (args->init != NULL && args->init (arg) < 0) {
			err = aosl_errno;
			q->terminated = 1;
			q->exiting = 1;
			__q_wait_destroy (q, fini, arg);
			q = NULL;
		}
	} else {
		/**
		 * For creating q failed case, we also need to call the
		 * possible fini function to free potential resources.
		 **/
		if (fini != NULL)
			fini (arg);
	}

	k_lock_lock (&args->sync->mutex);
	args->sync->result = q;
	args->err = err;
	k_cond_signal (&args->sync->cond);
	k_lock_unlock (&args->sync->mutex);

	if (q != NULL) {
		/**
		 * MUST set the stack base before any potential
		 * call to the user callback functions.
		 **/
		q->q_stack_base.id = (aosl_stack_id_t)&q;
		__mp_queue_poll_loop (q);
		__q_wait_destroy (q, fini, arg);
	}
}

#define __MPQ_CREATE_INIT ((struct mp_queue *)(uintptr_t)123)

struct mp_queue *__mpq_create (int flags, int pri, int stack_size, int max, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg)
{
	k_thread_t t;
	struct __mpq_create_args args;
	k_sync_t sync;
	int err;
	if (max < 1 || max > MPQ_MAX_SIZE) {
		aosl_errno = AOSL_EINVAL;
		return NULL;
	}

	args.name = name;
	args.init = init;
	args.fini = fini;
	args.arg = arg;
	args.flags = flags;
	args.q_max = max;
	k_lock_init (&sync.mutex);
	k_cond_init (&sync.cond);
	sync.result = __MPQ_CREATE_INIT;
	args.sync = &sync;

	err = k_thread_create (&t, name, pri, stack_size, mpq_thread_entry, &args);
	if (err != 0) {
		k_lock_destroy (&sync.mutex);
		k_cond_destroy (&sync.cond);
		return NULL;
	}

	k_lock_lock (&sync.mutex);
	while (sync.result == __MPQ_CREATE_INIT)
		k_cond_wait (&sync.cond, &sync.mutex);
	k_lock_unlock (&sync.mutex);

	k_lock_destroy (&sync.mutex);
	k_cond_destroy (&sync.cond);

	if (sync.result == NULL)
		aosl_errno = args.err;

	return (struct mp_queue *)sync.result;
}

static aosl_mpq_t __mpq_create_flags (int flags, int pri, int stack_size, int max, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg)
{
	struct mp_queue *q;

	/**
	 * The high 16bits of the flags are reserved, so
	 * should not be used for user.
	 **/
	if ((flags & 0xffff0000) != 0) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	q = __mpq_create (flags, pri, stack_size, max, name, init, fini, arg);
	if (q != NULL)
		return q->qid;

	return AOSL_MPQ_INVALID;
}

__export_in_so__ aosl_mpq_t aosl_mpq_create (int pri, int stack_size, int max, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg)
{
	return __mpq_create_flags (0, pri, stack_size, max, name, init, fini, arg);
}

__export_in_so__ aosl_mpq_t aosl_mpq_create_flags (int flags, int pri, int stack_size, int max, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg)
{
	return __mpq_create_flags (flags, pri, stack_size, max, name, init, fini, arg);
}

static int __queue_change_flags (struct mp_queue *q, int bit_op, int bits)
{
	k_lock_lock (&q->lock);
	switch (bit_op) {
	case BITOP_OR:
		q->q_flags |= bits;
		break;
	case BITOP_AND:
		q->q_flags &= bits;
		break;
	case BITOP_XOR:
		q->q_flags ^= bits;
		break;
	default:
		break;
	}
	k_lock_unlock (&q->lock);

	return 0;
}

__export_in_so__ int aosl_mpq_change_flags (aosl_mpq_t qid, int bit_op, int bits)
{
	int err;
	struct mp_queue *q;

	q = __mpq_get_or_this (qid);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	err = __queue_change_flags (q, bit_op, bits);
	__mpq_put_or_this (q);
	return err;
}

__export_in_so__ int aosl_mpq_get_flags (aosl_mpq_t qid)
{
	int flags;
	struct mp_queue *q;

	q = __mpq_get_or_this (qid);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	k_lock_lock (&q->lock);
	flags = q->q_flags;
	k_lock_unlock (&q->lock);
	__mpq_put_or_this (q);
	return flags;
}

#define __MPQ_DEFAULT_SIZE (10000)

struct mp_queue *__get_or_create_current (void)
{
	struct mp_queue *q = THIS_MPQ ();

	if (q == NULL) {
		q = __q_create (NULL, 0, __MPQ_DEFAULT_SIZE);
		if (q != NULL)
			__set_this_mpq (q);
	}

	return q;
}

__export_in_so__ aosl_mpq_t aosl_mpq_this (void)
{
	return this_mpq_id ();
}

__export_in_so__ aosl_mpq_t aosl_mpq_current (void)
{
	struct mp_queue *q = __get_or_create_current ();
	if (q != NULL)
		return q->qid;

	return -1;
}

__export_in_so__ int aosl_mpq_run_func_arg (uintptr_t n, uintptr_t *arg)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (q->run_func_argv == NULL) {
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	if (q->run_func_argc & ARGC_TYPE_DATA_LEN) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (n >= q->run_func_argc) {
		aosl_errno = AOSL_ENOENT;
		return -1;
	}

	if (arg != NULL)
		*arg = q->run_func_argv [n];

	return 0;
}

__export_in_so__ int aosl_mpq_run_func_data (size_t *len_p, void **data_p)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (q->run_func_argv == NULL) {
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	if (!(q->run_func_argc & ARGC_TYPE_DATA_LEN)) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (len_p != NULL)
		*len_p = (size_t)(q->run_func_argc & ~ARGC_TYPE_DATA_LEN);

	if (data_p != NULL)
		*data_p = (void *)q->run_func_argv;

	return 0;
}

__export_in_so__ aosl_mpq_t aosl_mpq_run_func_done_qid (void)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	return q->run_func_done_qid;
}

__export_in_so__ int aosl_mpq_running_refobj (aosl_refobj_t robj)
{
	if (!aosl_is_free_only (robj) && robj != NULL)
		return refobj_on_this_q (robj);

	return 0;
}

__export_in_so__ void *aosl_mpq_get_q_arg (void)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return NULL;
	}

	return q->q_arg;
}

__export_in_so__ int aosl_mpq_set_q_arg (void *arg)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	q->q_arg = arg;
	return 0;
}

static int __add_or_invoke_f (struct mp_queue *q, int sync, aosl_mpq_t done_qid, aosl_ref_t ref,
							int type_argv, const char *f_name, void *f, size_t len, void *data)
{
	if (sync && !aosl_mpq_invalid (done_qid)) {
		abort ();
		return -AOSL_EINVAL;
	}

	if (sync && (q == THIS_MPQ ())) {
		aosl_ts_t now;
		struct mpq_stack *curr_stack;
		struct mpq_stack stack;

		/**
		 * We could not process the queued functions when the current thread
		 * executing the 'call' relative functions, otherwise, we would take
		 * the risk of re-entrance of some caller functions, such as we call
		 * a function with 'call' q relatives inside when executing a queued
		 * function, we might encounter the re-entrance of the same function.
		 * So, we removed the following codes.
		 * __check_and_call_funcs (q);
		 **/

		now = aosl_tick_now ();
		curr_stack = q->q_stack_curr;
		/**
		 * New stack for sync call uses the same stack id with current stack,
		 * this is important for the nested resume call cases.
		 **/
		mpq_stack_init (&stack, curr_stack->id);
		q->q_stack_curr = &stack;
		__invoke_f (q, done_qid, ref, f_name, f, &now, (uintptr_t)(type_argv ? (len / sizeof (uintptr_t)) : (len | ARGC_TYPE_DATA_LEN)), (uintptr_t *)data);
		mpq_stack_fini (&stack);
		q->q_stack_curr = curr_stack;
		return 0;
	}

	return ____add_f (q, 0, sync, done_qid, ref, type_argv, f_name, f, len, data);
}

/**
 * KEEP IN MIND all the time, we must not pass the address of a 'va_list' arg
 * of a function to another function, because 'va_list' is not a normal type
 * it is a compiler special type, and may be various across compilers.
 * Otherwise, we may encounter crashes.
 **/
static int __add_func_args (struct mp_queue *q, int sync, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	uintptr_t *argv = NULL;

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	if (argc > 0) {
		uintptr_t l;

		argv = aosl_alloca (sizeof (uintptr_t) * argc);
		for (l = 0; l < argc; l++)
			argv [l] = va_arg (args, uintptr_t);
	}

	return_err (__add_or_invoke_f (q, sync, done_qid, ref, 1, f_name, f, argc * sizeof (uintptr_t), (void *)argv));
}

static int __add_func_args_qid (aosl_mpq_t tq, int treat_this, int sync, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	int err;
	struct mp_queue *q;

	if (treat_this) {
		q = __mpq_get_or_this (tq);
	} else {
		q = __mpq_get (tq);
	}

	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	err = __add_func_args (q, sync, dq, ref, f_name, f, argc, args);

	if (treat_this) {
		__mpq_put_or_this (q);
	} else {
		__mpq_put (q);
	}

	return err;
}

static int __add_func_argv (struct mp_queue *q, int sync, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return -1;
	}

	return_err (__add_or_invoke_f (q, sync, done_qid, ref, 1, f_name, f, argc * sizeof (uintptr_t), (void *)argv));
}

static int __add_func_argv_qid (aosl_mpq_t tq, int treat_this, int sync, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	int err;
	struct mp_queue *q;

	if (treat_this) {
		q = __mpq_get_or_this (tq);
	} else {
		q = __mpq_get (tq);
	}

	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	err = __add_func_argv (q, sync, dq, ref, f_name, f, argc, argv);

	if (treat_this) {
		__mpq_put_or_this (q);
	} else {
		__mpq_put (q);
	}

	return err;
}

static int __add_func_data (struct mp_queue *q, int sync, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	if (len > MPQ_DATA_LEN_MAX) {
		aosl_errno = AOSL_EMSGSIZE;
		return -1;
	}

	return_err (__add_or_invoke_f (q, sync, done_qid, ref, 0, f_name, f, len, data));
}

static int __add_func_data_qid (aosl_mpq_t tq, int treat_this, int sync, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	int err;
	struct mp_queue *q;

	if (treat_this) {
		q = __mpq_get_or_this (tq);
	} else {
		q = __mpq_get (tq);
	}

	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	err = __add_func_data (q, sync, dq, ref, f_name, f, len, data);

	if (treat_this) {
		__mpq_put_or_this (q);
	} else {
		__mpq_put (q);
	}

	return err;
}

int __mpq_queue_args (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	return __add_func_args (q, 0, done_qid, ref, f_name, f, argc, args);
}

__export_in_so__ int aosl_mpq_queue_args (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	return __add_func_args_qid (tq, 0, 0, dq, ref, f_name, f, argc, args);
}

int __mpq_call_args (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	return __add_func_args (q, 1, AOSL_MPQ_INVALID, ref, f_name, f, argc, args);
}

__export_in_so__ int aosl_mpq_call_args (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	return __add_func_args_qid (qid, 1, 1, AOSL_MPQ_INVALID, ref, f_name, f, argc, args);
}

__export_in_so__ int aosl_mpq_run_args (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	return __add_func_args_qid (qid, 1, (this_mpq_id () == qid), AOSL_MPQ_INVALID, ref, f_name, f, argc, args);
}

int __mpq_queue (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;
	va_start (args, argc);
	err = __add_func_args (q, 0, done_qid, ref, f_name, f, argc, args);
	va_end (args);
	return err;
}

__export_in_so__ int aosl_mpq_queue (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __add_func_args_qid (tq, 0, 0, dq, ref, f_name, f, argc, args);
	va_end (args);
	return err;
}

int __mpq_call (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;
	va_start (args, argc);
	err = __add_func_args (q, 1, AOSL_MPQ_INVALID, ref, f_name, f, argc, args);
	va_end (args);
	return err;
}

__export_in_so__ int aosl_mpq_call (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __add_func_args_qid (qid, 1, 1, AOSL_MPQ_INVALID, ref, f_name, f, argc, args);
	va_end (args);
	return err;
}

__export_in_so__ int aosl_mpq_run (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __add_func_args_qid (qid, 1, (this_mpq_id () == qid), AOSL_MPQ_INVALID, ref, f_name, f, argc, args);
	va_end (args);
	return err;
}

int __mpq_queue_argv (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __add_func_argv (q, 0, done_qid, ref, f_name, f, argc, argv);
}

__export_in_so__ int aosl_mpq_queue_argv (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __add_func_argv_qid (tq, 0, 0, dq, ref, f_name, f, argc, argv);
}

int __mpq_call_argv (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __add_func_argv (q, 1, AOSL_MPQ_INVALID, ref, f_name, f, argc, argv);
}

__export_in_so__ int aosl_mpq_call_argv (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __add_func_argv_qid (qid, 1, 1, AOSL_MPQ_INVALID, ref, f_name, f, argc, argv);
}

__export_in_so__ int aosl_mpq_run_argv (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv)
{
	return __add_func_argv_qid (qid, 1, (this_mpq_id () == qid), AOSL_MPQ_INVALID, ref, f_name, f, argc, argv);
}

int __mpq_queue_data (struct mp_queue *q, aosl_mpq_t done_qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __add_func_data (q, 0, done_qid, ref, f_name, f, len, data);
}

__export_in_so__ int aosl_mpq_queue_data (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __add_func_data_qid (tq, 0, 0, dq, ref, f_name, f, len, data);
}

int __mpq_call_data (struct mp_queue *q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __add_func_data (q, 1, AOSL_MPQ_INVALID, ref, f_name, f, len, data);
}

__export_in_so__ int aosl_mpq_call_data (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __add_func_data_qid (qid, 1, 1, AOSL_MPQ_INVALID, ref, f_name, f, len, data);
}

__export_in_so__ int aosl_mpq_run_data (aosl_mpq_t qid, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data)
{
	return __add_func_data_qid (qid, 1, (this_mpq_id () == qid), AOSL_MPQ_INVALID, ref, f_name, f, len, data);
}

__export_in_so__ int aosl_mpq_queued_count (aosl_mpq_t qid)
{
	struct mp_queue *q;
	int count;

	q = __mpq_get_or_this (qid);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	count = atomic_read (&q->count);
	__mpq_put_or_this (q);
	return count;
}

__export_in_so__ int aosl_mpq_last_costs (aosl_ts_t *load_p, aosl_ts_t *idle_p)
{
	struct mp_queue *q = THIS_MPQ ();

	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (load_p != NULL)
		*load_p = q->last_load_us;

	if (idle_p != NULL)
		*idle_p = q->last_idle_us;

	return 0;
}

__export_in_so__ int aosl_mpq_exec_counters (uint64_t *funcs_count_p, uint64_t *timers_count_p, uint64_t *fds_count_p)
{
	struct mp_queue *q = THIS_MPQ ();

	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (funcs_count_p != NULL)
		*funcs_count_p = q->exec_funcs_count;

	if (timers_count_p != NULL)
		*timers_count_p = q->exec_timers_count;

	if (fds_count_p != NULL)
		*fds_count_p = q->exec_fds_count;

	return 0;
}

__export_in_so__ void aosl_mpq_loop (void)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q != NULL) {
		/**
		 * MUST set the stack base before any potential
		 * call to the user callback functions.
		 **/
		q->q_stack_base.id = (aosl_stack_id_t)&q;
		__mp_queue_poll_loop (q);
		__q_wait_destroy (q, NULL, NULL);
	}
}

__export_in_so__ int aosl_mpq_this_destroyed (void)
{
	struct mp_queue *q = THIS_MPQ ();
	if (q != NULL)
		return q->terminated;

	return 1;
}

__export_in_so__ int aosl_mpq_is_main (void)
{
	struct mp_queue *this_q = THIS_MPQ ();
	aosl_mpq_t main_qid = aosl_main_qid;
	if (this_q == NULL || aosl_mpq_invalid (main_qid))
		return 0;

	return (int)(this_q->qid == main_qid);
}

void __mpq_destroy (struct mp_queue *q)
{
	struct mp_queue *this_q = THIS_MPQ ();

	q->terminated = 1;

	if (q != this_q)
		mp_kick_q (q);

	if (this_q != NULL) {
		/**
		 * If the target mpq object has been destroyed by us,
		 * and we are waiting its' termination, but the target
		 * mpq is a q which often queues something to us, and
		 * considering the scene that we are just waiting it,
		 * at the same time it is blocked by our wait_q due
		 * to we can not process the queued functions at this
		 * time, then deadlock occurs, such as the audio
		 * capture q and the main q.
		 * So, we must be able to handle these scenes.
		 **/
		k_lock_lock (&this_q->lock);
		if (this_q->wait_q_count > 0) {
			if (this_q->wait_q_count > 1) {
				k_cond_broadcast (&this_q->wait_q);
			} else {
				k_cond_signal (&this_q->wait_q);
			}
		}
		k_lock_unlock (&this_q->lock);
	}
}

static int mpq_destroy (aosl_mpq_t mpq_id, int check_allowed)
{
	struct mp_queue *q;

	q = __mpq_get (mpq_id);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (check_allowed && (q->q_flags & AOSL_MPQ_FLAG_DESTROY_NOT_ALLOWED) != 0) {
		__mpq_put (q);
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	__mpq_destroy (q);
	__mpq_put (q);
	return 0;
}

__export_in_so__ int aosl_mpq_destroy (aosl_mpq_t mpq_id)
{
	return mpq_destroy (mpq_id, 1);
}

void __mpq_add_wait (struct mp_queue *q, struct q_wait_entry *wait)
{
	k_lock_init (&wait->sync.mutex);
	k_cond_init (&wait->sync.cond);
	wait->sync.result = NULL;
	wait->next = NULL;

	/**
	 * We just reuse the q->lock to protect the destroy wait queue,
	 * it should be no problem.
	 **/
	k_lock_lock (&q->lock);
	if (q->destroy_wait_tail != NULL) {
		q->destroy_wait_tail->next = wait;
	} else {
		q->destroy_wait_head = wait;
	}
	q->destroy_wait_tail = wait;
	k_lock_unlock (&q->lock);
}

int __mpq_destroy_wait (struct q_wait_entry *wait)
{
	int err = 0;
	k_lock_lock (&wait->sync.mutex);
	while (wait->sync.result != __MPQ_DESTROY_DONE) {
		k_cond_wait (&wait->sync.cond, &wait->sync.mutex);
	}
	k_lock_unlock (&wait->sync.mutex);
	k_lock_destroy(&wait->sync.mutex);
	k_cond_destroy(&wait->sync.cond);
	return err;
}

static int mpq_do_wait (aosl_mpq_t mpq_id, int issue_destroy, int check_allowed)
{
	struct q_wait_entry wait = {0};
	struct mp_queue *q;

	q = __mpq_get (mpq_id);
	if (q == NULL)
		return -AOSL_EINVAL;

	if (q == THIS_MPQ ()) {
		__mpq_put (q);
		return -AOSL_EBUSY;
	}

	if (issue_destroy && check_allowed && (q->q_flags & AOSL_MPQ_FLAG_DESTROY_NOT_ALLOWED) != 0) {
		__mpq_put (q);
		return -AOSL_EPERM;
	}

	__mpq_add_wait (q, &wait);

	if (issue_destroy)
		__mpq_destroy (q);

	__mpq_put (q);

	return __mpq_destroy_wait (&wait);
}

__export_in_so__ int aosl_mpq_destroy_wait (aosl_mpq_t mpq_id)
{
	return_err (mpq_do_wait (mpq_id, 1, 1));
}

__export_in_so__ int aosl_mpq_wait (aosl_mpq_t mpq_id)
{
	return_err (mpq_do_wait (mpq_id, 0, 1));
}

static void __main_auto_exit (void)
{
	/**
	 * Make sure the possible started main q
	 * exit here.
	 **/
	aosl_main_exit_wait ();
}

static int __main_auto_exit_registered = 0;

__export_in_so__ int aosl_main_start (int pri, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg)
{
	if (atomic_cmpxchg ((atomic_t *)&aosl_main_qid, -1,
		0 /* to indicate we are creating the q */) == -1) {
		/* 1. No destroy; 2. Non block */
		const int main_q_flags = (AOSL_MPQ_FLAG_DESTROY_NOT_ALLOWED | AOSL_MPQ_FLAG_NONBLOCK);
		struct mp_queue *q = __mpq_create (main_q_flags, pri, 0, 100000, "aosl_main", init, fini, arg);
		if (q == NULL)
			return -1;

		aosl_main_qid = q->qid; /* set the real main q */

		if (atomic_cmpxchg ((atomic_t *)&__main_auto_exit_registered, 0, 1) == 0)
			atexit (__main_auto_exit);

		return 0;
	}

	aosl_errno = AOSL_EEXIST;
	return -1;
}

__export_in_so__ aosl_mpq_t aosl_mpq_main (void)
{
	int main_q = (int)aosl_main_qid;
	if (get_mpq_id (main_q) < MIN_MPQ_ID)
		return AOSL_MPQ_INVALID;

	return (aosl_mpq_t)main_q;
}

__export_in_so__ int aosl_main_exit (void)
{
	return mpq_destroy (aosl_main_qid, 0);
}

__export_in_so__ int aosl_main_exit_wait (void)
{
	return_err (mpq_do_wait (aosl_main_qid, 1, 0));
}

__export_in_so__ int aosl_main_wait (void)
{
	return_err (mpq_do_wait (aosl_main_qid, 0, 0));
}

extern void mpqp_shrink_pools (void);

__export_in_so__ void aosl_shrink_resources (void)
{
	mpqp_shrink_pools ();
}

/**
 * The AOSL library initialization function, we implement this function here
 * just for guaranteeing always linked this in, because the mpq is the basic
 * functionality of AOSL.
 **/
extern void k_mm_init (void);
extern void k_mm_fini (void);
extern void os_thread_init (void);
extern void os_thread_fini (void);
extern void k_errno_init (void);
extern void k_errno_fini (void);
extern void k_refobj_init (void);
extern void k_refobj_fini (void);
extern void k_timer_init (void);
extern void k_timer_fini (void);
extern void k_mpqp_init (void);
extern void k_mpqp_fini (void);
extern void k_route_init (void);
extern void k_route_fini (void);

// 0: no init
// 1: init ing
// 2: init done
static atomic_t s_aosl_init_step = 0;

__export_in_so__ void aosl_dtor (void)
{
	if (!atomic_read(&s_aosl_init_step)) {
		return;
	}

	if (THIS_MPQ () != NULL) {
		/**
		 * We do not allow any thread created by aosl to unload
		 * the aosl library, if so, this should be a fatal bug!
		 **/
		abort ();
	}

	k_route_fini ();
	k_mpqp_fini ();
	mpq_fini ();
	iofd_fini ();
	fileobj_fini ();
	k_timer_fini ();
	k_refobj_fini ();
	k_errno_fini ();
	os_thread_fini ();
	k_mm_fini ();

	atomic_set(&s_aosl_init_step, 0);
}

__export_in_so__ void aosl_ctor (void)
{
	if (atomic_read(&s_aosl_init_step)) {
		return;
	}

	atomic_set(&s_aosl_init_step, 1);

	k_mm_init ();
	os_thread_init ();
	k_errno_init ();
	k_refobj_init ();
	k_timer_init ();
	fileobj_init ();
	iofd_init ();
	mpq_init ();
	k_mpqp_init ();
	k_route_init ();

	atomic_set(&s_aosl_init_step, 2);
}