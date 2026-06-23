/***************************************************************************
 * Module:	AOSL thread relative definitions
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_THREAD_H__
#define __AOSL_THREAD_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_thread.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef uintptr_t aosl_tls_key_t;

/**
 * @brief Create a thread-local storage (TLS) key.
 * @param [out] key  pointer to store the created TLS key
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_tls_key_create (aosl_tls_key_t *key);

/**
 * @brief Get the value associated with a TLS key in the current thread.
 * @param [in] key  the TLS key
 * @return          the value, or NULL if not set
 **/
extern __aosl_api__ void *aosl_tls_key_get (aosl_tls_key_t key);

/**
 * @brief Set the value associated with a TLS key in the current thread.
 * @param [in] key    the TLS key
 * @param [in] value  the value to associate
 * @return            0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_tls_key_set (aosl_tls_key_t key, void *value);

/**
 * @brief Delete a TLS key. Does not free the associated values in each thread.
 * @param [in] key  the TLS key to delete
 * @return          0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_tls_key_delete (aosl_tls_key_t key);


typedef void *aosl_lock_t;

/**
 * @brief Create a mutex lock object.
 * @return  the lock handle, or NULL on failure
 **/
extern __aosl_api__ aosl_lock_t aosl_lock_create (void);

/**
 * @brief Acquire the mutex lock (blocking).
 * @param [in] lock  the lock handle
 **/
extern __aosl_api__ void aosl_lock_lock (aosl_lock_t lock);

/**
 * @brief Try to acquire the mutex lock without blocking.
 * @param [in] lock  the lock handle
 * @return           0 if the lock was acquired, <0 if it was already held
 **/
extern __aosl_api__ int aosl_lock_trylock (aosl_lock_t lock);

/**
 * @brief Release the mutex lock.
 * @param [in] lock  the lock handle
 **/
extern __aosl_api__ void aosl_lock_unlock (aosl_lock_t lock);

/**
 * @brief Destroy a mutex lock object and free its resources.
 * @param [in] lock  the lock handle
 **/
extern __aosl_api__ void aosl_lock_destroy (aosl_lock_t lock);


/**
 * @brief Static lock type for static declaration
 * This type can be statically declared and initialized at compile time.
 */
typedef struct {
	intptr_t state;                 /**< Atomic initialization state */
	aosl_static_mutex_t hal_mutex;  /**< HAL layer static mutex */
} aosl_static_lock_t;

/**
 * @brief Static lock initializer macro
 * Use this macro to initialize a static lock at compile time:
 * static aosl_static_lock_t my_lock = AOSL_STATIC_LOCK_INIT;
 */
#define AOSL_STATIC_LOCK_INIT { \
	.hal_mutex = AOSL_STATIC_MUTEX_INIT, \
	.state = 0 \
}

/**
 * @brief Initialize a statically declared lock. Called automatically on first use.
 * @param [in,out] lock  pointer to the static lock
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_static_lock_init (aosl_static_lock_t *lock);

/**
 * @brief Finalize a static lock, releasing underlying resources.
 * Must be called before the memory holding the lock is freed.
 * @param [in,out] lock  pointer to the static lock
 **/
extern __aosl_api__ void aosl_static_lock_fini (aosl_static_lock_t *lock);

/**
 * @brief Acquire a static lock (blocking).
 * @param [in,out] lock  pointer to the static lock
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_static_lock_lock (aosl_static_lock_t *lock);

/**
 * @brief Try to acquire a static lock without blocking.
 * @param [in,out] lock  pointer to the static lock
 * @return               0 if acquired, <0 if already held
 **/
extern __aosl_api__ int aosl_static_lock_trylock (aosl_static_lock_t *lock);

/**
 * @brief Release a static lock.
 * @param [in,out] lock  pointer to the static lock
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_static_lock_unlock (aosl_static_lock_t *lock);


typedef void *aosl_rwlock_t;

/**
 * @brief Create a read-write lock object.
 * @return  the rwlock handle, or NULL on failure
 **/
extern __aosl_api__ aosl_rwlock_t aosl_rwlock_create (void);

/**
 * @brief Acquire the read lock (shared, blocking).
 * @param [in] rwlock  the rwlock handle
 **/
extern __aosl_api__ void aosl_rwlock_rdlock (aosl_rwlock_t rwlock);

/**
 * @brief Try to acquire the read lock without blocking.
 * @param [in] rwlock  the rwlock handle
 * @return             0 if acquired, <0 otherwise
 **/
extern __aosl_api__ int aosl_rwlock_tryrdlock (aosl_rwlock_t rwlock);

/**
 * @brief Acquire the write lock (exclusive, blocking).
 * @param [in] rwlock  the rwlock handle
 **/
extern __aosl_api__ void aosl_rwlock_wrlock (aosl_rwlock_t rwlock);

/**
 * @brief Try to acquire the write lock without blocking.
 * @param [in] rwlock  the rwlock handle
 * @return             0 if acquired, <0 otherwise
 **/
extern __aosl_api__ int aosl_rwlock_trywrlock (aosl_rwlock_t rwlock);

/**
 * @brief Release the read lock.
 * @param [in] rwlock  the rwlock handle
 **/
extern __aosl_api__ void aosl_rwlock_rdunlock (aosl_rwlock_t rwlock);

/**
 * @brief Release the write lock.
 * @param [in] rwlock  the rwlock handle
 **/
extern __aosl_api__ void aosl_rwlock_wrunlock (aosl_rwlock_t rwlock);

/**
 * @brief Upgrade a held read lock to a write lock (blocking).
 * @param [in] rwlock  the rwlock handle
 **/
extern __aosl_api__ void aosl_rwlock_rd2wrlock (aosl_rwlock_t rwlock);

/**
 * @brief Downgrade a held write lock to a read lock.
 * @param [in] rwlock  the rwlock handle
 **/
extern __aosl_api__ void aosl_rwlock_wr2rdlock (aosl_rwlock_t rwlock);

/**
 * @brief Destroy a read-write lock object and free its resources.
 * @param [in] rwlock  the rwlock handle
 **/
extern __aosl_api__ void aosl_rwlock_destroy (aosl_rwlock_t rwlock);


//typedef void *aosl_cond_t;

/**
 * @brief Create a condition variable object.
 * @return  the condition variable handle, or NULL on failure
 **/
extern __aosl_api__ aosl_cond_t aosl_cond_create (void);

/**
 * @brief Wake up one thread waiting on the condition variable.
 * @param [in] cond_var  the condition variable handle
 **/
extern __aosl_api__ void aosl_cond_signal (aosl_cond_t cond_var);

/**
 * @brief Wake up all threads waiting on the condition variable.
 * @param [in] cond_var  the condition variable handle
 **/
extern __aosl_api__ void aosl_cond_broadcast (aosl_cond_t cond_var);

/**
 * @brief Wait on a condition variable, releasing the associated lock while waiting.
 * @param [in] cond_var  the condition variable handle
 * @param [in] lock      the lock to release during the wait
 **/
extern __aosl_api__ void aosl_cond_wait (aosl_cond_t cond_var, aosl_lock_t lock);

/**
 * @brief Wait on a condition variable with a timeout.
 * @param [in] cond_var  the condition variable handle
 * @param [in] lock      the lock to release during the wait
 * @param [in] timeo     timeout in milliseconds
 * @return               0 if signaled, <0 on timeout or error
 **/
extern __aosl_api__ int aosl_cond_timedwait (aosl_cond_t cond_var, aosl_lock_t lock, intptr_t timeo);

/**
 * @brief Destroy a condition variable object and free its resources.
 * @param [in] cond_var  the condition variable handle
 **/
extern __aosl_api__ void aosl_cond_destroy (aosl_cond_t cond_var);

typedef void *aosl_event_t;

/**
 * @brief Create an event object for thread synchronization.
 * @return  the event handle, or NULL on failure
 **/
extern __aosl_api__ aosl_event_t aosl_event_create (void);

/**
 * @brief Set the event to signaled state, waking all waiting threads.
 * The event remains signaled until explicitly reset.
 * @param [in] event_var  the event handle
 **/
extern __aosl_api__ void aosl_event_set (aosl_event_t event_var);

/**
 * @brief Pulse the event: wake all waiting threads, then immediately reset.
 * @param [in] event_var  the event handle
 **/
extern __aosl_api__ void aosl_event_pulse (aosl_event_t event_var);

/**
 * @brief Wait for the event to become signaled (blocking).
 * @param [in] event_var  the event handle
 **/
extern __aosl_api__ void aosl_event_wait (aosl_event_t event_var);

/**
 * @brief Wait for the event to become signaled with a timeout.
 * @param [in] event_var  the event handle
 * @param [in] timeo      timeout in milliseconds
 * @return                0 if signaled, <0 on timeout or error
 **/
extern __aosl_api__ int aosl_event_timedwait (aosl_event_t event_var, intptr_t timeo);

/**
 * @brief Reset the event to non-signaled state.
 * @param [in] event_var  the event handle
 **/
extern __aosl_api__ void aosl_event_reset (aosl_event_t event_var);

/**
 * @brief Destroy an event object and free its resources.
 * @param [in] event_var  the event handle
 **/
extern __aosl_api__ void aosl_event_destroy (aosl_event_t event_var);


#ifdef __cplusplus
}
#endif

#endif /* __AOSL_THREAD_H__ */