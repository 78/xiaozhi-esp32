/***************************************************************************
 * Module:	Multiplex queue iofd header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_MPQ_FD_H__
#define __AOSL_MPQ_FD_H__


#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_socket.h>
#include <api/aosl_mpq.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief The user provided read/write callback function, just use
 * AOSL_DEFAULT_READ_FN/AOSL_DEFAULT_WRITE_FN respectively
 * for read/write for most cases, except the read/write ops
 * are special, such as an 'ioctl' supplied read/write.
 * Please make sure that you can only do the actual I/O ops
 * in these callback functions, and return '-error' when
 * encounter errors, MUST NOT close the io fd in these
 * callback functions.
 **/
typedef isize_t (*aosl_fd_read_t) (aosl_fd_t fd, void *buf, size_t len, size_t extra_size, uintptr_t argc, uintptr_t argv []);
typedef isize_t (*aosl_fd_write_t) (aosl_fd_t fd, const void *buf, size_t len, size_t extra_size, uintptr_t argc, uintptr_t argv []);

#define AOSL_DEFAULT_READ_FN ((aosl_fd_read_t)1)
#define AOSL_DEFAULT_WRITE_FN ((aosl_fd_write_t)1)

/**
 * @brief The data packet check callback function is provided by the user for determining
 * whether the data has contained a complet user level packet, as following:
 * Parameters:
 *    data: the data buffer holding the packet
 *     len: the data length in bytes in the buffer
 *    argc: the args count when adding the fd
 *    argv: the args vector when adding the fd
 * Return value:
 *      <0: indicate some error occurs;
 *       0: no error, but the whole packet is not ready so far;
 *      >0: the whole packet length
 **/
typedef isize_t (*aosl_check_packet_t) (const void *data, size_t len, uintptr_t argc, uintptr_t argv []);

/**
 * @brief The iofd received data callback function type
 * Parameters:
 *    data: the data buffer holding the packet
 *     len: the data length in bytes in the buffer
 *    argc: the args count when adding the fd
 *    argv: the args vector when adding the fd
 * Return value:
 *    None.
 **/
typedef void (*aosl_fd_data_t) (void *data, size_t len, uintptr_t argc, uintptr_t argv []);

/**
 * @brief The iofd event callback function type
 * Parameters:
 *      fd: the fd
 *   event: if <0, then indicates an error occurs, and the error number is '-event';
 *          if >=0, then indicates an event needs to be reported to application;
 *    argc: the args count when adding the fd
 *    argv: the args vector when adding the fd
 * Return value:
 *    None.
 **/
/* event values when no error */
#define AOSL_IOFD_READY_FOR_WRITING 1
#define AOSL_IOFD_HUP 2
#define AOSL_IOFD_ERROR -20000

/**
 * @brief The fd event callback function.
 * Parameters:
 *      fd: the fd itself
 *   event: the reporting event, values are as following:
 *         <0: an error occurs:
 *             [-19999 ~ -1]: OS error and '-event' indicates the OS error number;
 *                 <= -20000: SDK defined errors such as HUP etc;
 *    argc: the args count passed in when adding the fd
 *    argv: the args vector passed in when adding the fd
 * Return value:
 *    None.
 **/
typedef void (*aosl_fd_event_t) (aosl_fd_t fd, int event, uintptr_t argc, uintptr_t argv []);

/**
 * @brief Add a file descriptor to the current mpq for async I/O monitoring.
 * @param [in] fd            the file descriptor to monitor
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] read_f        the read callback (use AOSL_DEFAULT_READ_FN for default)
 * @param [in] write_f       the write callback (use AOSL_DEFAULT_WRITE_FN for default)
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_add_fd (aosl_fd_t fd, size_t max_pkt_size,
								aosl_fd_read_t read_f, aosl_fd_write_t write_f,
							aosl_check_packet_t chk_pkt_f, aosl_fd_data_t data_f,
								aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Add a file descriptor to the specified mpq for async I/O monitoring.
 * @param [in] qid           the target mpq id
 * @param [in] fd            the file descriptor to monitor
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] read_f        the read callback (use AOSL_DEFAULT_READ_FN for default)
 * @param [in] write_f       the write callback (use AOSL_DEFAULT_WRITE_FN for default)
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_add_fd_on_q (aosl_mpq_t qid, aosl_fd_t fd, size_t max_pkt_size,
						aosl_fd_read_t read_f, aosl_fd_write_t write_f, aosl_check_packet_t chk_pkt_f,
								aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Write data to an mpq-managed fd asynchronously.
 * @param [in] fd   the fd to write to
 * @param [in] buf  the data buffer
 * @param [in] len  the number of bytes to write
 * @return          the number of bytes written, or <0 on failure
 **/
extern __aosl_api__ isize_t aosl_write (aosl_fd_t fd, const void *buf, size_t len);

/**
 * @brief Get the N-th argument of the mpq attached fd.
 * @param [in]  fd   the fd to retrieve the argument from
 * @param [in]  n    the argument index (0-based)
 * @param [out] arg  pointer to receive the N-th argument value
 * @return           0 on success, <0 on failure with errno set
 **/
extern __aosl_api__ int aosl_mpq_fd_arg (aosl_fd_t fd, uintptr_t n, uintptr_t *arg);

/**
 * @brief Close an mpq-managed file descriptor and remove it from the mpq.
 * @param [in] fd  the file descriptor to close
 * @return         0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_close (aosl_fd_t fd);

/**
 * @brief Remove a file descriptor from the mpq without closing it.
 * @param [in] fd  the file descriptor to remove
 * @return         0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_del_fd (aosl_fd_t fd);


#ifdef __cplusplus
}
#endif


#endif /* __AOSL_MPQ_FD_H__ */