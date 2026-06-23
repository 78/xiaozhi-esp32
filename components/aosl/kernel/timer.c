/***************************************************************************
 * Module		:		Red-Black tree based timer implementation
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include <api/aosl_types.h>
#include <api/aosl_alloca.h>
#include <api/aosl_mm.h>
#include <api/aosl_time.h>
#include <kernel/kernel.h>
#include <kernel/bitmap.h>
#include <kernel/err.h>
#include <kernel/timer.h>
#include <kernel/thread.h>
#include <kernel/mp_queue.h>

#define UNUSED(expr) (void)(expr)


#define STATIC_TIMER_ID_POOL_SIZE 8

static k_rwlock_t timer_table_lock;
static bitmap_t *timer_id_pool_bits = NULL;
static struct timer_node **timer_table = NULL;
static int timer_table_size = 0;

/* 0 is the only invalid life id, so init it to 1 */
static uint16_t __timer_life_id = 1;

void k_timer_init (void)
{
	timer_id_pool_bits = bitmap_create(STATIC_TIMER_ID_POOL_SIZE);
	timer_table = (struct timer_node **)aosl_malloc_impl (sizeof (struct timer_node *) * STATIC_TIMER_ID_POOL_SIZE);
	if (!timer_table || !timer_id_pool_bits) {
		abort ();
	}
	timer_table_size = STATIC_TIMER_ID_POOL_SIZE;
	memset (timer_table, 0, sizeof (struct timer_node *) * timer_table_size);

	k_rwlock_init (&timer_table_lock);
}

void k_timer_fini (void)
{
	if (timer_id_pool_bits) {
		bitmap_destroy (timer_id_pool_bits);
		timer_id_pool_bits = NULL;
	}
	if (timer_table) {
		for (int i = 0; i < timer_table_size; i++) {
			if (timer_table [i] != NULL) {
				AOSL_LOG_ERR("[dtor] no free");
			}
		}
		aosl_free (timer_table);
		timer_table = NULL;
		timer_table_size = 0;
	}

	k_rwlock_destroy (&timer_table_lock);
}

/* The max simultaneous timer objects count we supported */
#define TIMER_ID_POOL_MAX_SIZE 20480
#define MIN_TIMER_ID 0

static int get_unused_timer_id (void)
{
	int timer_id;

	k_rwlock_wrlock (&timer_table_lock);
	timer_id = bitmap_find_first_zero_bit(timer_id_pool_bits);
	if (timer_id < 0) {
		int new_table_size;
		bitmap_t *new_bits;
		struct timer_node **new_table;

		if (timer_table_size >= TIMER_ID_POOL_MAX_SIZE) {
			k_rwlock_wrunlock (&timer_table_lock);
			return -AOSL_EOVERFLOW;
		}

		new_table_size = timer_table_size + 8;

		new_bits = bitmap_create(new_table_size);
		if (!new_bits) {
			k_rwlock_wrunlock (&timer_table_lock);
			return -AOSL_ENOMEM;
		}

		new_table = (struct timer_node **)aosl_malloc_impl (sizeof (struct timer_node *) * new_table_size);
		if (!new_table) {
			k_rwlock_wrunlock (&timer_table_lock);
			bitmap_destroy(new_bits);
			return -AOSL_ENOMEM;
		}

		bitmap_copy (new_bits, timer_id_pool_bits);
		memcpy (new_table, timer_table, sizeof (struct timer_node *) * timer_table_size);
		memset (new_table + timer_table_size, 0, (new_table_size - timer_table_size) * sizeof (struct timer_node *));


		bitmap_destroy (timer_id_pool_bits);
		aosl_free (timer_table);

		timer_id_pool_bits = new_bits;
		timer_table = new_table;
		timer_table_size = new_table_size;

		timer_id = bitmap_find_first_zero_bit (timer_id_pool_bits);
		BUG_ON (timer_id < 0);
	}

	bitmap_set (timer_id_pool_bits, timer_id);
	k_rwlock_wrunlock (&timer_table_lock);

	return timer_id + MIN_TIMER_ID;
}

static void __put_unused_timer_id (int timer_id)
{
	BUG_ON (timer_id < 0 || timer_id >= timer_table_size);
	k_rwlock_wrlock (&timer_table_lock);
	bitmap_clear (timer_id_pool_bits, timer_id);
	k_rwlock_wrunlock (&timer_table_lock);
}

static __inline__ aosl_timer_t make_timer_obj_id (int16_t timer_id, uint16_t life_id)
{
	return (aosl_timer_t)(((uint32_t)life_id << 16) | timer_id);
}

static __inline__ int16_t get_timer_id (aosl_timer_t timer_obj_id)
{
	return (int16_t)timer_obj_id;
}

static __inline__ uint16_t get_timer_life_id (aosl_timer_t timer_obj_id)
{
	return (uint16_t)((uint32_t)timer_obj_id >> 16);
}

static void __timer_id_install (int timer_id, struct timer_node *timer)
{
	BUG_ON (timer_id < MIN_TIMER_ID);
	BUG_ON (timer_id >= timer_table_size + MIN_TIMER_ID);

	k_rwlock_wrlock (&timer_table_lock);
	if (timer_id - MIN_TIMER_ID < timer_table_size) {
		if (timer_table [timer_id - MIN_TIMER_ID] != NULL)
			abort ();

		timer_table [timer_id - MIN_TIMER_ID] = timer;
		timer->obj_id = make_timer_obj_id (timer_id, __timer_life_id);
		__timer_life_id++;

		/**
		 * 0 is the only invalid life id, so reset it to
		 * 1 if we the life id counter wrapped back.
		 **/
		if (__timer_life_id == 0)
			__timer_life_id = 1;
	}
	k_rwlock_wrunlock (&timer_table_lock);
}

static int __timer_id_uninstall (int timer_id, struct timer_node *timer)
{
	int err;

	BUG_ON (timer_id < MIN_TIMER_ID);
	timer_id -= MIN_TIMER_ID;
	BUG_ON (timer_id >= timer_table_size);

	k_rwlock_wrlock (&timer_table_lock);
	if (timer_table [timer_id] == timer) {
		timer_table [timer_id] = NULL;
		err = 0;
	} else {
		err = -AOSL_EINVAL;
	}
	k_rwlock_wrunlock (&timer_table_lock);
	return err;
}

static __inline__ void timer_dtor (struct timer_node *timer)
{
	if (timer->dtor != NULL)
		timer->dtor (timer->argc, timer->argv);
}

void __free_timer (struct timer_node *timer)
{
	int timer_id = get_timer_id (timer->obj_id);

	timer_dtor (timer);

	/**
	 * We defer the freeing of timer id to this point
	 * just for catching the potential logic errors
	 * that this timer id being allocated by another
	 * timer.
	 * Free it just before we free the timer itself
	 * should be better for these cases.
	 **/
	__put_unused_timer_id (timer_id - MIN_TIMER_ID);
	aosl_free (timer);
}

struct timer_node *timer_get (aosl_timer_t timer_obj_id)
{
	int16_t timer_id = get_timer_id (timer_obj_id);
	struct timer_node *timer;

	if (timer_id < MIN_TIMER_ID)
		return NULL;

	timer_id -= MIN_TIMER_ID;

	k_rwlock_rdlock (&timer_table_lock);
	if (timer_id < timer_table_size) {
		timer = timer_table [timer_id];
		if (timer != NULL) {
			if (timer->obj_id == timer_obj_id) {
				__timer_get (timer);
			} else {
				timer = NULL;
			}
		}
	} else {
		timer = NULL;
	}
	k_rwlock_rdunlock (&timer_table_lock);
	return timer;
}

void timer_put (struct timer_node *timer)
{
	__timer_put (timer);
}

static struct aosl_rb_node **__find_timer_prepare (struct timer_base * base, struct timer_node * timer, struct timer_node ** pprev,
					struct aosl_rb_node **rb_parent)
{
	struct aosl_rb_node **__rb_link = &base->active.rb_node;
	struct aosl_rb_node *__rb_parent, *rb_prev;

	rb_prev = __rb_parent = NULL;

	while (*__rb_link) {
		struct timer_node *entry;

		__rb_parent = *__rb_link;
		entry = aosl_rb_entry (__rb_parent, struct timer_node, timer_node);

		if (entry->expire_time > timer->expire_time) {
			__rb_link = &__rb_parent->rb_left;
		} else {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		}
	}

	if (pprev) {
		if (rb_prev) {
			*pprev = aosl_rb_entry (rb_prev, struct timer_node, timer_node);
		} else {
			*pprev = NULL;
		}
	}

	if (rb_parent)
		*rb_parent = __rb_parent;

	return __rb_link;
}

static __inline__ void __timer_link_list (struct timer_base * base, struct timer_node * timer,
				   struct timer_node * timer_prev, struct aosl_rb_node *rb_parent)
{
	timer->timer_prev = timer_prev;
	if (!timer_prev) {
		if (rb_parent) {
			struct timer_node *timer_next = aosl_rb_entry (rb_parent, struct timer_node, timer_node);

			timer->timer_next = timer_next;
			timer_next->timer_prev = timer;
		} else {
			timer->timer_next = NULL;
		}
		base->first = timer;
	} else {
		timer->timer_next = timer_prev->timer_next;
		if (timer->timer_next)
			timer->timer_next->timer_prev = timer;
		timer_prev->timer_next = timer;
	}
}

static __inline__ void __timer_link_rb (struct timer_base * base, struct timer_node * timer,
				 struct aosl_rb_node **rb_link, struct aosl_rb_node *rb_parent)
{
	rb_link_node (&timer->timer_node, rb_parent, rb_link);
	aosl_rb_insert_color (&timer->timer_node, &base->active);
}

/*
 * __insert_timer - internal function to (re)start a timer
 *
 * The timer is inserted in expiry order. Insertion into the
 * red black tree is O(log(n)).
 */
static void __insert_timer (struct timer_base *base, struct aosl_rb_node *node)
{
	struct timer_node *timer = aosl_rb_entry (node, struct timer_node, timer_node);
	struct aosl_rb_node **link;
	struct timer_node *timer_prev;
	struct aosl_rb_node *parent;

	/*
	 * Find the right place in the rbtree:
	 */
	link = __find_timer_prepare (base, timer, &timer_prev, &parent);

	__timer_link_list (base, timer, timer_prev, parent);
	__timer_link_rb (base, timer, link, parent);
}

static __inline__ void __timer_unlink_list (struct timer_base * base, struct timer_node * timer)
{
	struct timer_node *timer_next = timer->timer_next;

	if (base->first == timer) {
		if (timer_next) {
			timer_next->timer_prev = NULL;
			base->first = timer_next;
		} else {
			base->first = NULL;
		}
	} else {
		timer->timer_prev->timer_next = timer_next;
		if (timer_next)
			timer_next->timer_prev = timer->timer_prev;
	}

	/* this is important for indicating the timer is not scheduled */
	timer->timer_next = AOSL_LIST_POISON1;
	timer->timer_prev = AOSL_LIST_POISON2;
}

static __inline__ void __timer_unlink_rb (struct timer_base * base, struct timer_node * timer)
{
	aosl_rb_erase (&base->active, &timer->timer_node);
}

static __inline__ void __unlink_timer (struct timer_base *base, struct aosl_rb_node *node)
{
	struct timer_node *timer = aosl_rb_entry (node, struct timer_node, timer_node);
	__timer_unlink_list (base, timer);
	__timer_unlink_rb (base, timer);
}

static __inline__ void __sched_timer (struct mp_queue *q, struct timer_node *timer, aosl_ts_t expire_time)
{
	if (expire_time != 0) {
		/* if specified expire_time, then invalidate the timer interval to indicate it is a oneshot timer */
		if (timer->interval != AOSL_INVALID_TIMER_INTERVAL)
			timer->interval = AOSL_INVALID_TIMER_INTERVAL;

		timer->expire_time = expire_time;
	} else {
		if (timer->interval != AOSL_INVALID_TIMER_INTERVAL) {
			timer->expire_time = aosl_tick_now () + timer->interval;
		} else {
			timer->expire_time = (aosl_ts_t)(int64_t)-1;
		}
	}

	__insert_timer (&q->timer_base, &timer->timer_node);
}

static void __resched_timer (struct mp_queue *q, struct timer_node *timer, uintptr_t interval, aosl_ts_t expire_time)
{
	if (timer->timer_next != AOSL_LIST_POISON1)
		__unlink_timer (&q->timer_base, &timer->timer_node);

	if (expire_time == 0 && interval != AOSL_INVALID_TIMER_INTERVAL)
		timer->interval = interval;

	__sched_timer (q, timer, expire_time);
}

static __inline__ void __cancel_timer_on_q (struct mp_queue *q, struct timer_node *timer)
{
	if (timer->timer_next != AOSL_LIST_POISON1)
		__unlink_timer (&q->timer_base, &timer->timer_node);
}

static struct timer_node *__create_timer_on_q (struct mp_queue *q, uintptr_t interval,
		aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, uintptr_t argv [])
{
	struct timer_node *timer;
	size_t extra = argc * sizeof (uintptr_t);
	uintptr_t l;
	int timer_id;

	timer = (struct timer_node *)aosl_malloc (sizeof *timer + extra);
	if (timer == NULL)
		return ERR_PTR (-AOSL_ENOMEM);

	timer_id = get_unused_timer_id ();
	if (timer_id < 0) {
		aosl_free (timer);
		return ERR_PTR (timer_id);
	}

	/* this is important for indicating the timer is not scheduled */
	timer->timer_next = AOSL_LIST_POISON1;
	timer->timer_prev = AOSL_LIST_POISON2;

	timer->obj_id = AOSL_MPQ_TIMER_INVALID;
	atomic_set (&timer->usage, 1);

	timer->q = q->qid;

	timer->interval = interval;
	timer->expire_time = ULLONG_MAX;
	timer->func = func;
	timer->dtor = dtor;

	timer->argc = argc;
	for (l = 0; l < argc; l++)
		timer->argv [l] = argv [l];

	aosl_list_add_tail (&timer->node, &q->timers);
	q->timer_count++;
	__timer_id_install (timer_id, timer);
	return timer;
}

static int __kill_timer_on_q (struct mp_queue *q, struct timer_node *timer)
{
	int err;
	err = __timer_id_uninstall (get_timer_id (timer->obj_id), timer);
	if (err < 0)
		return err;

	__cancel_timer_on_q (q, timer);

	aosl_list_del (&timer->node);
	q->timer_count--;
	__timer_put (timer);
	return 0;
}

void mpq_init_timers (struct mp_queue *q)
{
	aosl_rb_root_init (&q->timer_base.active, NULL /* we keep the timer using the raw mechanism */);
	q->timer_base.first = NULL;
	aosl_list_head_init (&q->timers);
	q->timer_count = 0;
}

void mpq_fini_timers (struct mp_queue *q)
{
	struct timer_node *timer;
	struct aosl_list_head *node;

	/* free the active fds */
	while ((node = aosl_list_head (&q->timers))) {
		timer = aosl_list_entry (node, struct timer_node, node);
		__kill_timer_on_q (q, timer);
	}

	q->timer_count = 0;
}

int __check_and_run_timers (struct mp_queue *q)
{
	struct timer_node *timer;
	struct timer_base *base = &q->timer_base;
	aosl_ts_t now = aosl_tick_now ();
	int count = 0;

	while ((timer = base->first) && time_after_eq (now, timer->expire_time)) {
		__unlink_timer (&q->timer_base, &timer->timer_node);

		/* All oneshot timers must have invalid interval */
		if (timer->interval != AOSL_INVALID_TIMER_INTERVAL) {
#ifdef PERIODIC_TIMER_NO_DELAY_ACCUMULATION
			if (timer->interval > 0) {
				/**
				 * Keep the timer more accurate when the interval > 0,
				 * and do not use 'now + interval' here.
				 **/
				timer->expire_time += timer->interval;
			} else {
				/**
				 * If the timer interval is 0, we should use the current
				 * ticks as the expire time, otherwise the expire time
				 * would keep the same value with previous, please pay
				 * attention to the integer wrapping cases generally,
				 * although we are using a 64bit integer now.
				 * =====================================================
				 * We MUST NOT use 'now' as the expire time in this case
				 * because a dead loop here is not our expectation, so
				 * get the new current tick count again then the loop
				 * here would only be a 1ms time span instead of a dead
				 * loop.
				 **/
				timer->expire_time = aosl_tick_now ();
			}
#else
			/**
			 * If we just add the timer interval to the expire time for
			 * a periodic timer, then we might got too many invocations
			 * once one call delays some extra time, so we changed back
			 * to this way for the expire time.
			 **/
		    //	timer->expire_time = aosl_tick_now () + timer->interval;
		    timer->expire_time = aosl_tick_now () + timer->interval - (now - timer->expire_time);
#endif

			__insert_timer (&q->timer_base, &timer->timer_node);
		}

		timer->func (timer->obj_id, (const aosl_ts_t *)&now, timer->argc, timer->argv);
		mpq_stack_fini (q->q_stack_curr);
		count++;
	}

	return count;
}

__export_in_so__ aosl_timer_t aosl_mpq_create_timer (uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t *argv;
	uintptr_t l;
	struct timer_node *timer;

	if ((intptr_t)interval < 0) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (func == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return AOSL_MPQ_TIMER_INVALID;
	}

	q = __get_or_create_current ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);
	va_end (args);

	timer = __create_timer_on_q (q, interval, func, dtor, argc, argv);
	if (IS_ERR (timer))
		return_err (timer);

	return timer->obj_id;
}

static void ____target_q_create_timer (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	struct timer_node **timer_p = (struct timer_node **)argv [0];
	uintptr_t interval = argv [1];
	aosl_timer_func_t func = (aosl_timer_func_t)argv [2];
	aosl_obj_dtor_t dtor = (aosl_obj_dtor_t)argv [3];

	UNUSED (queued_ts_p);
	UNUSED (robj);

	*timer_p = __create_timer_on_q (THIS_MPQ (), interval, func, dtor, argc - 4, &argv [4]);
}

static aosl_timer_t mpq_create_timer_on_q_args (aosl_mpq_t qid, uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, va_list args)
{
	struct mp_queue *q;
	struct timer_node *timer;
	uintptr_t *argv;
	uintptr_t l;

	if (func == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return AOSL_MPQ_TIMER_INVALID;
	}

	q = __mpq_get_or_this (qid);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * (4 + argc));
	argv [0] = (uintptr_t)&timer;
	argv [1] = (uintptr_t)interval;
	argv [2] = (uintptr_t)func;
	argv [3] = (uintptr_t)dtor;
	for (l = 0; l < argc; l++)
		argv [4 + l] = va_arg (args, uintptr_t);

	if (__mpq_call_argv (q, -1, "____target_q_set_timer", ____target_q_create_timer, 4 + argc, argv) < 0)
		timer = ERR_PTR (-aosl_errno);

	__mpq_put_or_this (q);

	if (IS_ERR (timer))
		return_err (timer);

	return timer->obj_id;
}

__export_in_so__ aosl_timer_t aosl_mpq_create_timer_on_q (aosl_mpq_t qid, uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	va_list args;
	aosl_timer_t timer_id;

	if ((intptr_t)interval < 0) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	va_start (args, argc);
	timer_id = mpq_create_timer_on_q_args (qid, interval, func, dtor, argc, args);
	va_end (args);

	return timer_id;
}

__export_in_so__ aosl_timer_t aosl_mpq_create_oneshot_timer (aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t *argv;
	uintptr_t l;
	struct timer_node *timer;

	if (func == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return AOSL_MPQ_TIMER_INVALID;
	}

	q = __get_or_create_current ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);
	va_end (args);

	timer = __create_timer_on_q (q, AOSL_INVALID_TIMER_INTERVAL, func, dtor, argc, argv);
	if (IS_ERR (timer))
		return_err (timer);

	return timer->obj_id;
}

__export_in_so__ aosl_timer_t aosl_mpq_create_oneshot_timer_on_q (aosl_mpq_t qid, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	va_list args;
	aosl_timer_t timer_id;

	va_start (args, argc);
	timer_id = mpq_create_timer_on_q_args (qid, AOSL_INVALID_TIMER_INTERVAL, func, dtor, argc, args);
	va_end (args);

	return timer_id;
}

static struct timer_node *__set_timer_on_q (struct mp_queue *q, uintptr_t interval, aosl_ts_t expire_time,
							aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, uintptr_t argv [])
{
	struct timer_node *timer;

	timer = __create_timer_on_q (q, interval, func, dtor, argc , argv);
	if (!IS_ERR (timer))
		__sched_timer (q, timer, expire_time);

	return timer;
}

__export_in_so__ aosl_timer_t aosl_mpq_set_timer (uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t *argv;
	uintptr_t l;
	struct timer_node *timer;

	if ((intptr_t)interval < 0) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (func == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return AOSL_MPQ_TIMER_INVALID;
	}

	q = __get_or_create_current ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);
	va_end (args);

	timer = __set_timer_on_q (q, interval, 0, func, dtor, argc, argv);
	if (IS_ERR (timer))
		return_err (timer);

	return timer->obj_id;
}

static void ____target_q_set_timer (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	struct timer_node **timer_p = (struct timer_node **)argv [0];
	uintptr_t interval = argv [1];
	aosl_ts_t *expire_time_p = (aosl_ts_t *)argv [2];
	aosl_timer_func_t func = (aosl_timer_func_t)argv [3];
	aosl_obj_dtor_t dtor = (aosl_obj_dtor_t)argv [4];

	UNUSED (queued_ts_p);
	UNUSED (robj);

	*timer_p = __set_timer_on_q (THIS_MPQ (), interval, expire_time_p ? *expire_time_p : 0, func, dtor, argc - 5, &argv [5]);
}

static aosl_timer_t mpq_set_timer_on_q_args (aosl_mpq_t qid, uintptr_t interval, aosl_ts_t *expire_time_p, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, va_list args)
{
	struct mp_queue *q;
	struct timer_node *timer;
	uintptr_t *argv;
	uintptr_t l;

	if (func == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return AOSL_MPQ_TIMER_INVALID;
	}

	q = __mpq_get_or_this (qid);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * (5 + argc));
	argv [0] = (uintptr_t)&timer;
	argv [1] = (uintptr_t)interval;
	argv [2] = (uintptr_t)expire_time_p;
	argv [3] = (uintptr_t)func;
	argv [4] = (uintptr_t)dtor;
	for (l = 0; l < argc; l++)
		argv [5 + l] = va_arg (args, uintptr_t);

	if (__mpq_call_argv (q, -1, "____target_q_set_timer", ____target_q_set_timer, 5 + argc, argv) < 0)
		timer = ERR_PTR (-aosl_errno);

	__mpq_put_or_this (q);

	if (IS_ERR (timer))
		return_err (timer);

	return timer->obj_id;
}

__export_in_so__ aosl_timer_t aosl_mpq_set_timer_on_q (aosl_mpq_t qid, uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	va_list args;
	aosl_timer_t timer_id;

	if ((intptr_t)interval < 0) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	va_start (args, argc);
	timer_id = mpq_set_timer_on_q_args (qid, interval, NULL, func, dtor, argc, args);
	va_end (args);

	return timer_id;
}

__export_in_so__ aosl_timer_t aosl_mpq_set_oneshot_timer (aosl_ts_t expire_time, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t *argv;
	uintptr_t l;
	struct timer_node *timer;

	if (func == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (argc > MPQ_ARGC_MAX) {
		aosl_errno = AOSL_E2BIG;
		return AOSL_MPQ_TIMER_INVALID;
	}

	if (expire_time == 0) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	q = __get_or_create_current ();
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return AOSL_MPQ_TIMER_INVALID;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);
	va_end (args);

	timer = __set_timer_on_q (q, AOSL_INVALID_TIMER_INTERVAL, expire_time, func, dtor, argc, argv);
	if (IS_ERR (timer))
		return_err (timer);

	return timer->obj_id;
}

__export_in_so__ aosl_timer_t aosl_mpq_set_oneshot_timer_on_q (aosl_mpq_t qid, aosl_ts_t expire_time, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...)
{
	va_list args;
	aosl_timer_t timer_id;

	va_start (args, argc);
	timer_id = mpq_set_timer_on_q_args (qid, AOSL_INVALID_TIMER_INTERVAL, &expire_time, func, dtor, argc, args);
	va_end (args);

	return timer_id;
}

__export_in_so__ int aosl_mpq_timer_interval (aosl_timer_t timer_id, uintptr_t *interval_p)
{
	struct timer_node *timer = timer_get (timer_id);
	if (timer != NULL) {
		if (interval_p != NULL)
			*interval_p = timer->interval;

		timer_put (timer);
		return 0;
	}

	aosl_errno = AOSL_ENOENT;
	return -1;
}

__export_in_so__ int aosl_mpq_timer_active (aosl_timer_t timer_id, int *active_p)
{
	struct timer_node *timer = timer_get (timer_id);
	if (timer != NULL) {
		if (active_p != NULL)
			*active_p = (int)(timer->timer_next != AOSL_LIST_POISON1);

		timer_put (timer);
		return 0;
	}

	aosl_errno = AOSL_ENOENT;
	return -1;
}

static void ____target_q_resched_timer (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	struct timer_node *timer = (struct timer_node *)argv [0];
	uintptr_t interval = argv [1];
	aosl_ts_t *expire_time_p = (aosl_ts_t *)argv [2];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	__resched_timer (THIS_MPQ (), timer, interval, expire_time_p ? *expire_time_p : 0);
}

static int mpq_resched_timer (struct timer_node *timer, uintptr_t interval, aosl_ts_t *expire_time_p)
{
	struct mp_queue *q;

	q = __mpq_get_or_this (timer->q);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}


	if (__mpq_call (q, -1, "____target_q_resched_timer", ____target_q_resched_timer, 3, timer, interval, expire_time_p) < 0) {
		__mpq_put_or_this (q);
		return -1;
	}

	__mpq_put_or_this (q);
	return 0;
}

__export_in_so__ int aosl_mpq_resched_timer (aosl_timer_t timer_id, uintptr_t interval)
{
	struct timer_node *timer;
	int err;

	timer = timer_get (timer_id);
	if (timer == NULL) {
		aosl_errno = AOSL_ENOENT;
		return -1;
	}

	if ((intptr_t)interval < 0) {
		interval = timer->interval;
		if ((intptr_t)interval < 0) {
			aosl_errno = AOSL_EINVAL;
			err = -1;
			goto __put_timer;
		}
	}

	err = mpq_resched_timer (timer, interval, NULL);

__put_timer:
	timer_put (timer);
	return err;
}

__export_in_so__ int aosl_mpq_resched_oneshot_timer (aosl_timer_t timer_id, aosl_ts_t expire_time)
{
	struct timer_node *timer;
	int err;

	if (expire_time == 0) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	timer = timer_get (timer_id);
	if (timer == NULL) {
		aosl_errno = AOSL_ENOENT;
		return -1;
	}

	err = mpq_resched_timer (timer, AOSL_INVALID_TIMER_INTERVAL, &expire_time);
	timer_put (timer);
	return err;
}

static void ____target_q_cancel_timer (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	struct timer_node *timer = (struct timer_node *)argv [0];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	__cancel_timer_on_q (THIS_MPQ (), timer);
}

static int mpq_cancel_timer (struct timer_node *timer)
{
	struct mp_queue *q;

	q = __mpq_get_or_this (timer->q);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (__mpq_call (q, -1, "____target_q_cancel_timer", ____target_q_cancel_timer, 1, timer) < 0) {
		__mpq_put_or_this (q);
		return -1;
	}

	__mpq_put_or_this (q);
	return 0;
}

__export_in_so__ int aosl_mpq_cancel_timer (aosl_timer_t timer_id)
{
	struct timer_node *timer = timer_get (timer_id);
	int err;

	if (timer == NULL) {
		aosl_errno = AOSL_ENOENT;
		return -1;
	}

	err = mpq_cancel_timer (timer);
	timer_put (timer);
	return err;
}

static void ____target_q_kill_timer (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	struct mp_queue *q = THIS_MPQ ();
	struct timer_node *timer = (struct timer_node *)argv [0];
	int *err_p = (int *)argv [1];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	*err_p = __kill_timer_on_q (q, timer);
}

static int mpq_kill_timer (struct timer_node *timer)
{
	struct mp_queue *q;
	int err;

	q = __mpq_get_or_this (timer->q);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (__mpq_call (q, -1, "____target_q_kill_timer", ____target_q_kill_timer, 2, timer, &err) < 0) {
		__mpq_put_or_this (q);
		return -1;
	}

	__mpq_put_or_this (q);
	return_err (err);
}

__export_in_so__ int aosl_mpq_kill_timer (aosl_timer_t timer_id)
{
	struct timer_node *timer = timer_get (timer_id);
	int err;

	if (timer == NULL) {
		aosl_errno = AOSL_ENOENT;
		return -1;
	}

	err = mpq_kill_timer (timer);
	timer_put (timer);
	return err;
}

__export_in_so__ int aosl_mpq_timer_arg (aosl_timer_t timer_id, uintptr_t n, uintptr_t *arg_p)
{
	struct timer_node *timer = timer_get (timer_id);
	if (timer != NULL) {
		int err = -AOSL_EINVAL;

		if (n < timer->argc) {
			if (arg_p != NULL)
				*arg_p = timer->argv [n];

			err = 0;
		}

		timer_put (timer);
		return_err (err);
	}

	aosl_errno = AOSL_ENOENT;
	return -1;
}
