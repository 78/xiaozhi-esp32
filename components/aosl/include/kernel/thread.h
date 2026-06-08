/***************************************************************************
 * Module:	AOSL threading relative internal definitions
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __KERNEL_THREAD_H__
#define __KERNEL_THREAD_H__

#include <api/aosl_types.h>
#include <kernel/rwlock.h>
#include <hal/aosl_hal_thread.h>
#include <hal/aosl_hal_atomic.h>
#include <api/aosl_thread.h>

#define THREAD_STACK_SIZE (16 << 10)
#define THREAD_NAME_LEN 16

typedef aosl_thread_t k_thread_t;
typedef aosl_mutex_t  k_mutex_t;
typedef void (*k_thread_entry_t) (void *);

typedef struct {
	aosl_mutex_t mutex;
} k_lock_t;

#if defined(AOSL_HAL_HAVE_COND) && AOSL_HAL_HAVE_COND
#undef CONFIG_AOSL_COND
#elif defined(AOSL_HAL_HAVE_SEM) && AOSL_HAL_HAVE_SEM == 1
#define CONFIG_AOSL_COND
#else
#error "Please impl condition val or semaphore"
#endif

#ifdef CONFIG_AOSL_COND
typedef struct {
	k_lock_t lock;
	struct aosl_list_head wait_list; // cond_waiter
} k_cond_t;

#else

typedef struct {
	aosl_cond_t condval;
} k_cond_t;
#endif /* CONFIG_AOSL_COND */

typedef struct {
	const char *name;
	k_thread_entry_t entry;
	void *arg;
	aosl_thread_proiority_e pri;
	int stack_size;
	int done;
	k_lock_t *lock;
	k_cond_t *cond;
} k_thread_create_args_t;

extern int k_thread_create (k_thread_t *thread, const char *name, int priority, int stack_size,
													  k_thread_entry_t entry, void *arg);
extern k_thread_t k_thread_self (void);
extern void k_thread_exit (void *retval);
static inline int k_processors_count (void) {return 1;}

typedef struct {
	k_lock_t lk;
	int rd2wrlock_count;
	k_raw_rwlock_t rw;
} k_rwlock_t;

typedef struct {
	k_lock_t mutex;
	k_cond_t cond;
	void *result;
} k_sync_t;

typedef k_sync_t k_event_t;

/**
 * @brief Static lock initialization states
 */
typedef enum {
	K_STATIC_LOCK_UNINIT = 0,      /**< Uninitialized state */
	K_STATIC_LOCK_INITIALIZING,    /**< Initialization in progress */
	K_STATIC_LOCK_INITIALIZED      /**< Initialized state */
} k_static_lock_state_t;

/**
 * @brief Kernel layer static lock type
 * This structure layout must match aosl_static_lock_t from API layer
 */
typedef aosl_static_lock_t k_static_lock_t;

/**
 * @brief Static lock initializer macro
 * Use this macro to initialize a static lock at compile time:
 * static k_static_lock_t my_lock = K_STATIC_LOCK_INIT;
 */
#define K_STATIC_LOCK_INIT { \
	.hal_mutex = AOSL_STATIC_MUTEX_INIT, \
	.state = K_STATIC_LOCK_UNINIT \
}

typedef int k_tls_key_t;
extern void rb_tls_init (void);
extern void rb_tls_fini (void);
extern int k_tls_key_create (k_tls_key_t *key);
extern void *k_tls_key_get (k_tls_key_t key);
extern int k_tls_key_set (k_tls_key_t key, void *value);
extern int k_tls_key_delete (k_tls_key_t key);

extern void k_lock_init (k_lock_t *lk);
extern void k_lock_init_recursive (k_lock_t *lk);
extern void k_lock_lock (k_lock_t *lk);
extern int k_lock_trylock (k_lock_t *lk);
extern void k_lock_unlock (k_lock_t *lk);
extern void k_lock_destroy (k_lock_t *lk);

extern int k_static_lock_init (k_static_lock_t *lock);
extern void k_static_lock_fini (k_static_lock_t *lock);
extern int k_static_lock_lock (k_static_lock_t *lock);
extern int k_static_lock_trylock (k_static_lock_t *lock);
extern int k_static_lock_unlock (k_static_lock_t *lock);

extern void k_raw_rwlock_init (k_raw_rwlock_t *rw);
extern void k_raw_rwlock_rdlock (k_raw_rwlock_t *rw);
extern int k_raw_rwlock_tryrdlock (k_raw_rwlock_t *rw);
extern void k_raw_rwlock_wrlock (k_raw_rwlock_t *rw);
extern int k_raw_rwlock_trywrlock (k_raw_rwlock_t *rw);
extern void k_raw_rwlock_rdunlock (k_raw_rwlock_t *rw);
extern void k_raw_rwlock_wrunlock (k_raw_rwlock_t *rw);
extern void k_raw_rwlock_destroy (k_raw_rwlock_t *rw);

extern void k_rwlock_init (k_rwlock_t *rw);
extern void k_rwlock_rdlock (k_rwlock_t *rw);
extern int k_rwlock_tryrdlock (k_rwlock_t *rw);
extern void k_rwlock_wrlock (k_rwlock_t *rw);
extern int k_rwlock_trywrlock (k_rwlock_t *rw);
extern void k_rwlock_rdunlock (k_rwlock_t *rw);
extern void k_rwlock_wrunlock (k_rwlock_t *rw);
extern void k_rwlock_rd2wrlock (k_rwlock_t *rw);
extern void k_rwlock_wr2rdlock (k_rwlock_t *rw);
extern void k_rwlock_destroy (k_rwlock_t *rw);

extern void k_cond_init (k_cond_t *cond);
extern void k_cond_signal (k_cond_t *cond);
extern void k_cond_broadcast (k_cond_t *cond);
extern void k_cond_wait (k_cond_t *cond, k_lock_t *lk);
extern int k_cond_timedwait (k_cond_t *cond, k_lock_t *lk, intptr_t timeo);
extern void k_cond_destroy (k_cond_t *cond);

extern void k_event_init (k_event_t *event);
extern void k_event_set (k_event_t *event);
extern void k_event_pulse (k_event_t *event);
extern void k_event_wait (k_event_t *event);
extern int k_event_timedwait (k_event_t *event, intptr_t timeo);
extern void k_event_reset (k_event_t *event);
extern void k_event_destroy (k_event_t *event);

#endif /* __KERNEL_THREAD_H__ */