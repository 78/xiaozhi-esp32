#include <assert.h>
#include <stdio.h>
#include <ipro_osal.h>
#include <FreeRTOS_POSIX.h>
#include <pthread.h>
#include <time.h>
#include <semphr.h>

#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_thread.h>

typedef struct {
  void *(*entry)(void *);
  void *arg;
  TaskHandle_t task_handle;
  SemaphoreHandle_t join_sem;
  void *retval;
  volatile int completed;
  volatile int detached;
  volatile int joined;  // Flag to indicate if join has been called
} thread_wrapper_args_t;

static void thread_wrapper(void *arg)
{
  thread_wrapper_args_t *wrapper_args = (thread_wrapper_args_t *)arg;
  void *(*entry)(void *) = wrapper_args->entry;
  void *user_arg = wrapper_args->arg;
  SemaphoreHandle_t join_sem = wrapper_args->join_sem;
  int is_detached;
  
  void *retval = entry(user_arg);
  
  wrapper_args->retval = retval;
  wrapper_args->completed = 1;
  
  // Check if detached after setting completed flag
  is_detached = wrapper_args->detached;
  
  // Signal that thread has completed if not detached
  if (join_sem && !is_detached) {
    xSemaphoreGive(join_sem);
    
    // Wait for join to complete cleanup (with timeout to prevent hanging)
    // Use task notification to wait for join to signal us
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
  }
  
  // If detached, clean up resources now
  if (is_detached) {
    // Clear task tag first
    vTaskSetApplicationTaskTag(NULL, NULL);
    
    if (join_sem) {
      vSemaphoreDelete(join_sem);
    }
    aosl_free(wrapper_args);
  }
  
  // Task will be deleted by FreeRTOS
  ipro_osal_task_delete(NULL);
}

int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
                           void *(*entry)(void *), void *arg)
{
  assert(sizeof(ipro_osal_task_t) <= sizeof(aosl_thread_t));
  
  thread_wrapper_args_t *wrapper_args = aosl_malloc(sizeof(thread_wrapper_args_t));
  if (!wrapper_args) {
    return -1;
  }
  
  // Create semaphore for join
  SemaphoreHandle_t join_sem = xSemaphoreCreateBinary();
  if (!join_sem) {
    aosl_free(wrapper_args);
    return -1;
  }
  
  wrapper_args->entry = entry;
  wrapper_args->arg = arg;
  wrapper_args->join_sem = join_sem;
  wrapper_args->retval = NULL;
  wrapper_args->completed = 0;
  wrapper_args->detached = 0;
  wrapper_args->joined = 0;
  wrapper_args->task_handle = NULL;
  
  ipro_osal_task_t task;
  int ret = ipro_osal_task_create(&task, param->name, 
                                  (ipro_osal_task_fn_t)thread_wrapper,
                                  wrapper_args, 32 * 1024, 2);
  if (ret != IPRO_OSAL_OK) {
    vSemaphoreDelete(join_sem);
    aosl_free(wrapper_args);
    return -1;
  }
  
  // Store task handle in wrapper_args and return task handle as thread ID
  wrapper_args->task_handle = (TaskHandle_t)task.handle;
  
  // Store wrapper_args in task tag for join to find it
  vTaskSetApplicationTaskTag((TaskHandle_t)task.handle, (TaskHookFunction_t)wrapper_args);
  
  *thread = (aosl_thread_t)task.handle;
  return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
  // Note: After join or detach, the thread may have already deleted itself
  // Calling destroy on an already-deleted task handle may cause issues
  // For safety, we make this a no-op since join already cleans up resources
  // If the thread is still running (not joined/detached), this is typically an error
  // in the calling code, but we'll allow it for compatibility
  
  if (!thread) {
    return;
  }
  
  // Check if task still exists before trying to delete
  TaskHandle_t task_handle = (TaskHandle_t)thread;
  thread_wrapper_args_t *wrapper_args = (thread_wrapper_args_t *)xTaskGetApplicationTaskTag(task_handle);
  
  // If wrapper_args is NULL or completed, thread has already been cleaned up
  if (!wrapper_args || wrapper_args->completed) {
    return;
  }
  
  // Thread is still running - delete it (this is unusual)
  ipro_osal_task_t task;
  task.handle = (void *)thread;
  ipro_osal_task_delete(&task);
}

void aosl_hal_thread_exit(void *retval)
{
  ipro_osal_task_delete(NULL);
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
  (void)name;
  (void)size;
  return -1;
}

int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority)
{
  return 0;
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
  if (!thread) {
    return -1;
  }
  
  // Get wrapper_args from task tag
  TaskHandle_t task_handle = (TaskHandle_t)thread;
  thread_wrapper_args_t *wrapper_args = (thread_wrapper_args_t *)xTaskGetApplicationTaskTag(task_handle);
  
  if (!wrapper_args || !wrapper_args->join_sem) {
    return -1;
  }
  
  // Check if already joined or detached
  if (wrapper_args->joined || wrapper_args->detached) {
    return -1;
  }
  
  // Wait for thread to complete
  if (xSemaphoreTake(wrapper_args->join_sem, portMAX_DELAY) == pdTRUE) {
    if (retval) {
      *retval = wrapper_args->retval;
    }
    
    // Mark as joined
    wrapper_args->joined = 1;
    
    // Clear task tag before cleanup
    vTaskSetApplicationTaskTag(task_handle, NULL);
    
    // Notify the thread it can exit (if still waiting)
    xTaskNotifyGive(task_handle);
    
    // Clean up
    vSemaphoreDelete(wrapper_args->join_sem);
    aosl_free(wrapper_args);
    
    return 0;
  }
  
  return -1;
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
  if (!thread) {
    return;
  }
  
  // Get wrapper_args from task tag
  TaskHandle_t task_handle = (TaskHandle_t)thread;
  thread_wrapper_args_t *wrapper_args = (thread_wrapper_args_t *)xTaskGetApplicationTaskTag(task_handle);
  
  if (!wrapper_args) {
    return;
  }
  
  // Check if already joined
  if (wrapper_args->joined) {
    return;
  }
  
  // Mark as detached
  wrapper_args->detached = 1;
  
  // If thread already completed, clean up immediately
  if (wrapper_args->completed) {
    // Clear task tag before cleanup
    vTaskSetApplicationTaskTag(task_handle, NULL);
    
    if (wrapper_args->join_sem) {
      vSemaphoreDelete(wrapper_args->join_sem);
    }
    aosl_free(wrapper_args);
  }
  // Otherwise, thread will clean up itself when it completes
}

aosl_mutex_t aosl_hal_mutex_create()
{
  pthread_mutex_t *n_mutex = aosl_calloc(1, sizeof(pthread_mutex_t));
  if (NULL == n_mutex) {
    return NULL;
  }

  int err = pthread_mutex_init(n_mutex, NULL);
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
  if (NULL == n_mutex) {
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
  // Empty implementation for ipro7 platform
  // This platform does not support static mutex initialization
  return 0;
}

void aosl_hal_static_mutex_fini(aosl_static_mutex_t *mutex)
{
  (void)mutex;
}

aosl_cond_t aosl_hal_cond_create(void)
{
  pthread_cond_t *n_cond = aosl_calloc(1, sizeof(pthread_cond_t));
  if (NULL == n_cond) {
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
  if (NULL == n_cond) {
    return;
  }

  pthread_cond_destroy(n_cond);
  aosl_free(n_cond);
}

int aosl_hal_cond_signal(aosl_cond_t cond)
{
  pthread_cond_t *n_cond = (pthread_cond_t *)cond;
  if (NULL == n_cond) {
    return -1;
  }

  return pthread_cond_signal(n_cond);
}

int aosl_hal_cond_broadcast(aosl_cond_t cond)
{
  pthread_cond_t *n_cond = (pthread_cond_t *)cond;
  if (NULL == n_cond) {
    return -1;
  }

  return pthread_cond_broadcast(n_cond);
}

int aosl_hal_cond_wait(aosl_cond_t cond, aosl_mutex_t mutex)
{
  pthread_cond_t *n_cond = (pthread_cond_t *)cond;
  pthread_mutex_t *n_mutex = (pthread_mutex_t *)mutex;

  if (NULL == n_cond || NULL == n_mutex) {
    return -1;
  }

  return pthread_cond_wait(n_cond, n_mutex);
}

int aosl_hal_cond_timedwait(aosl_cond_t cond, aosl_mutex_t mutex, intptr_t timeout)
{
  pthread_cond_t *n_cond = (pthread_cond_t *)cond;
  pthread_mutex_t *n_mutex = (pthread_mutex_t *)mutex;

  if (NULL == n_cond || NULL == n_mutex) {
    return -1;
  }

  struct timespec timeo;
  struct timespec now;
  clock_gettime (CLOCK_REALTIME, &now);
  timeo.tv_sec = now.tv_sec + timeout / 1000;
  timeo.tv_nsec = now.tv_nsec + (timeout % 1000) * 1000000;
  while (timeo.tv_nsec >= 1000000000) {
    timeo.tv_nsec -= 1000000000;
    timeo.tv_sec++;
  }

  return pthread_cond_timedwait(n_cond, n_mutex, &timeo);
}
