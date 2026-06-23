
#include <RTOS/export/inc/os_api.h>
#include <assert.h>

#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_log.h>
#include <hal/aosl_hal_thread.h>

typedef struct {
  void *(*entry_func)(void *);
  void *arg;
} aosl_thread_entry_wrapper_args_t;

static void aosl_thread_entry_wrapper(uint32 argc, void* argv)
{
  aosl_thread_entry_wrapper_args_t *wrapper = (aosl_thread_entry_wrapper_args_t *)argv;
  if (wrapper != NULL && wrapper->entry_func != NULL) {
    wrapper->entry_func(wrapper->arg);
    aosl_free(wrapper);
  }
}

int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
                           void *(*entry)(void *), void *arg)
{
  BLOCK_ID thread_id;
  aosl_thread_entry_wrapper_args_t *wrapper = NULL;
  uint32 priority = 67;
  uint32 stack_size = 1024 * 16;

  if (NULL == thread || NULL == param || NULL == entry) {
    return -1;
  }

  wrapper = aosl_calloc(1, sizeof(aosl_thread_entry_wrapper_args_t));
  if (NULL == wrapper) {
    return -1;
  }

  wrapper->entry_func = entry;
  wrapper->arg        = arg;

  thread_id = SCI_CreateThread(param->name,                  // thread name
                               "aosl",                       // queue name (NULL if no queue needed)
                               aosl_thread_entry_wrapper,    // entry function
                               1,                            // argc (not used in wrapper)
                               wrapper,                      // argv (passes wrapper structure)
                               stack_size,                   // stack size
                               1000,                         // queue_num (no message queue)
                               priority,                     // priority
                               SCI_PREEMPT,                  // preemptable
                               SCI_AUTO_START);              // auto start
  if (thread_id == SCI_INVALID_BLOCK_ID) {
    aosl_free(wrapper);
    return -1;
  }

  *thread = (aosl_thread_t)thread_id;
  return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
  BLOCK_ID thread_id = (BLOCK_ID)thread;
  if (thread_id != SCI_INVALID_BLOCK_ID) {
    SCI_DeleteThread(thread_id);
  }
}

void aosl_hal_thread_exit(void *retval)
{
  SCI_ThreadExit();
}

aosl_thread_t aosl_hal_thread_self()
{
  return (aosl_thread_t)SCI_IdentifyThread();
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
  return 0;
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
  return;
}

aosl_mutex_t aosl_hal_mutex_create()
{
  SCI_MUTEX_PTR mutex_ptr = SCI_CreateMutex("aosl", SCI_INHERIT);
  if (NULL == mutex_ptr) {
    return NULL;
  }

  return (aosl_mutex_t)mutex_ptr;
}

void aosl_hal_mutex_destroy(aosl_mutex_t mutex)
{
  SCI_MUTEX_PTR mutex_ptr = (SCI_MUTEX_PTR)mutex;
  if (mutex_ptr == NULL) {
    return;
  }

  SCI_DeleteMutex(mutex_ptr);
}

int aosl_hal_mutex_lock(aosl_mutex_t mutex)
{
  SCI_MUTEX_PTR mutex_ptr = (SCI_MUTEX_PTR)mutex;
  uint32 ret;

  if (mutex_ptr == NULL) {
    return -1;
  }

  ret = SCI_GetMutex(mutex_ptr, SCI_WAIT_FOREVER);
  return (ret == SCI_SUCCESS) ? 0 : -1;
}

int aosl_hal_mutex_trylock(aosl_mutex_t mutex)
{
  SCI_MUTEX_PTR mutex_ptr = (SCI_MUTEX_PTR)mutex;
  uint32 ret;

  if (mutex_ptr == NULL) {
    return -1;
  }

  ret = SCI_GetMutex(mutex_ptr, SCI_NO_WAIT);
  return (ret == SCI_SUCCESS) ? 0 : -1;
}

int aosl_hal_mutex_unlock(aosl_mutex_t mutex)
{
  SCI_MUTEX_PTR mutex_ptr = (SCI_MUTEX_PTR)mutex;
  uint32 ret;

  if (mutex_ptr == NULL) {
    return -1;
  }

  ret = SCI_PutMutex(mutex_ptr);
  return (ret == SCI_SUCCESS) ? 0 : -1;
}

aosl_sem_t aosl_hal_sem_create(void)
{
  SCI_SEMAPHORE_PTR sem_ptr = SCI_CreateSemaphore("aosl_sem", 0);
  if (NULL == sem_ptr) {
    return NULL;
  }

  return (aosl_sem_t)sem_ptr;
}

void aosl_hal_sem_destroy(aosl_sem_t sem)
{
  SCI_SEMAPHORE_PTR sem_ptr = (SCI_SEMAPHORE_PTR)sem;
  if (sem_ptr == NULL) {
    return;
  }

  SCI_DeleteSemaphore(sem_ptr);
}

int aosl_hal_sem_post(aosl_sem_t sem)
{
  SCI_SEMAPHORE_PTR sem_ptr = (SCI_SEMAPHORE_PTR)sem;
  uint32 ret;

  if (sem_ptr == NULL) {
    return -1;
  }

  ret = SCI_PutSemaphore(sem_ptr);
  return (ret == SCI_SUCCESS) ? 0 : -1;
}

int aosl_hal_sem_wait(aosl_sem_t sem)
{
  SCI_SEMAPHORE_PTR sem_ptr = (SCI_SEMAPHORE_PTR)sem;
  uint32 ret;

  if (sem_ptr == NULL) {
    return -1;
  }

  ret = SCI_GetSemaphore(sem_ptr, SCI_WAIT_FOREVER);
  return (ret == SCI_SUCCESS) ? 0 : -1;
}

int aosl_hal_sem_timedwait(aosl_sem_t sem, intptr_t timeout_ms)
{
  SCI_SEMAPHORE_PTR sem_ptr = (SCI_SEMAPHORE_PTR)sem;
  uint32 wait_option;
  uint32 ret;

  if (sem_ptr == NULL) {
    return -1;
  }

  if (timeout_ms < 0) {
    wait_option = SCI_WAIT_FOREVER;
  } else if (timeout_ms == 0) {
    wait_option = 1;
  } else {
    wait_option = (uint32)timeout_ms;
  }

  ret = SCI_GetSemaphore(sem_ptr, wait_option);
  return (ret == SCI_SUCCESS) ? 0 : -1;
}

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
  // Empty implementation for platforms that don't support static mutex
  (void)mutex;
  return 0;
}

void aosl_hal_static_mutex_fini(aosl_static_mutex_t *mutex)
{
  (void)mutex;
}
