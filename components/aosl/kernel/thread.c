/***************************************************************************
 * Module:	AOSL threading relative internal implementations.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <kernel/compiler.h>
#include <kernel/kernel.h>
#include <kernel/err.h>
#include <kernel/types.h>
#include <kernel/thread.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_thread.h>
#include <api/aosl_time.h>
#include <hal/aosl_hal_thread.h>

#define UNUSED(expr) (void)(expr)

void os_thread_init (void)
{
	rb_tls_init ();
}

void os_thread_fini (void)
{
	rb_tls_fini ();
}

static void *k_os_thread_entry (void *arg)
{
	k_thread_create_args_t *args = (k_thread_create_args_t *)arg;
	k_thread_entry_t entry;

	// set thread name
	if (args->name != NULL) {
		size_t namelen;
		char __thread_name[THREAD_NAME_LEN];
		const char *thread_name;

		namelen = strlen (args->name);
		if (namelen < THREAD_NAME_LEN) {
			thread_name = args->name;
		} else {
			snprintf (__thread_name, THREAD_NAME_LEN, "%s", args->name);
			thread_name = __thread_name;
		}

		aosl_hal_thread_set_name (thread_name);
	}

	// set thread priority
	if (args->pri >= AOSL_THRD_PRI_LOW && args->pri <= AOSL_THRD_PRI_RT)
		aosl_hal_thread_set_priority (args->pri);

	entry = args->entry;
	arg = args->arg;

	k_lock_lock (args->lock);
	args->done = 1;
	k_cond_signal (args->cond);
	k_lock_unlock (args->lock);

	entry (arg);

	return NULL;
}

int k_thread_create (k_thread_t *thread, const char *name, int priority, int stack_size,
										 k_thread_entry_t entry, void *arg)
{
	aosl_thread_param_t param = {0};
	k_thread_create_args_t args;
	k_lock_t lock;
	k_cond_t cond;
	int err;

	if (priority < AOSL_THRD_PRI_DEFAULT || priority > AOSL_THRD_PRI_RT) {
		return -AOSL_EINVAL;
	}
	if (stack_size == 0) {
		stack_size = THREAD_STACK_SIZE;
	}
	args.name = name;
	args.entry = entry;
	args.arg = arg;
	args.pri = priority;
	args.stack_size = stack_size;
	args.done = 0;

	k_lock_init (&lock);
	k_cond_init (&cond);
	args.lock = &lock;
	args.cond = &cond;

	param.name = name;
	param.priority = (aosl_thread_proiority_e)priority;
	param.stack_size = stack_size;

	err = aosl_hal_thread_create(thread, &param, k_os_thread_entry, &args);
	if (err < 0) {
		aosl_hal_set_error(err);
	} else if (err == 0) {
		aosl_hal_thread_detach(*thread);

		k_lock_lock (&lock);
		while (!args.done)
			k_cond_wait (&cond, &lock);
		k_lock_unlock (&lock);
	}

	k_lock_destroy (&lock);
	k_cond_destroy (&cond);
	return err;
}

k_thread_t k_thread_self (void)
{
  return aosl_hal_thread_self();
}

void k_thread_exit (void *retval)
{
	aosl_hal_thread_exit(retval);
}

void k_lock_init (k_lock_t *lk)
{
  lk->mutex = aosl_hal_mutex_create();
}

void k_lock_init_recursive (k_lock_t *lk)
{
  UNUSED (lk);
  abort();
}

void k_lock_lock (k_lock_t *lk)
{
	aosl_hal_mutex_lock(lk->mutex);
}

// 1: success, 0: failed
int k_lock_trylock (k_lock_t *lk)
{
	if (!lk->mutex) {
		return 0;
	}

	if (aosl_hal_mutex_trylock (lk->mutex) == 0) {
		return 1;
	}

	return 0;
}

void k_lock_unlock (k_lock_t *lk)
{
	aosl_hal_mutex_unlock (lk->mutex);
}

void k_lock_destroy (k_lock_t *lk)
{
	if (lk && lk->mutex) {
		aosl_hal_mutex_destroy (lk->mutex);
		lk->mutex = NULL;
	}
}

#ifndef CONFIG_AOSL_COND
void k_cond_init (k_cond_t *cond)
{
  aosl_cond_t ret = aosl_hal_cond_create();
  cond->condval = ret;
}

void k_cond_signal (k_cond_t* cond)
{
	aosl_hal_cond_signal(cond->condval);
}

void k_cond_broadcast (k_cond_t* cond)
{
	aosl_hal_cond_broadcast (cond->condval);
}

void k_cond_wait (k_cond_t *cond, k_lock_t *lock)
{
	aosl_hal_cond_wait (cond->condval, lock->mutex);
}

int k_cond_timedwait (k_cond_t *cond, k_lock_t *lock, intptr_t timeo)
{
  return aosl_hal_cond_timedwait (cond->condval, lock->mutex, timeo);
}

void k_cond_destroy (k_cond_t *cond)
{
	aosl_hal_cond_destroy (cond->condval);
	cond->condval = NULL;
}
#endif

void k_rwlock_init (k_rwlock_t *rw)
{
	k_lock_init (&rw->lk);
	rw->rd2wrlock_count = 0;
	k_raw_rwlock_init (&rw->rw);
}

void k_rwlock_rdlock (k_rwlock_t *rw)
{
	k_raw_rwlock_rdlock (&rw->rw);
}

int k_rwlock_tryrdlock (k_rwlock_t *rw)
{
	return k_raw_rwlock_tryrdlock (&rw->rw);
}

void k_rwlock_wrlock (k_rwlock_t *rw)
{
	for (;;) {
		k_lock_lock (&rw->lk);
		if (k_raw_rwlock_trywrlock (&rw->rw))
			break;

		/**
		 * The rw lock readers hold the read
		 * lock now, and they might want to
		 * change to write lock for doing
		 * some writing operations and then
		 * back to read lock again, but the
		 * locking order in this case would
		 * be hold the read lock first and
		 * then try to lock the rw->lk, but
		 * the expected locking order should
		 * be locking the rw->lk first, so
		 * if we try to write lock rw->rw
		 * failed here, we must release the
		 * rw->lk to give a chance to the
		 * read lock holders for locking
		 * rw->lk first and then release
		 * the read lock of rw->rw!
		 **/
		k_lock_unlock (&rw->lk);
		/* Release the CPU 10us */
		aosl_msleep (1);
	}
}

int k_rwlock_trywrlock (k_rwlock_t *rw)
{
	int ret;
	k_lock_lock (&rw->lk);
	ret = k_raw_rwlock_trywrlock (&rw->rw);
	if (!ret)
		k_lock_unlock (&rw->lk);

	return ret;
}

void k_rwlock_rdunlock (k_rwlock_t *rw)
{
	k_raw_rwlock_rdunlock (&rw->rw);
}

void k_rwlock_wrunlock (k_rwlock_t *rw)
{
	k_raw_rwlock_wrunlock (&rw->rw);
	k_lock_unlock (&rw->lk);
}

void k_rwlock_rd2wrlock (k_rwlock_t *rw)
{
	for (;;) {
		if (k_lock_trylock (&rw->lk))
			break;

		if (rw->rd2wrlock_count != 0) {
			/**
			 * We do not allow more than one thread
			 * try to rd2wr lock the rw lock at any
			 * time, PLEASE GUARANTEE this logic in
			 * the using logic!!
			 **/
			abort ();
		}

		/* Release the CPU 1ms */
		aosl_msleep (1);
	}

	rw->rd2wrlock_count = 1;
	k_raw_rwlock_rdunlock (&rw->rw);
	k_raw_rwlock_wrlock (&rw->rw);
}

void k_rwlock_wr2rdlock (k_rwlock_t *rw)
{
	k_raw_rwlock_wrunlock (&rw->rw);
	k_raw_rwlock_rdlock (&rw->rw);
	rw->rd2wrlock_count = 0;
	k_lock_unlock (&rw->lk);
}

void k_rwlock_destroy (k_rwlock_t *rw)
{
	k_lock_destroy (&rw->lk);
	k_raw_rwlock_destroy (&rw->rw);
}


#define K_EVENT_SET ((void *)(uintptr_t)0x5f534554) /* "_SET" */
#define K_EVENT_PULSE ((void *)(uintptr_t)0x50554c53) /* "PULS" */

void k_event_init (k_event_t *event)
{
	k_lock_init (&event->mutex);
	k_cond_init (&event->cond);
	event->result = NULL;
}

void k_event_set (k_event_t *event)
{
	k_lock_lock (&event->mutex);
	if (event->result != K_EVENT_SET) {
		event->result = K_EVENT_SET;
		k_cond_broadcast (&event->cond);
	}
	k_lock_unlock (&event->mutex);
}

void k_event_pulse (k_event_t *event)
{
	k_lock_lock (&event->mutex);
	if (event->result != K_EVENT_PULSE) {
		event->result = K_EVENT_PULSE;
		k_cond_signal (&event->cond);
	}
	k_lock_unlock (&event->mutex);
}

void k_event_wait (k_event_t *event)
{
	k_lock_lock (&event->mutex);
	if (event->result != K_EVENT_SET && event->result != K_EVENT_PULSE)
		k_cond_wait (&event->cond, &event->mutex);

	if (event->result == K_EVENT_PULSE)
		event->result = NULL;
	k_lock_unlock (&event->mutex);
}

int k_event_timedwait (k_event_t *event, intptr_t timeo)
{
	int ret = 0;
	k_lock_lock (&event->mutex);
	if (event->result != K_EVENT_SET && event->result != K_EVENT_PULSE) {
		if (timeo < 0) {
			k_cond_wait (&event->cond, &event->mutex);
		} else {
			ret = k_cond_timedwait (&event->cond, &event->mutex, timeo);
		}
	}

	if (event->result == K_EVENT_PULSE)
		event->result = NULL;
	k_lock_unlock (&event->mutex);
	return ret;
}

void k_event_reset (k_event_t *event)
{
	k_lock_lock (&event->mutex);
	event->result = NULL;
	k_lock_unlock (&event->mutex);
}

void k_event_destroy (k_event_t *event)
{
	k_lock_destroy (&event->mutex);
	k_cond_destroy (&event->cond);
}

int k_static_lock_init (k_static_lock_t *lock)
{
	// Use atomic compare-and-exchange to try to change state from UNINIT to INITIALIZING
	intptr_t old_state = aosl_hal_atomic_cmpxchg(&lock->state,
	                                              K_STATIC_LOCK_UNINIT,
	                                              K_STATIC_LOCK_INITIALIZING);

	if (old_state == K_STATIC_LOCK_UNINIT) {
		// Current thread won the race and gets to perform initialization
		int ret = aosl_hal_static_mutex_init(&lock->hal_mutex);
		if (ret != 0) {
			AOSL_LOG_ERR("static_lock_init: hal_static_mutex_init failed, ret=%d", ret);
			// Initialization failed, restore state to UNINIT
			aosl_hal_atomic_set(&lock->state, K_STATIC_LOCK_UNINIT);
			return ret;
		}

		// Initialization succeeded, set state to INITIALIZED
		aosl_hal_atomic_set(&lock->state, K_STATIC_LOCK_INITIALIZED);
		return 0;

	} else if (old_state == K_STATIC_LOCK_INITIALIZING) {
		// Another thread is currently initializing, spin-wait until done
		int retries = 0;
		while (aosl_hal_atomic_read(&lock->state) == K_STATIC_LOCK_INITIALIZING) {
			if (++retries > 100) {
				AOSL_LOG_ERR("static_lock_init: stuck waiting for INITIALIZING state, possible bug");
				return -1;
			}
			aosl_msleep(10);
		}
		return 0;

	} else {
		// Already initialized (old_state == K_STATIC_LOCK_INITIALIZED)
		return 0;
	}
}

void k_static_lock_fini (k_static_lock_t *lock)
{
	int retries = 0;
	intptr_t state = aosl_hal_atomic_read(&lock->state);

	// Wait for concurrent initialization to complete
	while (state == K_STATIC_LOCK_INITIALIZING) {
		if (++retries > 100) {
			AOSL_LOG_ERR("static_lock_fini: stuck in INITIALIZING state, possible bug");
			return;
		}
		aosl_msleep(10);
		state = aosl_hal_atomic_read(&lock->state);
	}

	if (state == K_STATIC_LOCK_INITIALIZED) {
		aosl_hal_static_mutex_fini(&lock->hal_mutex);
		aosl_hal_atomic_set(&lock->state, K_STATIC_LOCK_UNINIT);
	}
}

int k_static_lock_lock (k_static_lock_t *lock)
{
	int ret;
	// Check initialization state
	intptr_t state = aosl_hal_atomic_read(&lock->state);

	if (state != K_STATIC_LOCK_INITIALIZED) {
		ret = k_static_lock_init(lock);
		if (ret != 0) {
			AOSL_LOG_ERR("static_lock_lock: init failed, ret=%d", ret);
			return ret;
		}
	}

	ret = aosl_hal_mutex_lock((aosl_mutex_t)lock->hal_mutex.opaque);
	if (ret != 0) {
		AOSL_LOG_ERR("static_lock_lock: mutex_lock failed, ret=%d", ret);
	}
	return ret;
}

int k_static_lock_trylock (k_static_lock_t *lock)
{
	int ret;
	// Check initialization state
	intptr_t state = aosl_hal_atomic_read(&lock->state);

	if (state != K_STATIC_LOCK_INITIALIZED) {
		ret = k_static_lock_init(lock);
		if (ret != 0) {
			AOSL_LOG_ERR("static_lock_trylock: init failed, ret=%d", ret);
			return ret;
		}
	}

	return aosl_hal_mutex_trylock((aosl_mutex_t)lock->hal_mutex.opaque);
}

int k_static_lock_unlock (k_static_lock_t *lock)
{
	int ret;
	// Check initialization state
	intptr_t state = aosl_hal_atomic_read(&lock->state);

	if (state != K_STATIC_LOCK_INITIALIZED) {
		ret = k_static_lock_init(lock);
		if (ret != 0) {
			AOSL_LOG_ERR("static_lock_unlock: init failed, ret=%d", ret);
			return ret;
		}
	}

	ret = aosl_hal_mutex_unlock((aosl_mutex_t)lock->hal_mutex.opaque);
	if (ret != 0) {
		AOSL_LOG_ERR("static_lock_unlock: mutex_unlock failed, ret=%d", ret);
	}
	return ret;
}
