#include <windows.h>
#include <process.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <api/aosl_defs.h>
#include <api/aosl_log.h>
#include <api/aosl_mm.h>
#include <hal/aosl_hal_thread.h>

typedef struct win_thread_node {
    struct win_thread_node *next;
    aosl_thread_t tid;
    HANDLE handle;
    void *retval;
    int detached;
    int finished;
} win_thread_node_t;

typedef struct win_thread_start {
    win_thread_node_t *node;
    void *(*entry)(void *);
    void *arg;
} win_thread_start_t;

static INIT_ONCE g_thread_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_thread_lock;
static win_thread_node_t *g_thread_list = NULL;

aosl_static_assert(sizeof(CRITICAL_SECTION) <= AOSL_STATIC_MUTEX_SIZE, static_mutex_size_check);

static BOOL CALLBACK init_thread_registry(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    (void)once;
    (void)param;
    (void)ctx;
    InitializeCriticalSection(&g_thread_lock);
    return TRUE;
}

static void thread_registry_init(void)
{
    InitOnceExecuteOnce(&g_thread_once, init_thread_registry, NULL, NULL);
}

static void thread_registry_lock(void)
{
    thread_registry_init();
    EnterCriticalSection(&g_thread_lock);
}

static void thread_registry_unlock(void)
{
    LeaveCriticalSection(&g_thread_lock);
}

static win_thread_node_t *find_thread_node(aosl_thread_t tid, win_thread_node_t **prev_out)
{
    win_thread_node_t *prev = NULL;
    win_thread_node_t *node = g_thread_list;

    while (node != NULL) {
        if (node->tid == tid) {
            if (prev_out) {
                *prev_out = prev;
            }
            return node;
        }
        prev = node;
        node = node->next;
    }

    if (prev_out) {
        *prev_out = NULL;
    }
    return NULL;
}

static void remove_thread_node(win_thread_node_t *prev, win_thread_node_t *node)
{
    if (!node) {
        return;
    }

    if (prev) {
        prev->next = node->next;
    } else {
        g_thread_list = node->next;
    }
    node->next = NULL;
}

static int set_thread_name_internal(const char *name)
{
    typedef HRESULT(WINAPI *set_thread_description_t)(HANDLE, PCWSTR);
    static set_thread_description_t set_thread_description = NULL;
    static int searched = 0;
    WCHAR name_w[64];
    int converted;

    if (!searched) {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (kernel32 != NULL) {
            set_thread_description = (set_thread_description_t)GetProcAddress(kernel32, "SetThreadDescription");
        }
        searched = 1;
    }

    if (!set_thread_description || !name) {
        return 0;
    }

    converted = MultiByteToWideChar(CP_UTF8, 0, name, -1, name_w, (int)(sizeof(name_w) / sizeof(name_w[0])));
    if (converted <= 0) {
        converted = MultiByteToWideChar(CP_ACP, 0, name, -1, name_w, (int)(sizeof(name_w) / sizeof(name_w[0])));
        if (converted <= 0) {
            return -1;
        }
    }

    return SUCCEEDED(set_thread_description(GetCurrentThread(), name_w)) ? 0 : -1;
}

static int get_thread_name_internal(char *name, size_t size)
{
    typedef HRESULT(WINAPI *get_thread_description_t)(HANDLE, PWSTR *);
    static get_thread_description_t get_thread_description = NULL;
    static int searched = 0;
    PWSTR name_w = NULL;
    int converted;

    if (!name || size == 0) {
        return -1;
    }

    name[0] = '\0';

    if (!searched) {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (kernel32 != NULL) {
            get_thread_description = (get_thread_description_t)GetProcAddress(kernel32, "GetThreadDescription");
        }
        searched = 1;
    }

    /* If the platform does not provide GetThreadDescription, skip fetching. */
    if (!get_thread_description) {
        return -1;
    }

    if (!SUCCEEDED(get_thread_description(GetCurrentThread(), &name_w)) || name_w == NULL) {
        return -1;
    }

    converted = WideCharToMultiByte(CP_UTF8, 0, name_w, -1, name, (int)size, NULL, NULL);
    if (converted <= 0) {
        converted = WideCharToMultiByte(CP_ACP, 0, name_w, -1, name, (int)size, NULL, NULL);
    }

    LocalFree(name_w);
    if (converted <= 0) {
        name[0] = '\0';
        return -1;
    }

    return 0;
}

static unsigned __stdcall thread_entry_wrapper(void *arg)
{
    win_thread_start_t *start = (win_thread_start_t *)arg;
    win_thread_node_t *node = start->node;
    void *retval = start->entry(start->arg);
    HANDLE handle_to_close = NULL;
    int free_node = 0;

    thread_registry_lock();
    node->retval = retval;
    node->finished = 1;
    if (node->detached) {
        win_thread_node_t *prev = NULL;
        win_thread_node_t *curr = find_thread_node(node->tid, &prev);
        if (curr != NULL) {
            remove_thread_node(prev, curr);
        }
        handle_to_close = node->handle;
        node->handle = NULL;
        free_node = 1;
    }
    thread_registry_unlock();

    aosl_free(start);

    if (handle_to_close != NULL) {
        CloseHandle(handle_to_close);
    }
    if (free_node) {
        aosl_free(node);
    }

    return (unsigned)(uintptr_t)retval;
}

int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
                           void *(*entry)(void *), void *arg)
{
    uintptr_t handle_raw;
    unsigned tid = 0;
    win_thread_node_t *node;
    win_thread_start_t *start;

    (void)param;

    if (!thread || !entry) {
        return -1;
    }

    node = (win_thread_node_t *)aosl_calloc(1, sizeof(*node));
    start = (win_thread_start_t *)aosl_calloc(1, sizeof(*start));
    if (!node || !start) {
        aosl_free(node);
        aosl_free(start);
        return -1;
    }

    start->node = node;
    start->entry = entry;
    start->arg = arg;

    handle_raw = _beginthreadex(NULL,
                                (unsigned)(param ? param->stack_size : 0),
                                thread_entry_wrapper,
                                start,
                                0,
                                &tid);
    if (handle_raw == 0) {
        aosl_free(start);
        aosl_free(node);
        return -1;
    }

    node->tid = (aosl_thread_t)tid;
    node->handle = (HANDLE)handle_raw;

    thread_registry_lock();
    node->next = g_thread_list;
    g_thread_list = node;
    thread_registry_unlock();

    *thread = node->tid;
    return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
    HANDLE handle_to_close = NULL;
    win_thread_node_t *node_to_free = NULL;

    thread_registry_lock();
    {
        win_thread_node_t *prev = NULL;
        win_thread_node_t *node = find_thread_node(thread, &prev);
        if (node != NULL && node->finished && !node->detached) {
            remove_thread_node(prev, node);
            handle_to_close = node->handle;
            node->handle = NULL;
            node_to_free = node;
        }
    }
    thread_registry_unlock();

    if (handle_to_close != NULL) {
        CloseHandle(handle_to_close);
    }
    if (node_to_free != NULL) {
        aosl_free(node_to_free);
    }
}

void aosl_hal_thread_exit(void *retval)
{
    _endthreadex((unsigned)(uintptr_t)retval);
}

aosl_thread_t aosl_hal_thread_self()
{
    return (aosl_thread_t)GetCurrentThreadId();
}

int aosl_hal_thread_set_name(const char *name)
{
    return set_thread_name_internal(name);
}

int aosl_hal_thread_get_name(char *name, size_t size)
{
    return get_thread_name_internal(name, size);
}

int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority)
{
    int os_priority;

    switch (priority) {
    case AOSL_THRD_PRI_LOW:
        os_priority = THREAD_PRIORITY_BELOW_NORMAL;
        break;
    case AOSL_THRD_PRI_NORMAL:
    case AOSL_THRD_PRI_DEFAULT:
        os_priority = THREAD_PRIORITY_NORMAL;
        break;
    case AOSL_THRD_PRI_HIGH:
        os_priority = THREAD_PRIORITY_ABOVE_NORMAL;
        break;
    case AOSL_THRD_PRI_HIGHEST:
        os_priority = THREAD_PRIORITY_HIGHEST;
        break;
    case AOSL_THRD_PRI_RT:
        os_priority = THREAD_PRIORITY_TIME_CRITICAL;
        break;
    default:
        return -1;
    }

    return SetThreadPriority(GetCurrentThread(), os_priority) ? 0 : -1;
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
    HANDLE handle;
    void *thread_retval = NULL;
    win_thread_node_t *node_to_free = NULL;

    thread_registry_lock();
    node_to_free = find_thread_node(thread, NULL);
    if (node_to_free == NULL || node_to_free->detached) {
        thread_registry_unlock();
        return -1;
    }
    handle = node_to_free->handle;
    thread_registry_unlock();

    if (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0) {
        return -1;
    }

    thread_registry_lock();
    {
        win_thread_node_t *prev = NULL;
        win_thread_node_t *node = find_thread_node(thread, &prev);
        if (node == NULL) {
            thread_registry_unlock();
            return -1;
        }
        thread_retval = node->retval;
        remove_thread_node(prev, node);
        node->handle = NULL;
        node_to_free = node;
    }
    thread_registry_unlock();

    CloseHandle(handle);
    if (retval) {
        *retval = thread_retval;
    }
    aosl_free(node_to_free);
    return 0;
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
    HANDLE handle_to_close = NULL;
    win_thread_node_t *node_to_free = NULL;

    thread_registry_lock();
    {
        win_thread_node_t *prev = NULL;
        win_thread_node_t *node = find_thread_node(thread, &prev);
        if (node == NULL || node->detached) {
            thread_registry_unlock();
            return;
        }

        node->detached = 1;
        handle_to_close = node->handle;
        node->handle = NULL;

        if (node->finished) {
            remove_thread_node(prev, node);
            node_to_free = node;
        }
    }
    thread_registry_unlock();

    if (handle_to_close != NULL) {
        CloseHandle(handle_to_close);
    }
    if (node_to_free != NULL) {
        aosl_free(node_to_free);
    }
}

aosl_mutex_t aosl_hal_mutex_create()
{
    CRITICAL_SECTION *mutex = (CRITICAL_SECTION *)aosl_malloc(sizeof(*mutex));
    if (!mutex) {
        return NULL;
    }

    InitializeCriticalSection(mutex);
    return (aosl_mutex_t)mutex;
}

void aosl_hal_mutex_destroy(aosl_mutex_t mutex)
{
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)mutex;
    if (!cs) {
        return;
    }

    DeleteCriticalSection(cs);
    aosl_free(cs);
}

int aosl_hal_mutex_lock(aosl_mutex_t mutex)
{
    if (!mutex) {
        return -1;
    }
    EnterCriticalSection((CRITICAL_SECTION *)mutex);
    return 0;
}

int aosl_hal_mutex_trylock(aosl_mutex_t mutex)
{
    if (!mutex) {
        return -1;
    }
    return TryEnterCriticalSection((CRITICAL_SECTION *)mutex) ? 0 : -1;
}

int aosl_hal_mutex_unlock(aosl_mutex_t mutex)
{
    if (!mutex) {
        return -1;
    }
    LeaveCriticalSection((CRITICAL_SECTION *)mutex);
    return 0;
}

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
    if (!mutex) {
        return -1;
    }

    InitializeCriticalSection((CRITICAL_SECTION *)mutex->opaque);
    return 0;
}

void aosl_hal_static_mutex_fini(aosl_static_mutex_t *mutex)
{
    if (mutex) {
        DeleteCriticalSection((CRITICAL_SECTION *)mutex->opaque);
    }
}

aosl_cond_t aosl_hal_cond_create(void)
{
    CONDITION_VARIABLE *cond = (CONDITION_VARIABLE *)aosl_malloc(sizeof(*cond));
    if (!cond) {
        return NULL;
    }

    InitializeConditionVariable(cond);
    return (aosl_cond_t)cond;
}

void aosl_hal_cond_destroy(aosl_cond_t cond)
{
    if (cond) {
        aosl_free(cond);
    }
}

int aosl_hal_cond_signal(aosl_cond_t cond)
{
    if (!cond) {
        return -1;
    }
    WakeConditionVariable((CONDITION_VARIABLE *)cond);
    return 0;
}

int aosl_hal_cond_broadcast(aosl_cond_t cond)
{
    if (!cond) {
        return -1;
    }
    WakeAllConditionVariable((CONDITION_VARIABLE *)cond);
    return 0;
}

int aosl_hal_cond_wait(aosl_cond_t cond, aosl_mutex_t mutex)
{
    if (!cond || !mutex) {
        return -1;
    }

    return SleepConditionVariableCS((CONDITION_VARIABLE *)cond,
                                    (CRITICAL_SECTION *)mutex,
                                    INFINITE)
               ? 0
               : -1;
}

int aosl_hal_cond_timedwait(aosl_cond_t cond, aosl_mutex_t mutex, intptr_t timeout_ms)
{
    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;

    if (!cond || !mutex) {
        return -1;
    }

    if (SleepConditionVariableCS((CONDITION_VARIABLE *)cond, (CRITICAL_SECTION *)mutex, timeout)) {
        return 0;
    }

    return (GetLastError() == ERROR_TIMEOUT) ? -1 : -1;
}

aosl_sem_t aosl_hal_sem_create(void)
{
    HANDLE sem = CreateSemaphoreA(NULL, 0, LONG_MAX, NULL);
    return (aosl_sem_t)sem;
}

void aosl_hal_sem_destroy(aosl_sem_t sem)
{
    if (sem) {
        CloseHandle((HANDLE)sem);
    }
}

int aosl_hal_sem_post(aosl_sem_t sem)
{
    if (!sem) {
        return -1;
    }
    return ReleaseSemaphore((HANDLE)sem, 1, NULL) ? 0 : -1;
}

int aosl_hal_sem_wait(aosl_sem_t sem)
{
    if (!sem) {
        return -1;
    }
    return (WaitForSingleObject((HANDLE)sem, INFINITE) == WAIT_OBJECT_0) ? 0 : -1;
}

int aosl_hal_sem_timedwait(aosl_sem_t sem, intptr_t timeout_ms)
{
    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result;

    if (!sem) {
        return -1;
    }

    result = WaitForSingleObject((HANDLE)sem, timeout);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
}
