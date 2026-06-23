#ifndef __AOSL_HAL_CONFIG_H__
#define __AOSL_HAL_CONFIG_H__

/* Darwin/iOS uses kqueue to implement the epoll HAL interface */
#define AOSL_HAL_HAVE_EPOLL 1
#define AOSL_HAL_HAVE_POLL 1
#define AOSL_HAL_HAVE_SELECT 1

#define AOSL_HAL_HAVE_COND 1
/* iOS does not support POSIX unnamed semaphores (sem_init) */
#define AOSL_HAL_HAVE_SEM 1

#define AOSL_HAL_HAVE_HWRNG 0

#endif /* __AOSL_HAL_CONFIG_H__ */
