/*
 * Darwin/iOS HAL thread implementation
 *
 * Key differences from Linux:
 * - pthread_setname_np() takes only name (not thread + name)
 * - No prctl(PR_SET_NAME)
 * - No POSIX unnamed semaphores (sem_init/sem_destroy)
 *   -> Use dispatch_semaphore instead
 * - No pthread_condattr_setclock(CLOCK_MONOTONIC)
 *   -> Use pthread_cond_timedwait_relative_np() for relative timeout
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_thread.h>

aosl_static_assert(sizeof(pthread_mutex_t) <= AOSL_STATIC_MUTEX_SIZE,
                   static_mutex_size_check);

int aosl_hal_thread_create(aosl_thread_t *thread, aosl_thread_param_t *param,
                           void *(*entry)(void *), void *arg)
{
	assert(sizeof(pthread_t) <= sizeof(aosl_thread_t));
	pthread_t n_td;
	int err = pthread_create(&n_td, NULL, entry, arg);
	if (err != 0) {
		AOSL_LOG_ERR("create failed, err=%d", err);
		return -1;
	}
	*thread = (aosl_thread_t)n_td;
	return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread) {}

void aosl_hal_thread_exit(void *retval)
{
	pthread_exit(retval);
}

aosl_thread_t aosl_hal_thread_self()
{
	return (aosl_thread_t)pthread_self();
}

int aosl_hal_thread_set_name(const char *name)
{
	/* Darwin: pthread_setname_np sets name for the calling thread only */
	return pthread_setname_np(name);
}

int aosl_hal_thread_get_name(char *name, size_t size)
{
	if (!name || size == 0) {
		return -1;
	}

	return pthread_getname_np(pthread_self(), name, size);
}

static int __get_os_priority(aosl_thread_proiority_e aosl_pri, int *os_pri)
{
	int min_prio = sched_get_priority_min(SCHED_RR);
	int max_prio = sched_get_priority_max(SCHED_RR);
	if (max_prio - min_prio <= 2) return -1;

	int top_prio = max_prio - 1;
	int low_prio = min_prio + 1;

	switch (aosl_pri) {
	case AOSL_THRD_PRI_LOW:     *os_pri = low_prio; break;
	case AOSL_THRD_PRI_NORMAL:  *os_pri = (low_prio + top_prio - 1) / 2; break;
	case AOSL_THRD_PRI_HIGH:    *os_pri = (top_prio - 2 > low_prio) ? top_prio - 2 : low_prio; break;
	case AOSL_THRD_PRI_HIGHEST: *os_pri = (top_prio - 1 > low_prio) ? top_prio - 1 : low_prio; break;
	case AOSL_THRD_PRI_RT:      *os_pri = top_prio; break;
	default: return -1;
	}
	return 0;
}

int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority)
{
	int os_pri;
	if (__get_os_priority(priority, &os_pri) != 0) return -1;

	struct sched_param param;
	param.sched_priority = os_pri;
	int err = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
	if (err != 0) {
		AOSL_LOG_ERR("set priority failed, err=%d", err);
		return err;
	}
	return 0;
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
	return pthread_join((pthread_t)thread, retval);
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
	pthread_detach((pthread_t)thread);
}

aosl_mutex_t aosl_hal_mutex_create()
{
	pthread_mutex_t *m = aosl_calloc(1, sizeof(pthread_mutex_t));
	if (!m) return NULL;
	if (pthread_mutex_init(m, NULL) != 0) {
		AOSL_LOG_ERR("mutex create failed");
		aosl_free(m);
		return NULL;
	}
	return (aosl_mutex_t)m;
}

void aosl_hal_mutex_destroy(aosl_mutex_t mutex)
{
	if (!mutex) return;
	pthread_mutex_destroy((pthread_mutex_t *)mutex);
	aosl_free(mutex);
}

int aosl_hal_mutex_lock(aosl_mutex_t mutex)    { return pthread_mutex_lock((pthread_mutex_t *)mutex); }
int aosl_hal_mutex_trylock(aosl_mutex_t mutex)  { return pthread_mutex_trylock((pthread_mutex_t *)mutex); }
int aosl_hal_mutex_unlock(aosl_mutex_t mutex)   { return pthread_mutex_unlock((pthread_mutex_t *)mutex); }

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
	if (!mutex) return -1;
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
	pthread_cond_t *c = aosl_calloc(1, sizeof(pthread_cond_t));
	if (!c) return NULL;

	/*
	 * Darwin does not support pthread_condattr_setclock(CLOCK_MONOTONIC).
	 * We use default attributes and handle timeout with relative wait.
	 */
	if (pthread_cond_init(c, NULL) != 0) {
		AOSL_LOG_ERR("cond create failed");
		aosl_free(c);
		return NULL;
	}
	return (aosl_cond_t)c;
}

void aosl_hal_cond_destroy(aosl_cond_t cond)
{
	if (!cond) return;
	pthread_cond_destroy((pthread_cond_t *)cond);
	aosl_free(cond);
}

int aosl_hal_cond_signal(aosl_cond_t cond)
{
	return cond ? pthread_cond_signal((pthread_cond_t *)cond) : -1;
}

int aosl_hal_cond_broadcast(aosl_cond_t cond)
{
	return cond ? pthread_cond_broadcast((pthread_cond_t *)cond) : -1;
}

int aosl_hal_cond_wait(aosl_cond_t cond, aosl_mutex_t mutex)
{
	if (!cond || !mutex) return -1;
	return pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}

int aosl_hal_cond_timedwait(aosl_cond_t cond, aosl_mutex_t mutex, intptr_t timeout_ms)
{
	if (!cond || !mutex) return -1;

	/*
	 * Use gettimeofday + absolute time since Darwin doesn't support
	 * CLOCK_MONOTONIC condattr. This is the standard Darwin approach.
	 */
	struct timeval now;
	gettimeofday(&now, NULL);

	struct timespec abstime;
	abstime.tv_sec = now.tv_sec + timeout_ms / 1000;
	abstime.tv_nsec = (now.tv_usec * 1000) + (timeout_ms % 1000) * 1000000;
	while (abstime.tv_nsec >= 1000000000) {
		abstime.tv_nsec -= 1000000000;
		abstime.tv_sec++;
	}

	return pthread_cond_timedwait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex, &abstime);
}

/*
 * Semaphore implementation using dispatch_semaphore_t
 * iOS/macOS deprecated POSIX unnamed semaphores (sem_init/sem_destroy).
 */
aosl_sem_t aosl_hal_sem_create(void)
{
	dispatch_semaphore_t *sem = aosl_malloc(sizeof(dispatch_semaphore_t));
	if (!sem) return NULL;

	*sem = dispatch_semaphore_create(0);
	if (*sem == NULL) {
		aosl_free(sem);
		return NULL;
	}
	return (aosl_sem_t)sem;
}

void aosl_hal_sem_destroy(aosl_sem_t sem)
{
	if (!sem) return;
	/* dispatch_semaphore is ARC-managed on newer SDKs, but we release manually for MRC */
	dispatch_semaphore_t *ds = (dispatch_semaphore_t *)sem;
	/* dispatch_release is needed in non-ARC builds */
#if !__has_feature(objc_arc)
	if (*ds) dispatch_release(*ds);
#endif
	aosl_free(ds);
}

int aosl_hal_sem_post(aosl_sem_t sem)
{
	if (!sem) return -1;
	dispatch_semaphore_signal(*(dispatch_semaphore_t *)sem);
	return 0;
}

int aosl_hal_sem_wait(aosl_sem_t sem)
{
	if (!sem) return -1;
	dispatch_semaphore_wait(*(dispatch_semaphore_t *)sem, DISPATCH_TIME_FOREVER);
	return 0;
}

int aosl_hal_sem_timedwait(aosl_sem_t sem, intptr_t timeout_ms)
{
	if (!sem) return -1;
	dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, timeout_ms * 1000000LL);
	long ret = dispatch_semaphore_wait(*(dispatch_semaphore_t *)sem, timeout);
	return (ret == 0) ? 0 : -1;
}
