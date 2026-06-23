/***************************************************************************
 * Module:	thread hal definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_THREAD_H__
#define __AOSL_HAL_THREAD_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <hal/aosl_hal_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief thread priority levels
 */
typedef enum {
  AOSL_THRD_PRI_DEFAULT = 0,
  AOSL_THRD_PRI_LOW = 1,
  AOSL_THRD_PRI_NORMAL = 2,
  AOSL_THRD_PRI_HIGH = 3,
  AOSL_THRD_PRI_HIGHEST = 4,
  AOSL_THRD_PRI_RT = 5,
} aosl_thread_proiority_e;

/**
 * @brief thread handle type
 */
typedef intptr_t aosl_thread_t;

/**
 * @brief thread creation parameters
 */
typedef struct {
  const char *name;
  aosl_thread_proiority_e priority;
  int  stack_size;
} aosl_thread_param_t;

/**
 * @brief create a new thread
 * @param [out] thread pointer to store created thread handle
 * @param [in] param thread creation parameters
 * @param [in] entry thread entry function
 * @param [in] args argument to pass to thread entry function
 * @return 0 on success, < 0 on error
 */
int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
                           void *(*entry)(void *), void *args);

/**
 * @brief destroy a thread
 * @param [in] thread thread handle
 */
void aosl_hal_thread_destroy(aosl_thread_t thread);

/**
 * @brief exit current thread
 * @param [in] retval return value of the thread
 */
void aosl_hal_thread_exit(void *retval);

/**
 * @brief set current thread name
 * @param [in] name thread name
 * @return 0 on success, < 0 on error
 */
int aosl_hal_thread_set_name(const char *name);

/**
 * @brief get current thread name
 * @param [out] name output buffer for thread name
 * @param [in] size output buffer size
 * @return 0 on success, < 0 on error
 */
int aosl_hal_thread_get_name(char *name, size_t size);

/**
 * @brief set current thread priority
 * @param [in] priority thread priority
 * @return 0 on success, < 0 on error
 */
int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority);

/**
 * @brief join a thread
 * @param [in] thread thread handle
 * @param [out] retval pointer to store thread return value
 * @return 0 on success, < 0 on error
 */
int aosl_hal_thread_join(aosl_thread_t thread, void **retval);

/**
 * @brief detach a thread
 * @param [in] thread thread handle
 */
void aosl_hal_thread_detach(aosl_thread_t thread);

/**
 * @brief get current thread handle
 * @return current thread handle
 */
aosl_thread_t aosl_hal_thread_self(void);

/**
 * @brief mutex type handle
 */
typedef void* aosl_mutex_t;

/**
 * @brief static mutex size in bytes
 * Large enough to hold platform-specific mutex data
 * Linux: pthread_mutex_t (~40 bytes)
 * FreeRTOS: StaticSemaphore_t (~80 bytes)
 */
#define AOSL_STATIC_MUTEX_SIZE 128

/**
 * @brief static mutex type with opaque array
 * Contains platform-specific mutex data in an opaque array
 */
typedef struct {
    union {
        uint8_t opaque[AOSL_STATIC_MUTEX_SIZE];
        uint64_t _align; /* force 8-byte alignment */
    };
} aosl_static_mutex_t;

/**
 * @brief platform-specific static mutex initializer macro
 * Each platform defines this macro in their implementation
 */
#ifndef AOSL_STATIC_MUTEX_INIT
#define AOSL_STATIC_MUTEX_INIT { .opaque = { 0 } }
#endif

/**
 * @brief initialize a static mutex
 * Note: This function does not guarantee thread safety.
 * The caller must ensure no concurrent calls occur.
 * 
 * @param [in] mutex pointer to static mutex
 * @return 0 on success, < 0 on error
 */
int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex);

/**
 * @brief finalize a static mutex
 * Release resources associated with a static mutex.
 * Must be called before the memory holding the static mutex is freed.
 * 
 * @param [in] mutex pointer to static mutex
 */
void aosl_hal_static_mutex_fini(aosl_static_mutex_t *mutex);

/**
 * @brief create a new mutex
 * @return mutex handle, or NULL on error
 */
aosl_mutex_t aosl_hal_mutex_create(void);

/**
 * @brief destroy a mutex
 * @param [in] mutex mutex handle
 */
void aosl_hal_mutex_destroy(aosl_mutex_t mutex);

/**
 * @brief lock a mutex
 * @param [in] mutex mutex handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_mutex_lock(aosl_mutex_t mutex);

/**
 * @brief try to lock a mutex
 * @param [in] mutex mutex handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_mutex_trylock(aosl_mutex_t mutex);

/**
 * @brief unlock a mutex
 * @param [in] mutex mutex handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_mutex_unlock(aosl_mutex_t mutex);


/**
 * @note Implement condition variable or semaphore
 *       and set AOSL_HAL_HAVE_COND or AOSL_HAL_HAVE_SEM in aosl_hal_config.h
 */

/**
 * @brief condition variable handle type
 */
typedef void* aosl_cond_t;

/**
 * @brief   create a condition variable
 * @return  condition variable handle, or NULL on error
 */
aosl_cond_t aosl_hal_cond_create(void);

/**
 * @brief destroy a condition variable
 * @param [in] cond condition variable handle
 */
void aosl_hal_cond_destroy(aosl_cond_t cond);

/**
 * @brief signal a condition variable
 * @param [in] cond condition variable handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_cond_signal(aosl_cond_t cond);

/**
 * @brief broadcast a condition variable
 * @param [in] cond condition variable handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_cond_broadcast(aosl_cond_t cond);

/**
 * @brief wait on a condition variable
 * @param [in] cond condition variable handle
 * @param [in] mutex mutex handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_cond_wait(aosl_cond_t cond, aosl_mutex_t mutex);

/**
 * @brief timed wait on a condition variable
 * @param [in] cond condition variable handle
 * @param [in] mutex mutex handle
 * @param [in] timeout_ms timeout in milliseconds
 * @return 0 on success, < 0 on error
 */
int aosl_hal_cond_timedwait(aosl_cond_t cond, aosl_mutex_t mutex, intptr_t timeout_ms);

/**
 * @brief semaphore handle type
 */
typedef void *aosl_sem_t;

/**
 * @brief create a semaphore
 * @return semaphore handle, or NULL on error
 */
aosl_sem_t aosl_hal_sem_create(void);

/**
 * @brief destroy a semaphore
 * @param [in] sem semaphore handle
 */
void aosl_hal_sem_destroy(aosl_sem_t sem);

/**
 * @brief post (signal) a semaphore
 * @param [in] sem semaphore handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_sem_post(aosl_sem_t sem);

/**
 * @brief wait (pend) a semaphore
 * @param [in] sem semaphore handle
 * @return 0 on success, < 0 on error
 */
int aosl_hal_sem_wait(aosl_sem_t sem);

/**
 * @brief timed wait (pend) a semaphore
 * @param [in] sem semaphore handle
 * @param [in] timeout_ms timeout in milliseconds
 * @return 0 on success, < 0 on error
 */
int aosl_hal_sem_timedwait(aosl_sem_t sem, intptr_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __AOSL_HAL_THREAD_H__ */
