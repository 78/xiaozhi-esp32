/***************************************************************************
 * Module:	rwlock implementation for Microsoft Windows
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <kernel/compiler.h>
#include <kernel/list.h>
#include <kernel/thread.h>
#include <kernel/rwlock.h>
#include <api/aosl_atomic.h>

void rwlock_init (k_raw_rwlock_t *rw)
{
	rw->count = RWLOCK_UNLOCKED_VALUE;
	rw->wait_lock = aosl_hal_mutex_create();
	aosl_list_head_init (&rw->wait_list);
}

enum rwlock_waiter_type {
	RWLOCK_WAITING_FOR_WRITE,
	RWLOCK_WAITING_FOR_READ
};

struct rwlock_waiter {
	struct aosl_list_head list;
	k_event_t *event;
	enum rwlock_waiter_type type;
};

enum rwlock_wake_type {
	RWLOCK_WAKE_ANY,		/* Wake whatever's at head of wait list */
	RWLOCK_WAKE_READERS,	/* Wake readers only */
	RWLOCK_WAKE_READ_OWNED	/* Waker thread holds the read lock */
};

static void __rwlock_do_wake (k_raw_rwlock_t *rw, enum rwlock_wake_type wake_type)
{
	struct rwlock_waiter *waiter;
	k_event_t *event;
	struct aosl_list_head *next;
	intptr_t oldcount, woken, loop, adjustment;

	waiter = aosl_list_entry(rw->wait_list.next, struct rwlock_waiter, list);
	if (waiter->type == RWLOCK_WAITING_FOR_WRITE) {
		if (wake_type == RWLOCK_WAKE_ANY)
			k_event_pulse (waiter->event);

		return;
	}

	adjustment = 0;
	if (wake_type != RWLOCK_WAKE_READ_OWNED) {
		adjustment = RWLOCK_ACTIVE_READ_BIAS;
 try_reader_grant:
		oldcount = rwlock_atomic_update(adjustment, rw) - adjustment;
		if (oldcount < RWLOCK_WAITING_BIAS) {
			if (rwlock_atomic_update(-adjustment, rw) & RWLOCK_ACTIVE_MASK)
				return;

			goto try_reader_grant;
		}
	}

	woken = 0;
	do {
		woken++;

		if (waiter->list.next == &rw->wait_list)
			break;

		waiter = aosl_list_entry(waiter->list.next, struct rwlock_waiter, list);
	} while (waiter->type != RWLOCK_WAITING_FOR_WRITE);

	adjustment = woken * RWLOCK_ACTIVE_READ_BIAS - adjustment;
	if (waiter->type != RWLOCK_WAITING_FOR_WRITE)
		adjustment -= RWLOCK_WAITING_BIAS;

	if (adjustment)
		rwlock_atomic_add(adjustment, rw);

	next = rw->wait_list.next;
	loop = woken;
	do {
		waiter = aosl_list_entry(next, struct rwlock_waiter, list);
		next = waiter->list.next;
		event = waiter->event;
		aosl_wmb();
		waiter->event = NULL;
		k_event_pulse (event);
	} while (--loop);

	/**
	 * Removed the woken up threads from the list here,
	 * so no need to delete the list node in the target
	 * thread again.
	 **/
	rw->wait_list.next = next;
	next->prev = &rw->wait_list;
}

void rwlock_read_lock_failed (intptr_t *rw_count)
{
	k_raw_rwlock_t *rw = container_of (rw_count, k_raw_rwlock_t, count);
	intptr_t count, adjustment = -RWLOCK_ACTIVE_READ_BIAS;
	k_event_t event;
	struct rwlock_waiter waiter;

	k_event_init (&event);

	/* set up my own style of waitqueue */
	waiter.event = &event;
	waiter.type = RWLOCK_WAITING_FOR_READ;

	aosl_hal_mutex_lock (rw->wait_lock);

	if (aosl_list_empty (&rw->wait_list))
		adjustment += RWLOCK_WAITING_BIAS;

	aosl_list_add_tail (&waiter.list, &rw->wait_list);

	/* we're now waiting on the lock, but no longer actively locking */
	count = rwlock_atomic_update (adjustment, rw);

	if (count == RWLOCK_WAITING_BIAS || (count > RWLOCK_WAITING_BIAS && adjustment != -RWLOCK_ACTIVE_READ_BIAS))
		__rwlock_do_wake (rw, RWLOCK_WAKE_ANY);

	aosl_hal_mutex_unlock (rw->wait_lock);

	/* wait to be given the lock */
	for (;;) {
		aosl_rmb ();
		if (waiter.event == NULL)
			break;

		k_event_wait (&event);
	}

	k_event_destroy (&event);
}

void rwlock_write_lock_failed (intptr_t *rw_count)
{
	k_raw_rwlock_t *rw = container_of (rw_count, k_raw_rwlock_t, count);
	intptr_t count, adjustment = -RWLOCK_ACTIVE_WRITE_BIAS;
	k_event_t event;
	struct rwlock_waiter waiter;

	k_event_init (&event);

	/* set up my own style of waitqueue */
	waiter.event = &event;
	waiter.type = RWLOCK_WAITING_FOR_WRITE;

	aosl_hal_mutex_lock (rw->wait_lock);

	if (aosl_list_empty(&rw->wait_list))
		adjustment += RWLOCK_WAITING_BIAS;

	aosl_list_add_tail (&waiter.list, &rw->wait_list);

	/* we're now waiting on the lock, but no longer actively locking */
	count = rwlock_atomic_update(adjustment, rw);

	if (count > RWLOCK_WAITING_BIAS && adjustment == -RWLOCK_ACTIVE_WRITE_BIAS)
		__rwlock_do_wake(rw, RWLOCK_WAKE_READERS);

	/* wait until we successfully acquire the lock */
	for (;;) {
		if (!(count & RWLOCK_ACTIVE_MASK)) {
			/* Try acquiring the write lock. */
			count = RWLOCK_ACTIVE_WRITE_BIAS;
			if (!aosl_list_is_singular (&rw->wait_list))
				count += RWLOCK_WAITING_BIAS;

			if (rw->count == RWLOCK_WAITING_BIAS && atomic_intptr_cmpxchg (&rw->count, RWLOCK_WAITING_BIAS, count) == RWLOCK_WAITING_BIAS)
				break;
		}

		aosl_hal_mutex_unlock (rw->wait_lock);
		k_event_wait (&event);
		aosl_hal_mutex_lock (rw->wait_lock);
		count = rw->count;
	}

	__aosl_list_del_entry (&waiter.list);
	aosl_hal_mutex_unlock (rw->wait_lock);

	k_event_destroy (&event);
}

void rwlock_wake (intptr_t *rw_count)
{
	k_raw_rwlock_t *rw = container_of (rw_count, k_raw_rwlock_t, count);

	aosl_hal_mutex_lock (rw->wait_lock);

	/* do nothing if list empty */
	if (!aosl_list_empty (&rw->wait_list))
		__rwlock_do_wake(rw, RWLOCK_WAKE_ANY);

	aosl_hal_mutex_unlock (rw->wait_lock);
}

void rwlock_downgrade_wake (intptr_t *rw_count)
{
	k_raw_rwlock_t *rw = container_of (rw_count, k_raw_rwlock_t, count);

	aosl_hal_mutex_lock (rw->wait_lock);

	/* do nothing if list empty */
	if (!aosl_list_empty(&rw->wait_list))
		__rwlock_do_wake(rw, RWLOCK_WAKE_READ_OWNED);

	aosl_hal_mutex_unlock (rw->wait_lock);
}