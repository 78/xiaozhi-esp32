/***************************************************************************
 * Module:	Io multiplexing hal definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_IOMP_H__
#define __AOSL_HAL_IOMP_H__

#include <stdint.h>
#include <stdbool.h>
#include <hal/aosl_hal_config.h>
#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief epoll control operations
 */
typedef enum {
	AOSL_POLL_CTL_ADD = 1,
	AOSL_POLL_CTL_MOD = 2,
	AOSL_POLL_CTL_DEL = 3
} aosl_epoll_op_e;

/**
 * @brief poll event types
 */
typedef enum {
	AOSL_POLLIN = 1 << 0,
	AOSL_POLLOUT = 1 << 1,
	AOSL_POLLERR = 1 << 2,
	AOSL_POLLHUP = 1 << 3,
	AOSL_POLLET = 1 << 4,
} aosl_poll_type_e;

/**
 * @brief poll event structure
 */
typedef struct {
	aosl_fd_t fd;
	uint32_t events;
	uint32_t revents; // only for poll
} aosl_poll_event_t;

/**
 * @note Implement at least one of epoll/poll/select
 *       and set AOSL_HAL_HAVE_EPOLL, AOSL_HAL_HAVE_POLL or AOSL_HAL_HAVE_SELECT in aosl_hal_config.h
 */

/**
 * @brief create epoll instance
 * @return epoll file descriptor, or -1 on error
 */
int aosl_hal_epoll_create(void);

/**
 * @brief destroy epoll instance
 * @param [in] epfd epoll file descriptor
 * @return 0 on success, or -1 on error
 */
int aosl_hal_epoll_destroy(int epfd);

/**
 * @brief 	control interface for epoll instance
 * @param [in] epfd epoll instance
 * @param [in] op   epoll operation
 * @param [in] fd   file descriptor to be controlled
 * @param [out] ev  event lists
 * @return 0 on success, or -1 on error
 */
int aosl_hal_epoll_ctl(int epfd, aosl_epoll_op_e op, aosl_fd_t fd, aosl_poll_event_t *ev);

/**
 * @brief wait for events on epoll instance
 * @param [in] epfd epoll instance
 * @param [out] evlist event lists happend
 * @param [in] maxevents evlist buffer size
 * @param [in] timeout timeout in milliseconds, -1 means infinite wait
 * @return event counts on success, or -1 on error
 */
int aosl_hal_epoll_wait(int epfd, aosl_poll_event_t *evlist, int maxevents, int timeout_ms);

/**
 * @brief wait for events on poll instance
 * @param [in/out] fds event lists
 * @param nfds    fds size
 * @param timeout_ms timeout in milliseconds, -1 means infinite wait
 * @return event counts on success, or -1 on error
 */
int aosl_hal_poll(aosl_poll_event_t fds[], int nfds, int timeout_ms);

/**
 * @brief fd_set type
 */
typedef void* fd_set_t;
/**
 * @brief create fd_set
 * @return fd_set handle, or NULL on error
 */
fd_set_t aosl_hal_fdset_create(void);

/**
 * @brief destroy fd_set
 * @param [in] fdset fd_set handle
 */
void aosl_hal_fdset_destroy(fd_set_t fdset);

/**
 * @brief clear all fd in fd_set
 * @param [in/out] fdset fd set handle
 */
void aosl_hal_fdset_zero(fd_set_t fdset);

/**
 * @brief 	set fd in fd_set
 * @param [in/out] fdset fd set handle
 * @param [in] fd  file descriptor to be set
 */
void aosl_hal_fdset_set(fd_set_t fdset, aosl_fd_t fd);

/**
 * @brief clear fd in fd_set
 * @param [in/out] fdset fd set handle
 * @param [in] fd  file descriptor to be cleared
 */
void aosl_hal_fdset_clr(fd_set_t fdset, aosl_fd_t fd);

/**
 * @brief check if fd is set in fd_set
 * @param [in] fdset fd set handle
 * @param [in] fd  file descriptor to be checked
 * @return non-zero if fd is set, otherwise zero
 */
int aosl_hal_fdset_isset(fd_set_t fdset, aosl_fd_t fd);

/**
 * @brief select function
 * @param [in] nfds highest-numbered file descriptor plus 1
 * @param [in/out] readfds fd_set for read events
 * @param [in/out] writefds fd_set for write events
 * @param [in/out] exceptfds fd_set for except events
 * @param [in] timeout_ms timeout in milliseconds, -1 means infinite wait
 * @return number of ready descriptors on success, or -1 on error
 */
int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds, fd_set_t exceptfds, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __AOSL_HAL_IOMP_H__ */
