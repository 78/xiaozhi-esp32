#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "kernel/os/os.h"
#include "sys/list.h"

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_thread.h>
#include <hal/aosl_hal_atomic.h>
#include "cedarx/os_glue/include/pthread.h"
#include "cedarx/os_glue/include/atomic.h"

#define __PTHREAD__JOIN___			1

#define PTHREAD_STACK_DEFAULT_SIZE          (2 * 1024)

#if __PTHREAD__JOIN___
#define PTHREAD_DETACH_STATE        (1 << 0)
typedef struct {
    OS_Thread_t                 thd;
    // pthread_osstate_t           state;
    struct list_head            task_list;
    OS_ThreadEntry_t            func;
    void *                      arg;
}aosl_pthread_t;

static LIST_HEAD_DEF(aosl_pthread_task_head);
static OS_Mutex_t aosl_pthread_task_lock = {OS_INVALID_HANDLE};

#endif


// Forward declarations for atomic operations
static inline void aosl_atomic_inc(volatile intptr_t *val);
static inline void aosl_atomic_dec(volatile intptr_t *val);

// Implementation of atomic operations
static inline void aosl_atomic_inc(volatile intptr_t *val)
{
	ENTER_CRITICAL();
	(*val)++;
	ENTER_CRITICAL();
}
static inline void aosl_atomic_dec(volatile intptr_t *val)
{
	ENTER_CRITICAL();
	(*val)--;
	ENTER_CRITICAL();
}

#define UNUSED(expr) (void)(expr)

typedef struct {
	SemaphoreHandle_t sem;
	volatile intptr_t waiters;
} aosl_freertos_cond_t;

#if !__PTHREAD__JOIN___

// Wrapper structure to pass pthread-style entry function to FreeRTOS task
typedef struct {
	void *(*entry)(void *);
	void *arg;
} thread_wrapper_args_t;

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

#else

static void thread_wrapper(void *arg)
{
    aosl_pthread_t *wrap = (aosl_pthread_t *)arg;
    wrap->func(wrap->arg);
    aosl_hal_thread_exit(NULL);
    return;
}

#endif


// Wrapper function to convert pthread-style entry to FreeRTOS task

int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
								 void *(*entry)(void *), void *args)
{
#if !__PTHREAD__JOIN___
	assert(sizeof(aosl_thread_t) >= sizeof(TaskHandle_t));

	TaskHandle_t xTaskHandle;
	int sched_priority = OS_PRIORITY_HIGH;

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
		return -1;
	}


	wrapper_args->entry = entry;
	wrapper_args->arg = args;

	if (param->stack_size == 0) {
		param->stack_size = PTHREAD_STACK_DEFAULT_SIZE;
	}

	/* Create the FreeRTOS task that will run the pthread-style entry function */
	if (xTaskCreate(thread_wrapper, param->name, (uint16_t)(param->stack_size / sizeof (StackType_t)),
			 wrapper_args, sched_priority, &xTaskHandle) != pdPASS) {
		aosl_free(wrapper_args);
		return -1;
	}

	*thread = (aosl_thread_t)xTaskHandle;
	
	return 0;
#else
	OS_Status ret = -1;
	OS_Priority prio;
	aosl_pthread_t *thrd;
	uint32_t size;

	if(thread == NULL)
        return -1;
    if(entry == NULL)
        return -1;
	if(param == NULL || param->priority == AOSL_THRD_PRI_DEFAULT)
		prio = OS_PRIORITY_NORMAL;
	else
		prio = (OS_Priority)param->priority;

	if(param == NULL || param->stack_size == 0)
		size = PTHREAD_STACK_DEFAULT_SIZE;
	else
		size = param->stack_size;

    thrd = aosl_malloc(sizeof(aosl_pthread_t));
    if(thrd == NULL)
        return -1;
    memset(thrd, 0x00, sizeof(aosl_pthread_t));

    INIT_LIST_HEAD(&thrd->task_list);
    *thread = (aosl_thread_t)thrd;
    // thrd->state = PTHREAD_TASK_STATE_READY;
    thrd->func = (OS_ThreadEntry_t)entry;
    thrd->arg = args;

	if (!OS_MutexIsValid(&aosl_pthread_task_lock))
        OS_MutexCreate(&aosl_pthread_task_lock);

    OS_MutexLock(&aosl_pthread_task_lock, OS_WAIT_FOREVER);
    ret = OS_ThreadCreate(&thrd->thd,
                          "pthread",
                          (OS_ThreadEntry_t)thread_wrapper,
                          thrd,
                          prio,
                          size);

    if(ret != OS_OK)
    {
        aosl_free(thrd);
        ret = -1;
		AOSL_LOG_ERR("thread create failed\n");
    } 
	else
	{
		list_add(&thrd->task_list, &aosl_pthread_task_head);
	}
    OS_MutexUnlock(&aosl_pthread_task_lock);

    return ret;

#endif
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
	UNUSED(thread);
}

void aosl_hal_thread_exit(void *retval)
{
#if !__PTHREAD__JOIN___
	UNUSED(retval);
	vTaskDelete(NULL);
#else
	if (retval != NULL)
        AOSL_LOG_ERR("pthread_exit pass arg to pthread join is not realized\n");

    struct list_head *pos;
    struct list_head *n;
    aosl_pthread_t* iter = NULL;
    OS_ThreadHandle_t hdl = OS_ThreadGetCurrentHandle();

    OS_MutexLock(&aosl_pthread_task_lock, OS_WAIT_FOREVER);
    list_for_each_safe(pos, n, &aosl_pthread_task_head)
    {
        iter = list_entry(pos, aosl_pthread_t, task_list);

        if(iter->thd.handle == hdl) {
            
            list_del(pos);
            OS_Thread_t temp_thd = iter->thd;
            aosl_free(iter);
            OS_MutexUnlock(&aosl_pthread_task_lock);
            OS_ThreadDelete(&temp_thd);
            break;
        }
    }
    OS_MutexUnlock(&aosl_pthread_task_lock);
#endif
}
aosl_thread_t aosl_hal_thread_self(void)
{
#if !__PTHREAD__JOIN___
	return (aosl_thread_t)xTaskGetCurrentTaskHandle();
#else
	struct list_head *pos;
    struct list_head *n;
    aosl_pthread_t* iter = NULL;
	OS_ThreadHandle_t hdl = OS_ThreadGetCurrentHandle();
	OS_MutexLock(&aosl_pthread_task_lock, OS_WAIT_FOREVER);
    
    list_for_each_safe(pos, n, &aosl_pthread_task_head)
    {
        iter = list_entry(pos, aosl_pthread_t, task_list);
        if(iter->thd.handle == hdl) {
            
            OS_MutexUnlock(&aosl_pthread_task_lock);
            return (aosl_thread_t)iter;
        }
    }
    OS_MutexUnlock(&aosl_pthread_task_lock);

    AOSL_LOG_ERR("Thread self: NO WAY HERE!!! \n");

    return (aosl_thread_t)NULL;
#endif
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
#if !__PTHREAD__JOIN___
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
#else

	UNUSED(priority);
	return 0;
#endif
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
#if !__PTHREAD__JOIN___
	UNUSED(thread);
	UNUSED(retval);
	return 0;
#else
	struct list_head *pos;
    struct list_head *n;
    aosl_pthread_t* iter = NULL;
    int exit = 0;
    int timeout = 20000;

    while (!exit) {
        exit = 1;
        OS_MutexLock(&aosl_pthread_task_lock, OS_WAIT_FOREVER);
        list_for_each_safe(pos, n, &aosl_pthread_task_head)
        {
            iter = list_entry(pos, aosl_pthread_t, task_list);
            if(iter == (aosl_pthread_t*)thread) {
                exit = 0;
                break;
            }
        }
        OS_MutexUnlock(&aosl_pthread_task_lock);

        OS_MSleep(10);
        timeout -= 10;
        if (timeout <= 0) {
            AOSL_LOG_ERR("thread %p can not exit\n", ((aosl_pthread_t*)thread)->thd.handle);
            return -1;
        }
    }
    return 0;
#endif
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

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
	if (!mutex) {
		return -1;
	}

	// Use FreeRTOS v10.2.1 static mutex creation
	SemaphoreHandle_t handle = xSemaphoreCreateMutexStatic((StaticSemaphore_t*)&mutex->opaque);
	if (!handle) {
		return -1;
	}

	return 0;
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
