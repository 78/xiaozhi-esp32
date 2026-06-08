#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include <sys/time.h>
#include <sys/errno.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_thread.h>

// Verify that AOSL_STATIC_MUTEX_SIZE is large enough for pthread_mutex_t
aosl_static_assert(sizeof(pthread_mutex_t) <= AOSL_STATIC_MUTEX_SIZE, 
                   static_mutex_size_check);

/**
 * @brief Thread priority definition
 */
typedef enum  {
	OS_PRIORITY_IDLE            = 0,
	OS_PRIORITY_LOW             = 1,
	OS_PRIORITY_BELOW_NORMAL    = 2,
	OS_PRIORITY_NORMAL          = 3,
	OS_PRIORITY_ABOVE_NORMAL    = 4,
	OS_PRIORITY_HIGH            = 5,
	OS_PRIORITY_REAL_TIME       = 6
} OS_Priority;

// Wrapper structure to pass pthread-style entry function to FreeRTOS task
typedef struct {
	void *(*entry)(void *);
	void *arg;
} thread_wrapper_args_t;

// Wrapper function to convert pthread-style entry to FreeRTOS task
static void thread_wrapper(void *arg)
{
	thread_wrapper_args_t *wrapper_args = (thread_wrapper_args_t *)arg;
	void *(*entry)(void *) = wrapper_args->entry;
	void *user_arg = wrapper_args->arg;
	
	// Free the wrapper args
	aosl_free(wrapper_args);
	
	// Call the pthread-style entry function
	void *retval = entry(user_arg);
	
	// FreeRTOS tasks should not return, so delete the task
	(void)retval;
	vTaskDelete(NULL);
}

int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
													 void *(*entry)(void *), void *args)
{
	assert(sizeof(aosl_thread_t) >= sizeof(TaskHandle_t));

	TaskHandle_t xTaskHandle;
	int sched_priority = OS_PRIORITY_HIGH;
	int ret;

	if (param->stack_size == 0) {
		param->stack_size = 16 << 10;
	}

	// Allocate wrapper args to pass both entry function and user args
	thread_wrapper_args_t *wrapper_args = aosl_malloc(sizeof(thread_wrapper_args_t));
	if (!wrapper_args) {
		return -ENOMEM;
	}
	wrapper_args->entry = entry;
	wrapper_args->arg = args;

	/* Create the FreeRTOS task that will run the pthread-style entry function */
	ret = xTaskCreate(thread_wrapper, param->name, (uint16_t)(param->stack_size / sizeof (StackType_t)),
					 wrapper_args, sched_priority, &xTaskHandle);
	if (ret != pdPASS) {
		aosl_free(wrapper_args);
		return -ENOMEM;
	}

	*thread = (aosl_thread_t)xTaskHandle;

	return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
}

void aosl_hal_thread_exit(void *retval)
{
	vTaskDelete(NULL);
}

aosl_thread_t aosl_hal_thread_self()
{
	return (aosl_thread_t)xTaskGetCurrentTaskHandle();
}

int aosl_hal_thread_set_name(const char *name)
{
	return 0;
}

int aosl_hal_thread_get_name(char *name, size_t size)
{
	const char *task_name;

	if (!name || size == 0) {
		return -1;
	}

	task_name = pcTaskGetName((TaskHandle_t)aosl_hal_thread_self());
	if (!task_name) {
		return -1;
	}

	snprintf(name, size, "%s", task_name);
	return 0;
}

int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority)
{
	return 0;
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
	return 0;
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
	return;
}

aosl_mutex_t aosl_hal_mutex_create()
{
	int err;
	pthread_mutex_t *n_mutex = aosl_calloc(1, sizeof(pthread_mutex_t));
	if (!n_mutex) {
		return NULL;
	}
	err = pthread_mutex_init(n_mutex, NULL);
	if (err != 0) {
		AOSL_LOG_ERR("mutex create failed, err=%d", err);
		aosl_free(n_mutex);
		return NULL;
	}

	return (aosl_mutex_t)n_mutex;
}

void aosl_hal_mutex_destroy(aosl_mutex_t mutex)
{
	pthread_mutex_t *n_mutex = (pthread_mutex_t *)mutex;
	if (!n_mutex) {
		return;
	}
	pthread_mutex_destroy(n_mutex);
	aosl_free(n_mutex);
}

int aosl_hal_mutex_lock(aosl_mutex_t mutex)
{
	return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

int aosl_hal_mutex_trylock(aosl_mutex_t mutex)
{
	return pthread_mutex_trylock((pthread_mutex_t *)mutex);
}

int aosl_hal_mutex_unlock(aosl_mutex_t mutex)
{
	return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
	if (!mutex) {
		return -1;
	}

	// Initialize with PTHREAD_MUTEX_INITIALIZER and copy to opaque array
	pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
	memcpy(mutex->opaque, &init_mutex, sizeof(pthread_mutex_t));
	
	return 0;
}

void aosl_hal_static_mutex_fini(aosl_static_mutex_t *mutex)
{
	(void)mutex;
}

aosl_cond_t aosl_hal_cond_create(void)
{
	pthread_cond_t *n_cond = aosl_calloc(1, sizeof(pthread_cond_t));
	if (!n_cond) {
		return NULL;
	}

	int err = pthread_cond_init(n_cond, NULL);
	if (err != 0) {
		AOSL_LOG_ERR("cond create failed, err=%d", err);
		aosl_free(n_cond);
		return NULL;
	}

	return (aosl_cond_t)n_cond;
}

void aosl_hal_cond_destroy(aosl_cond_t cond)
{
	pthread_cond_t *n_cond = (pthread_cond_t *)cond;
	if (!n_cond) {
		return;
	}

	pthread_cond_destroy(n_cond);
	aosl_free(n_cond);
}

int aosl_hal_cond_signal(aosl_cond_t cond)
{
	pthread_cond_t *n_cond = (pthread_cond_t *)cond;
	if (!n_cond) {
		return -1;
	}

	return pthread_cond_signal(n_cond);
}

int aosl_hal_cond_broadcast(aosl_cond_t cond)
{
	pthread_cond_t *n_cond = (pthread_cond_t *)cond;
	if (!n_cond) {
		return -1;
	}

	return pthread_cond_broadcast(n_cond);
}

int aosl_hal_cond_wait(aosl_cond_t cond, aosl_mutex_t mutex)
{
	pthread_cond_t *n_cond = (pthread_cond_t *)cond;
	pthread_mutex_t *n_mutex = (pthread_mutex_t *)mutex;

	if (!n_cond || !n_mutex) {
		return -1;
	}

	return pthread_cond_wait(n_cond, n_mutex);
}

int aosl_hal_cond_timedwait(aosl_cond_t cond, aosl_mutex_t mutex, intptr_t timeout_ms)
{
	pthread_cond_t *n_cond = (pthread_cond_t *)cond;
	pthread_mutex_t *n_mutex = (pthread_mutex_t *)mutex;

	if (!n_cond || !n_mutex) {
		return -1;
	}

	struct timespec timeo;
	struct timespec now;
	// Use CLOCK_REALTIME because esp32 not support CLOCK_MONOTONIC
	clock_gettime (CLOCK_REALTIME, &now);
	timeo.tv_sec = now.tv_sec + timeout_ms / 1000;
	timeo.tv_nsec = now.tv_nsec + (timeout_ms % 1000) * 1000000;
	while (timeo.tv_nsec >= 1000000000) {
		timeo.tv_nsec -= 1000000000;
		timeo.tv_sec++;
	}

	return pthread_cond_timedwait(n_cond, n_mutex, &timeo);
}
