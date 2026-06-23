#include <string.h>

#include <ql_rtos.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_thread.h>

#define ASR1606_DEFAULT_STACK_SIZE (8 * 1024)
#define ASR1606_DEFAULT_PRIORITY   10

/* Wrapper to convert pthread-style void*(*)(void*) entry to ThreadX void(*)(void*) */
typedef struct {
  void *(*entry)(void *);
  void *arg;
} thread_wrapper_args_t;

static void thread_wrapper(void *arg)
{
  thread_wrapper_args_t *wrapper_args = (thread_wrapper_args_t *)arg;
  void *(*entry)(void *) = wrapper_args->entry;
  void *user_arg = wrapper_args->arg;

  aosl_free(wrapper_args);

  entry(user_arg);

  /* ThreadX tasks must not return; delete self */
  ql_rtos_task_delete(NULL);
}

int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
                           void *(*entry)(void *), void *arg)
{
  thread_wrapper_args_t *wrapper_args = aosl_malloc(sizeof(thread_wrapper_args_t));
  if (!wrapper_args) {
    return -1;
  }
  wrapper_args->entry = entry;
  wrapper_args->arg = arg;

  u32 stack_size = (param && param->stack_size > 0)
                     ? param->stack_size : ASR1606_DEFAULT_STACK_SIZE;
  u8 priority = ASR1606_DEFAULT_PRIORITY;
  const char *name = (param && param->name) ? param->name : "aosl";

  ql_task_t task = NULL;
  QlOSStatus ret = ql_rtos_task_create(&task, stack_size, priority,
                                       (char *)name, thread_wrapper, wrapper_args);
  if (ret != 0 || task == NULL) {
    aosl_free(wrapper_args);
    return -1;
  }

  *thread = (aosl_thread_t)task;
  return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
  /* Task self-deletes in thread_wrapper; nothing to do here */
  (void)thread;
}

void aosl_hal_thread_exit(void *retval)
{
  (void)retval;
  ql_rtos_task_delete(NULL);
}

aosl_thread_t aosl_hal_thread_self(void)
{
  ql_task_t task = NULL;
  ql_rtos_task_get_current_ref(&task);
  return (aosl_thread_t)task;
}

int aosl_hal_thread_set_name(const char *name)
{
  ql_task_t task = NULL;
  ql_rtos_task_get_current_ref(&task);
  if (task && name) {
    ql_rtos_task_set_name(task, (char *)name);
    return 0;
  }
  return -1;
}

int aosl_hal_thread_get_name(char *name, size_t size)
{
  (void)name;
  (void)size;
  return -1;
}

int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority)
{
  (void)priority;
  return 0;
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
  /* Kernel layer uses its own cond-based sync; HAL join is not called */
  (void)thread;
  (void)retval;
  return 0;
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
  (void)thread;
}

/* --- Mutex (via ql_rtos_mutex) --- */

aosl_mutex_t aosl_hal_mutex_create()
{
  ql_mutex_t mutex = NULL;
  QlOSStatus ret = ql_rtos_mutex_create(&mutex);
  if (ret != 0) {
    return NULL;
  }
  return (aosl_mutex_t)mutex;
}

void aosl_hal_mutex_destroy(aosl_mutex_t mutex)
{
  if (mutex) {
    ql_rtos_mutex_delete((ql_mutex_t)mutex);
  }
}

int aosl_hal_mutex_lock(aosl_mutex_t mutex)
{
  return ql_rtos_mutex_lock((ql_mutex_t)mutex, QL_WAIT_FOREVER);
}

int aosl_hal_mutex_trylock(aosl_mutex_t mutex)
{
  return ql_rtos_mutex_try_lock((ql_mutex_t)mutex);
}

int aosl_hal_mutex_unlock(aosl_mutex_t mutex)
{
  return ql_rtos_mutex_unlock((ql_mutex_t)mutex);
}

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
  if (!mutex) {
    return -1;
  }

  /*
   * Create a dynamic mutex and store the handle in the opaque array.
   * The handle is a void* which fits within 128 bytes.
   */
  ql_mutex_t m = NULL;
  QlOSStatus ret = ql_rtos_mutex_create(&m);
  if (ret != 0) {
    return -1;
  }

  memset(mutex->opaque, 0, AOSL_STATIC_MUTEX_SIZE);
  memcpy(mutex->opaque, &m, sizeof(ql_mutex_t));
  return 0;
}

/* --- Semaphore (ThreadX native, used instead of condition variables) --- */

aosl_sem_t aosl_hal_sem_create(void)
{
  ql_sem_t sem = NULL;
  QlOSStatus ret = ql_rtos_semaphore_create(&sem, 0);
  if (ret != 0) {
    return NULL;
  }
  return (aosl_sem_t)sem;
}

void aosl_hal_sem_destroy(aosl_sem_t sem)
{
  if (sem) {
    ql_rtos_semaphore_delete((ql_sem_t)sem);
  }
}

int aosl_hal_sem_post(aosl_sem_t sem)
{
  return ql_rtos_semaphore_release((ql_sem_t)sem);
}

int aosl_hal_sem_wait(aosl_sem_t sem)
{
  return ql_rtos_semaphore_wait((ql_sem_t)sem, QL_WAIT_FOREVER);
}

int aosl_hal_sem_timedwait(aosl_sem_t sem, intptr_t timeout)
{
  return ql_rtos_semaphore_wait((ql_sem_t)sem, (u32)timeout);
}
