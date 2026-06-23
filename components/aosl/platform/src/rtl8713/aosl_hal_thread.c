#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_thread.h>
#include <hal/aosl_hal_atomic.h>

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

#define UNUSED(expr) (void)(expr)

typedef struct {
	SemaphoreHandle_t sem;
	volatile intptr_t waiters;
} aosl_freertos_cond_t;

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
	int sched_priority;

	switch (param->priority) {
		case AOSL_THRD_PRI_LOW:
			sched_priority = OS_PRIORITY_LOW;
			break;
		case AOSL_THRD_PRI_NORMAL:
		case AOSL_THRD_PRI_DEFAULT:
			sched_priority = OS_PRIORITY_NORMAL;
			break;
		case AOSL_THRD_PRI_HIGH:
			sched_priority = OS_PRIORITY_ABOVE_NORMAL;
			break;
		case AOSL_THRD_PRI_HIGHEST:
			sched_priority = OS_PRIORITY_HIGH;
			break;
		case AOSL_THRD_PRI_RT:
			sched_priority = OS_PRIORITY_REAL_TIME;
			break;
		default:
			sched_priority = OS_PRIORITY_NORMAL;
			break;
	}

	// Allocate wrapper args to pass both entry function and user args
	thread_wrapper_args_t *wrapper_args = aosl_malloc(sizeof(thread_wrapper_args_t));
	if (!wrapper_args) {
		return -ENOMEM;
	}
	wrapper_args->entry = entry;
	wrapper_args->arg = args;

	/* Create the FreeRTOS task that will run the pthread-style entry function */
	if (xTaskCreate(thread_wrapper, param->name, (uint16_t)(param->stack_size / sizeof (StackType_t)),
					 wrapper_args, sched_priority, &xTaskHandle) != pdPASS) {
		aosl_free(wrapper_args);
		return -ENOMEM;
	}

	*thread = (aosl_thread_t)xTaskHandle;

	return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
	UNUSED(thread);
}

void aosl_hal_thread_exit(void *retval)
{
	UNUSED(retval);
	vTaskDelete(NULL);
}

aosl_thread_t aosl_hal_thread_self(void)
{
	return (aosl_thread_t)xTaskGetCurrentTaskHandle();
}

int aosl_hal_thread_set_name(const char *name)
{
	UNUSED(name);
	return 0;
}

int aosl_hal_thread_get_name(char *name, size_t size)
{
  (void)name;
  (void)size;
  return -1;
}

int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority)
{
	int sched_priority;

	switch (priority) {
		case AOSL_THRD_PRI_LOW:
			sched_priority = OS_PRIORITY_LOW;
			break;
		case AOSL_THRD_PRI_NORMAL:
		case AOSL_THRD_PRI_DEFAULT:
			sched_priority = OS_PRIORITY_NORMAL;
			break;
		case AOSL_THRD_PRI_HIGH:
			sched_priority = OS_PRIORITY_ABOVE_NORMAL;
			break;
		case AOSL_THRD_PRI_HIGHEST:
			sched_priority = OS_PRIORITY_HIGH;
			break;
		case AOSL_THRD_PRI_RT:
			sched_priority = OS_PRIORITY_REAL_TIME;
			break;
		default:
			sched_priority = OS_PRIORITY_NORMAL;
			break;
	}

	vTaskPrioritySet(NULL, (UBaseType_t)sched_priority);
	return 0;
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
	UNUSED(thread);
	UNUSED(retval);
	return 0;
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
	UNUSED(thread);
	return;
}

aosl_mutex_t aosl_hal_mutex_create(void)
{
	SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
	if (!mutex) {
		AOSL_LOG_ERR("mutex create failed");
		return NULL;
	}

	return (aosl_mutex_t)mutex;
}

void aosl_hal_mutex_destroy(aosl_mutex_t mutex)
{
	if (!mutex) {
		return;
	}
	vSemaphoreDelete((SemaphoreHandle_t)mutex);
}

int aosl_hal_mutex_lock(aosl_mutex_t mutex)
{
	if (xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdPASS) {
		return 0;
	}
	return -1;
}

int aosl_hal_mutex_trylock(aosl_mutex_t mutex)
{
	if (xSemaphoreTake((SemaphoreHandle_t)mutex, 0) == pdPASS) {
		return 0;
	}
	return -1;
}

int aosl_hal_mutex_unlock(aosl_mutex_t mutex)
{
	if (xSemaphoreGive((SemaphoreHandle_t)mutex) == pdPASS) {
		return 0;
	}
	return -1;
}


aosl_sem_t aosl_hal_sem_create(void)
{
	/* Create a counting semaphore with max count 1 and initial count 0 */
	SemaphoreHandle_t sem = xSemaphoreCreateCounting(1, 0);
	if (!sem) {
		AOSL_LOG_ERR("semaphore create failed");
		return NULL;
	}

	return (aosl_sem_t)sem;
}

void aosl_hal_sem_destroy(aosl_sem_t sem)
{
	if (!sem) {
		return;
	}

	vSemaphoreDelete((SemaphoreHandle_t)sem);
}

int aosl_hal_sem_post(aosl_sem_t sem)
{
	if (!sem) {
		return -1;
	}

	if (xSemaphoreGive((SemaphoreHandle_t)sem) == pdPASS) {
		return 0;
	}
	return -1;
}

int aosl_hal_sem_wait(aosl_sem_t sem)
{
	if (!sem) {
		return -1;
	}

	if (xSemaphoreTake((SemaphoreHandle_t)sem, portMAX_DELAY) == pdPASS) {
		return 0;
	}
	return -1;
}

int aosl_hal_sem_timedwait(aosl_sem_t sem, intptr_t timeout_ms)
{
	if (!sem) {
		return -1;
	}

	TickType_t timeout_ticks;
	if (timeout_ms < 0) {
		timeout_ticks = portMAX_DELAY;
	} else {
		timeout_ticks = pdMS_TO_TICKS(timeout_ms);
		if (timeout_ticks == 0 && timeout_ms > 0) {
			timeout_ticks = 1; /* At least 1 tick for non-zero timeout */
		}
	}

	if (xSemaphoreTake((SemaphoreHandle_t)sem, timeout_ticks) == pdPASS) {
		return 0;
	}
	return -1;
}

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
	// Empty implementation for rtl8713 platform
	// This platform does not support static mutex initialization
	(void)mutex;
	return 0;
}

void aosl_hal_static_mutex_fini(aosl_static_mutex_t *mutex)
{
	(void)mutex;
}
