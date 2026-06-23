/***************************************************************************
 * Module:	AOSL threading relative internal implementations.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <kernel/thread.h>

#ifdef CONFIG_AOSL_COND

#include <stdio.h>
#include <string.h>
#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/err.h>
#include <api/aosl_mm.h>
#include <api/aosl_thread.h>
#include <api/aosl_time.h>
#include <hal/aosl_hal_thread.h>

void k_cond_init (k_cond_t *cond)
{
	k_lock_init (&cond->lock);
	aosl_list_head_init (&cond->wait_list);
}

struct cond_waiter {
	struct aosl_list_head list;
	aosl_sem_t event;
};

void k_cond_signal (k_cond_t *cond)
{
	k_lock_lock (&cond->lock);
	if (!aosl_list_empty (&cond->wait_list)) {
		struct cond_waiter *waiter = aosl_list_entry (cond->wait_list.next, struct cond_waiter, list);
		aosl_hal_sem_post (waiter->event);
	}
	k_lock_unlock (&cond->lock);
}

void k_cond_broadcast (k_cond_t *cond)
{
	struct cond_waiter *waiter;

	k_lock_lock (&cond->lock);
	aosl_list_for_each_entry_t (struct cond_waiter, waiter, &cond->wait_list, list)
	aosl_hal_sem_post (waiter->event);
	k_lock_unlock (&cond->lock);
}

static int __k_cond_wait_with_timeo (k_cond_t *cond, k_lock_t *lock, intptr_t timeo)
{
	struct cond_waiter waiter = {0};
	int err;

	k_lock_lock (&cond->lock);
	/* now we can release the user provided mutex lock. */
	k_lock_unlock (lock);
	aosl_list_add_tail (&waiter.list, &cond->wait_list);
	waiter.event = aosl_hal_sem_create ();
	k_lock_unlock (&cond->lock);
	if (waiter.event == NULL)
		abort ();

	if (timeo < 0)
		timeo = (intptr_t)-1;

	err = aosl_hal_sem_timedwait (waiter.event, timeo);

	/**
	 * We must acquire the user provided mutex lock first, and
	 * then our own cond->lock, otherwise, there would lead to
	 * the standard dead lock case.
	 **/
	k_lock_lock (lock);
	k_lock_lock (&cond->lock);
	__aosl_list_del_entry (&waiter.list);
	k_lock_unlock (&cond->lock);
	aosl_hal_sem_destroy (waiter.event);

	return err;
}

void k_cond_wait (k_cond_t *cond, k_lock_t *lock)
{
	__k_cond_wait_with_timeo (cond, lock, (intptr_t)-1);
}

int k_cond_timedwait (k_cond_t *cond, k_lock_t *lock, intptr_t timeo)
{
  int ret = __k_cond_wait_with_timeo (cond, lock, timeo);
  if (ret == 0) {
    return 0;
  } else {
    return ret;
  }
}

void k_cond_destroy (k_cond_t *cond)
{
	/**
	 * A correct program must make sure that no waiters are blocked on the condvar
	 * when it is destroyed, and that there are no concurrent signals or
	 * broadcasts.  To wake waiters reliably, the program must signal or
	 * broadcast while holding the mutex or after having held the mutex.  It must
	 * also ensure that no signal or broadcast are still pending to unblock
	 * waiters; IOW, because waiters can wake up spuriously, the program must
	 * effectively ensure that destruction happens after the execution of those
	 * signal or broadcast calls.
	 * Thus, we can assume that all waiters that are still accessing the condvar
	 * have been woken.  We wait until they have confirmed to have woken up.
	 **/
	while (!aosl_list_empty_careful (&cond->wait_list))
		aosl_msleep (1);

	k_lock_destroy (&cond->lock);
}

#endif /* CONFIG_AOSL_COND */