/***************************************************************************
 * Module:	AOSL threading relative internal implementations.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <kernel/thread.h>
#include <kernel/compiler.h>
#include <kernel/err.h>
#include <api/aosl_thread.h>
#include <api/aosl_mm.h>

__export_in_so__ aosl_thread_t aosl_thread_self (void)
{
	return (aosl_thread_t)k_thread_self ();
}

__export_in_so__ int aosl_tls_key_create (aosl_tls_key_t *key)
{
	k_tls_key_t k;
	int err;

	err = k_tls_key_create (&k);
	if (err < 0)
		return_err (err);

	*key = (aosl_tls_key_t)k;
	return 0;
}

__export_in_so__ void *aosl_tls_key_get (aosl_tls_key_t key)
{
	return k_tls_key_get ((k_tls_key_t)key);
}

__export_in_so__ int aosl_tls_key_set (aosl_tls_key_t key, void *value)
{
	return_err (k_tls_key_set ((k_tls_key_t)key, value));
}

__export_in_so__ int aosl_tls_key_delete (aosl_tls_key_t key)
{
	return_err (k_tls_key_delete ((k_tls_key_t)key));
}

__export_in_so__ aosl_lock_t aosl_lock_create (void)
{
	k_lock_t *lk = (k_lock_t *)aosl_malloc (sizeof (k_lock_t));
	if (lk != NULL) {
		k_lock_init (lk);
	}

	return lk;
}

__export_in_so__ void aosl_lock_lock (aosl_lock_t lock)
{
	k_lock_lock ((k_lock_t *)lock);
}

__export_in_so__ int aosl_lock_trylock (aosl_lock_t lock)
{
	return k_lock_trylock ((k_lock_t *)lock);
}

__export_in_so__ void aosl_lock_unlock (aosl_lock_t lock)
{
	k_lock_unlock ((k_lock_t *)lock);
}

__export_in_so__ void aosl_lock_destroy (aosl_lock_t lock)
{
	k_lock_t *lk = (k_lock_t *)lock;
	k_lock_destroy (lk);
	aosl_free (lk);
}

__export_in_so__ int aosl_static_lock_init (aosl_static_lock_t *lock)
{
	return_err (k_static_lock_init ((k_static_lock_t *)lock));
}

__export_in_so__ void aosl_static_lock_fini (aosl_static_lock_t *lock)
{
	k_static_lock_fini ((k_static_lock_t *)lock);
}

__export_in_so__ int aosl_static_lock_lock (aosl_static_lock_t *lock)
{
	return_err (k_static_lock_lock ((k_static_lock_t *)lock));
}

__export_in_so__ int aosl_static_lock_trylock (aosl_static_lock_t *lock)
{
	return_err (k_static_lock_trylock ((k_static_lock_t *)lock));
}

__export_in_so__ int aosl_static_lock_unlock (aosl_static_lock_t *lock)
{
	return_err (k_static_lock_unlock ((k_static_lock_t *)lock));
}

__export_in_so__ aosl_rwlock_t aosl_rwlock_create (void)
{
	k_rwlock_t *rw = (k_rwlock_t *)aosl_malloc (sizeof (k_rwlock_t));
	if (rw != NULL)
		k_rwlock_init (rw);

	return rw;
}

__export_in_so__ void aosl_rwlock_rdlock (aosl_rwlock_t rwlock)
{
	k_rwlock_rdlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ int aosl_rwlock_tryrdlock (aosl_rwlock_t rwlock)
{
	return k_rwlock_tryrdlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ void aosl_rwlock_wrlock (aosl_rwlock_t rwlock)
{
	k_rwlock_wrlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ int aosl_rwlock_trywrlock (aosl_rwlock_t rwlock)
{
	return k_rwlock_trywrlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ void aosl_rwlock_rdunlock (aosl_rwlock_t rwlock)
{
	k_rwlock_rdunlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ void aosl_rwlock_wrunlock (aosl_rwlock_t rwlock)
{
	k_rwlock_wrunlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ void aosl_rwlock_rd2wrlock (aosl_rwlock_t rwlock)
{
	k_rwlock_rd2wrlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ void aosl_rwlock_wr2rdlock (aosl_rwlock_t rwlock)
{
	k_rwlock_wr2rdlock ((k_rwlock_t *)rwlock);
}

__export_in_so__ void aosl_rwlock_destroy (aosl_rwlock_t rwlock)
{
	k_rwlock_t *rw = (k_rwlock_t *)rwlock;
	k_rwlock_destroy (rw);
	aosl_free (rw);
}

__export_in_so__ aosl_cond_t aosl_cond_create (void)
{
	k_cond_t *cond = (k_cond_t *)aosl_malloc (sizeof (k_cond_t));
	if (cond != NULL)
		k_cond_init (cond);

	return cond;
}

__export_in_so__ void aosl_cond_signal (aosl_cond_t cond_var)
{
	k_cond_signal ((k_cond_t *)cond_var);
}

__export_in_so__ void aosl_cond_broadcast (aosl_cond_t cond_var)
{
	k_cond_broadcast ((k_cond_t *)cond_var);
}

__export_in_so__ void aosl_cond_wait (aosl_cond_t cond_var, aosl_lock_t lock)
{
	k_cond_wait ((k_cond_t *)cond_var, (k_lock_t *)lock);
}

__export_in_so__ int aosl_cond_timedwait (aosl_cond_t cond_var, aosl_lock_t lock, intptr_t timeo)
{
	return k_cond_timedwait ((k_cond_t *)cond_var, (k_lock_t *)lock, timeo);
}

__export_in_so__ void aosl_cond_destroy (aosl_cond_t cond_var)
{
	k_cond_t *cond = (k_cond_t *)cond_var;
	k_cond_destroy (cond);
	aosl_free (cond);
}

__export_in_so__ aosl_event_t aosl_event_create (void)
{
	k_event_t *event = (k_event_t *)aosl_malloc_impl (sizeof (k_event_t));
	if (event != NULL)
		k_event_init (event);

	return event;
}

__export_in_so__ void aosl_event_set (aosl_event_t event_var)
{
	k_event_set ((k_event_t *)event_var);
}

__export_in_so__ void aosl_event_pulse (aosl_event_t event_var)
{
	k_event_pulse ((k_event_t *)event_var);
}

__export_in_so__ void aosl_event_wait (aosl_event_t event_var)
{
	k_event_wait ((k_event_t *)event_var);
}

__export_in_so__ int aosl_event_timedwait (aosl_event_t event_var, intptr_t timeo)
{
	return k_event_timedwait ((k_event_t *)event_var, timeo);
}

__export_in_so__ void aosl_event_reset (aosl_event_t event_var)
{
	k_event_reset ((k_event_t *)event_var);
}

__export_in_so__ void aosl_event_destroy (aosl_event_t event_var)
{
	k_event_t *event = (k_event_t *)event_var;
	k_event_destroy (event);
	aosl_free (event);
}
