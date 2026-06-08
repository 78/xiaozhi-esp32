/***************************************************************************
 * Module:	Socket helper utils implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>

#include <api/aosl_alloca.h>
#include <api/aosl_socket.h>
#include <api/aosl_time.h>
#include <api/aosl_mpq_net.h>

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/err.h>
#include <kernel/mp_queue.h>
#include <kernel/iofd.h>
#include <kernel/byteorder/generic.h>
#include <kernel/net.h>

#ifdef CONFIG_AOSL_IPV6
#include <kernel/ipv6.h>
#endif

// TODO(zgx): flags, fd had been set noblock, maybe not need this flag
#define MSG_DONTWAIT 0

#define UNUSED(expr) (void)(expr)

__export_in_so__ uint32_t aosl_htonl(uint32_t x)
{
	return aosl__htonl(x);
}
__export_in_so__ uint16_t aosl_htons(uint16_t x)
{
	return aosl__htons(x);
}
__export_in_so__ uint32_t aosl_ntohl(uint32_t x)
{
	return aosl__ntohl(x);
}
__export_in_so__ uint16_t aosl_ntohs(uint16_t x)
{
	return aosl__ntohs(x);
}

__export_in_so__ aosl_fd_t aosl_socket (int domain, int type, int protocol)
{
	return (aosl_fd_t)aosl_hal_sk_socket (domain, type, protocol);
}

__export_in_so__ int aosl_bind (aosl_fd_t sockfd, const aosl_sockaddr_t *addr)
{
	return aosl_hal_sk_bind (sockfd, addr);
}

#if 0
__export_in_so__ int aosl_getsockname (aosl_fd_t sockfd, aosl_sockaddr_t *addr)
{
	return aosl_hal_sk_getsockname (sockfd, addr);
}

__export_in_so__ int aosl_getpeername (aosl_fd_t sockfd, aosl_sockaddr_t *addr)
{
	return aosl_hal_sk_getpeername (sockfd, addr);
}

__export_in_so__ int aosl_getsockopt (aosl_fd_t sockfd, int level, int optname, void *optval, int *optlen)
{
	return aosl_hal_sk_getsockopt (sockfd, level, optname, optval, optlen);
}

__export_in_so__ int aosl_setsockopt (aosl_fd_t sockfd, int level, int optname, const void *optval, int optlen)
{
	return aosl_hal_sk_setsockopt (sockfd, level, optname, optval, optlen);
}
#endif

int aosl_get_sockaddr(aosl_fd_t sockfd, aosl_sockaddr_t *addr)
{
	// get port
	aosl_sockaddr_t sock_addr = {0};
	if (aosl_hal_sk_get_sockname(sockfd, &sock_addr) < 0) {
		return -1;
	}
	// get local ip
	if (aosl_hal_sk_get_local_ip(addr)) {
		return -1;
	}
	if (addr->sa_family != sock_addr.sa_family) {
		return -1;
	}
	addr->sa_port = sock_addr.sa_port;

	return 0;
}

struct recvfrom_args {
	aosl_sk_addr_t addr;
};

struct sendto_args {
	int flags;
	aosl_sk_addr_t addr;
};

static isize_t __default_accept (aosl_fd_t fd, void *buf, size_t len, size_t extra, uintptr_t argc, uintptr_t argv [])
{
	aosl_accept_data_t *accept_data = (aosl_accept_data_t *)buf;

	UNUSED (len);
	UNUSED (extra);
	UNUSED (argc);
	UNUSED (argv);

	accept_data->newsk = aosl_hal_sk_accept (fd, &accept_data->addr.sa);
	if (aosl_fd_invalid (accept_data->newsk)) {
		return aosl_hal_set_error(AOSL_HAL_RET_EHAL);
	}

	return sizeof (aosl_accept_data_t);
}

static isize_t __default_recv (aosl_fd_t fd, void *buf, size_t len, size_t extra, uintptr_t argc, uintptr_t argv [])
{
	int flags = MSG_DONTWAIT;
	isize_t err;

	UNUSED (extra);
	UNUSED (argc);
	UNUSED (argv);

	err = aosl_hal_sk_recv (fd, buf, len, flags);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return err;
}

static isize_t __default_send (aosl_fd_t fd, const void *buf, size_t len, size_t extra, uintptr_t argc, uintptr_t argv [])
{
	int flags = MSG_DONTWAIT;
	isize_t err;

	UNUSED (argc);
	UNUSED (argv);

	if (extra >= sizeof (flags))
		flags |= *(int *)AOSL_P_ALIGN_PTR ((char *)buf + len);

	err = aosl_hal_sk_send (fd, buf, len, flags);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return err;
}

static isize_t __default_recvfrom (aosl_fd_t fd, void *buf, size_t len, size_t extra, uintptr_t argc, uintptr_t argv [])
{
	isize_t err;

	UNUSED (argc);
	UNUSED (argv);

	if (extra >= sizeof (struct recvfrom_args)) {
		struct recvfrom_args *args = (struct recvfrom_args *)AOSL_P_ALIGN_PTR ((char *)buf + len);
		err = aosl_hal_sk_recvfrom (fd, buf, len, MSG_DONTWAIT, &args->addr.sa);
	} else {
		err = aosl_hal_sk_recv (fd, buf, len, MSG_DONTWAIT);
	}

	if (err < 0) {
		return aosl_hal_set_error(err);
	}

	return err;
}

static isize_t __default_sendto (aosl_fd_t fd, const void *buf, size_t len, size_t extra, uintptr_t argc, uintptr_t argv [])
{
	int flags = MSG_DONTWAIT;
	void *extra_data = AOSL_P_ALIGN_PTR ((char *)buf + len);
	isize_t err;

	UNUSED (argc);
	UNUSED (argv);

	if (extra >= sizeof (flags))
		flags |= *(int *)extra_data;

	if (extra >= sizeof (struct sendto_args)) {
		struct sendto_args *args = (struct sendto_args *)extra_data;
		err = aosl_hal_sk_sendto (fd, buf, len, flags, &args->addr.sa);
	} else {
		err = aosl_hal_sk_send (fd, buf, len, flags);
	}

	if (err < 0) {
		return aosl_hal_set_error(err);
	}

	return err;
}

static __inline__ int __do_connect (aosl_fd_t sockfd, const aosl_sockaddr_t *dest_addr)
{
	struct iofd *f;
	int err;

	err = aosl_hal_sk_connect (sockfd, dest_addr);
	if (err < 0) {
		if (err == AOSL_HAL_RET_EINPROGRESS) {
			return 0;
		}
		return aosl_hal_set_error(err);
	}

	f = iofd_get (sockfd);
	if (f != NULL) {
		__iofd_write_data (THIS_MPQ (), f);
		iofd_put (f);
	}

	return 0;
}

static int __this_q_connect_argv (struct mp_queue *q, aosl_fd_t fd, const aosl_sockaddr_t *dest_addr,
														int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
										aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, uintptr_t *argv)
{
	int err;

	err = __mpq_add_fd_argv (q, fd, timeo, max_pkt_size, 0, IOFD_NOT_READY, __default_recv, __default_send, chk_pkt_f, NULL, data_f, event_f, argc, argv);
	if (err < 0)
		return err;

	err = __do_connect (fd, dest_addr);
	if (err < 0) {
		__mpq_del_fd (q, fd);
		return err;
	}

	return 0;
}

static int __mpq_connect_args (aosl_fd_t fd, const aosl_sockaddr_t *dest_addr, int timeo, size_t max_pkt_size,
					aosl_check_packet_t chk_pkt_f, aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, va_list args)
{
	struct mp_queue *q;
	uintptr_t l;
	uintptr_t *argv;

	if (argc > MPQ_ARGC_MAX)
		return -AOSL_E2BIG;

	q = __get_or_create_current ();
	if (q == NULL)
		return -AOSL_EINVAL;

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);

	return __this_q_connect_argv (q, fd, dest_addr, timeo, max_pkt_size, chk_pkt_f, data_f, event_f, argc, argv);
}

__export_in_so__ int aosl_mpq_connect (aosl_fd_t fd, const aosl_sockaddr_t *dest_addr,
											int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
									aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __mpq_connect_args (fd, dest_addr, timeo, max_pkt_size, chk_pkt_f, data_f, event_f, argc, args);
	va_end (args);

	return_err (err);
}

static void ____target_q_connect (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	int *err_p = (int *)argv [0];
	aosl_fd_t fd = (aosl_fd_t)argv [1];
	const aosl_sockaddr_t *dest_addr = (const aosl_sockaddr_t *)argv [2];
	int timeo = (size_t)argv [3];
	size_t max_pkt_size = (size_t)argv [4];
	aosl_check_packet_t chk_pkt_f = (aosl_check_packet_t)argv [5];
	aosl_fd_data_t data_f = (aosl_fd_data_t)argv [6];
	aosl_fd_event_t event_f = (aosl_fd_event_t)argv [7];

	UNUSED (queued_ts_p);
	UNUSED (robj);

	*err_p = __this_q_connect_argv (THIS_MPQ (), fd, dest_addr, timeo, max_pkt_size, chk_pkt_f, data_f, event_f, argc - 9, &argv [9]);
}

static int __mpq_connect_on_q_args (aosl_mpq_t qid, aosl_fd_t fd, const aosl_sockaddr_t *dest_addr,
														int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
										aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, va_list args)
{
	struct mp_queue *q;
	uintptr_t l;
	uintptr_t *argv;
	int err;

	if (argc > MPQ_ARGC_MAX)
		return -AOSL_E2BIG;

	q = __mpq_get (qid);
	if (q == NULL)
		return -AOSL_EINVAL;

	argv = aosl_alloca (sizeof (uintptr_t) * (9 + argc));
	argv [0] = (uintptr_t)&err;
	argv [1] = (uintptr_t)fd;
	argv [2] = (uintptr_t)dest_addr;
	argv [3] = (uintptr_t)timeo;
	argv [4] = (uintptr_t)max_pkt_size;
	argv [5] = (uintptr_t)chk_pkt_f;
	argv [6] = (uintptr_t)data_f;
	argv [7] = (uintptr_t)event_f;
	for (l = 0; l < argc; l++)
		argv [9 + l] = va_arg (args, uintptr_t);

	if (__mpq_call_argv (q, -1, "____target_q_connect", ____target_q_connect, 8 + argc, argv) < 0)
		err = -aosl_errno;

	__mpq_put (q);

	return err;
}

__export_in_so__ int aosl_mpq_connect_on_q (aosl_mpq_t qid, aosl_fd_t fd, const aosl_sockaddr_t *dest_addr,
																int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
														aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __mpq_connect_on_q_args (qid, fd, dest_addr, timeo, max_pkt_size, chk_pkt_f, data_f, event_f, argc, args);
	va_end (args);

	return_err (err);
}

static int __this_q_listen_argv (struct mp_queue *q, aosl_fd_t fd, int backlog, aosl_sk_accepted_t accepted_f, aosl_fd_event_t event_f, uintptr_t argc, uintptr_t *argv)
{
	size_t max_pkt_size;

	int err = aosl_hal_sk_listen (fd, backlog);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}

	max_pkt_size = sizeof (aosl_accept_data_t);
	return __mpq_add_fd_argv (q, fd, -1, max_pkt_size, 0, IOFD_SOCK_LISTEN, __default_accept, NULL, NULL, NULL, (aosl_fd_data_t)(void *)accepted_f, event_f, argc, argv);
}

static int __mpq_listen_args (aosl_fd_t fd, int backlog, aosl_sk_accepted_t accepted_f, aosl_fd_event_t event_f, uintptr_t argc, va_list args)
{
	struct mp_queue *q;
	uintptr_t l;
	uintptr_t *argv;

	if (argc > MPQ_ARGC_MAX)
		return -AOSL_E2BIG;

	q = __get_or_create_current ();
	if (q == NULL)
		return -AOSL_EINVAL;

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);

	return __this_q_listen_argv (q, fd, backlog, accepted_f, event_f, argc, argv);
}

__export_in_so__ int aosl_mpq_listen (aosl_fd_t fd, int backlog, aosl_sk_accepted_t accepted_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __mpq_listen_args (fd, backlog, accepted_f, event_f, argc, args);
	va_end (args);

	return_err (err);
}

static void ____target_q_listen (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	int *err_p = (int *)argv [0];
	aosl_fd_t fd = (aosl_fd_t)argv [1];
	int backlog = (int)argv [2];
	aosl_sk_accepted_t accepted_f = (aosl_sk_accepted_t)argv [3];
	aosl_fd_event_t event_f = (aosl_fd_event_t)argv [4];

	UNUSED (queued_ts_p);
	UNUSED (robj);

	*err_p = __this_q_listen_argv (THIS_MPQ (), fd, backlog, accepted_f, event_f, argc - 5, &argv [5]);
}

__export_in_so__ int aosl_mpq_listen_on_q (aosl_mpq_t qid, aosl_fd_t fd, int backlog,
		aosl_sk_accepted_t accepted_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t l;
	uintptr_t *argv;
	int err;

	if (argc > MPQ_ARGC_MAX)
		return_err (-AOSL_E2BIG);

	q = __mpq_get (qid);
	if (q == NULL)
		return_err (-AOSL_EINVAL);

	argv = aosl_alloca (sizeof (uintptr_t) * (5 + argc));
	argv [0] = (uintptr_t)&err;
	argv [1] = (uintptr_t)fd;
	argv [2] = (uintptr_t)backlog;
	argv [3] = (uintptr_t)accepted_f;
	argv [4] = (uintptr_t)event_f;
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [5 + l] = va_arg (args, uintptr_t);
	va_end (args);

	if (__mpq_call_argv (q, -1, "____target_q_listen", ____target_q_listen, 5 + argc, argv) < 0)
		err = -aosl_errno;

	__mpq_put (q);

	return_err (err);
}

static __inline__ int __this_q_add_dgram_sk_argv (struct mp_queue *q, aosl_fd_t fd, size_t max_pkt_size,
				aosl_dgram_sk_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, uintptr_t *argv)
{
	size_t extra_bytes = sizeof (struct recvfrom_args);
	return __mpq_add_fd_argv (q, fd, -1, max_pkt_size, extra_bytes, 0, __default_recvfrom, __default_sendto, NULL, NULL, (aosl_fd_data_t)(void *)data_f, event_f, argc, argv);
}

static int __mpq_add_dgram_sk_args (aosl_fd_t fd, size_t max_pkt_size, aosl_dgram_sk_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, va_list args)
{
	struct mp_queue *q;
	uintptr_t l;
	uintptr_t *argv;

	if (argc > MPQ_ARGC_MAX)
		return -AOSL_E2BIG;

	q = __get_or_create_current ();
	if (q == NULL)
		return -AOSL_EINVAL;

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);

	return __this_q_add_dgram_sk_argv (q, fd, max_pkt_size, data_f, event_f, argc, argv);
}

__export_in_so__ int aosl_mpq_add_dgram_socket (aosl_fd_t fd, size_t max_pkt_size, aosl_dgram_sk_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __mpq_add_dgram_sk_args (fd, max_pkt_size, data_f, event_f, argc, args);
	va_end (args);

	return_err (err);
}

static void ____target_q_add_dgram_sk (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	int *err_p = (int *)argv [0];
	aosl_fd_t fd = (aosl_fd_t)argv [1];
	size_t max_pkt_size = (size_t)argv [2];
	aosl_dgram_sk_data_t data_f = (aosl_dgram_sk_data_t)argv [3];
	aosl_fd_event_t event_f = (aosl_fd_event_t)argv [4];

	UNUSED (queued_ts_p);
	UNUSED (robj);

	*err_p = __this_q_add_dgram_sk_argv (THIS_MPQ (), fd, max_pkt_size, data_f, event_f, argc - 5, &argv [5]);
}

__export_in_so__ int aosl_mpq_add_dgram_socket_on_q (aosl_mpq_t qid, aosl_fd_t fd, size_t max_pkt_size,
							aosl_dgram_sk_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t l;
	uintptr_t *argv;
	int err;

	if (argc > MPQ_ARGC_MAX)
		return_err (-AOSL_E2BIG);

	q = __mpq_get (qid);
	if (q == NULL)
		return_err (-AOSL_EINVAL);

	argv = aosl_alloca (sizeof (uintptr_t) * (5 + argc));
	argv [0] = (uintptr_t)&err;
	argv [1] = (uintptr_t)fd;
	argv [2] = (uintptr_t)max_pkt_size;
	argv [3] = (uintptr_t)data_f;
	argv [4] = (uintptr_t)event_f;
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [5 + l] = va_arg (args, uintptr_t);
	va_end (args);

	if (__mpq_call_argv (q, -1, "____target_q_add_dgram_sk", ____target_q_add_dgram_sk, 5 + argc, argv) < 0)
		err = -aosl_errno;

	__mpq_put (q);

	return_err (err);
}

static __inline__ int __this_q_add_stream_sk_argv (struct mp_queue *q, aosl_fd_t fd,
								size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
									aosl_fd_data_t data_f, aosl_fd_event_t event_f,
													uintptr_t argc, uintptr_t *argv)
{
	return __mpq_add_fd_argv (q, fd, -1, max_pkt_size, 0, 0, __default_recv, __default_send, chk_pkt_f, NULL, data_f, event_f, argc, argv);
}

static int __mpq_add_stream_sk_args (aosl_fd_t fd, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
						aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, va_list args)
{
	struct mp_queue *q;
	uintptr_t l;
	uintptr_t *argv;

	if (argc > MPQ_ARGC_MAX)
		return -AOSL_E2BIG;

	q = __get_or_create_current ();
	if (q == NULL)
		return -AOSL_EINVAL;

	argv = aosl_alloca (sizeof (uintptr_t) * argc);
	for (l = 0; l < argc; l++)
		argv [l] = va_arg (args, uintptr_t);

	return __this_q_add_stream_sk_argv (q, fd, max_pkt_size, chk_pkt_f, data_f, event_f, argc, argv);
}

__export_in_so__ int aosl_mpq_add_stream_socket (aosl_fd_t fd, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
										aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __mpq_add_stream_sk_args (fd, max_pkt_size, chk_pkt_f, data_f, event_f, argc, args);
	va_end (args);

	return_err (err);
}

static void ____target_q_add_stream_sk (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	int *err_p = (int *)argv [0];
	aosl_fd_t fd = (aosl_fd_t)argv [1];
	size_t max_pkt_size = (size_t)argv [2];
	aosl_check_packet_t chk_pkt_f = (aosl_check_packet_t)argv [3];
	aosl_fd_data_t data_f = (aosl_fd_data_t)argv [4];
	aosl_fd_event_t event_f = (aosl_fd_event_t)argv [5];

	UNUSED (queued_ts_p);
	UNUSED (robj);

	*err_p = __this_q_add_stream_sk_argv (THIS_MPQ (), fd, max_pkt_size, chk_pkt_f, data_f, event_f, argc - 6, &argv [6]);
}

__export_in_so__ int aosl_mpq_add_stream_socket_on_q (aosl_mpq_t qid, aosl_fd_t fd,
								size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
		aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	struct mp_queue *q;
	va_list args;
	uintptr_t l;
	uintptr_t *argv;
	int err;

	if (argc > MPQ_ARGC_MAX)
		return_err (-AOSL_E2BIG);

	q = __mpq_get (qid);
	if (q == NULL)
		return_err (-AOSL_EINVAL);

	argv = aosl_alloca (sizeof (uintptr_t) * (6 + argc));
	argv [0] = (uintptr_t)&err;
	argv [1] = (uintptr_t)fd;
	argv [2] = (uintptr_t)max_pkt_size;
	argv [3] = (uintptr_t)chk_pkt_f;
	argv [4] = (uintptr_t)data_f;
	argv [5] = (uintptr_t)event_f;
	va_start (args, argc);
	for (l = 0; l < argc; l++)
		argv [6 + l] = va_arg (args, uintptr_t);
	va_end (args);

	if (__mpq_call_argv (q, -1, "____target_q_add_stream_sk", ____target_q_add_stream_sk, 6 + argc, argv) < 0)
		err = -aosl_errno;

	__mpq_put (q);

	return_err (err);
}

static isize_t ____send (struct iofd *f, const void *buf, size_t len, int flags)
{
	w_buffer_t *node;
	int *flags_p;
	isize_t err;

	if (len > FD_MAX_WBUF_SIZE)
		return -AOSL_EMSGSIZE;

	if (w_queue_space (&f->w_q) < len)
		return -AOSL_EAGAIN;

	if (f->w_q.head != NULL || (f->flags & IOFD_NOT_READY) != 0) {
		err = 0;
		goto __queue_it;
	}

	err = aosl_hal_sk_send (iofd_fobj (f)->fd, buf, len, flags);
	if (err <= 0) {
		return aosl_hal_set_error((int)err);
	}

	if ((size_t)err < len) {
__queue_it:
		node = (w_buffer_t *)aosl_malloc (sizeof (w_buffer_t) + AOSL_I_ALIGN_PTR (len - err) + sizeof (flags));
		if (node == NULL)
			return -AOSL_ENOMEM;

		memcpy (node + 1, (char *)buf + err, len - err);
		node->w_data = node + 1;
		node->w_tail = (char *)(node + 1) + (len - err);
		node->w_extra_size = sizeof (flags);

		flags_p = AOSL_P_ALIGN_PTR (node->w_tail);
		*flags_p = flags;

		w_queue_add (&f->w_q, node);
	}

	return len;
}

static void ____target_q_send (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	isize_t *err_p = (isize_t *)argv [0];
	struct iofd *f = (struct iofd *)argv [1];
	const void *buf = (const void *)argv [2];
	size_t len = (size_t)argv [3];
	int flags = (int)argv [4];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	*err_p = ____send (f, buf, len, flags);
}

__export_in_so__ isize_t aosl_send (aosl_fd_t fd, const void *buf, size_t len, int flags)
{
	struct iofd *f;
	isize_t err = -AOSL_EINVAL;

	f = iofd_get (fd);
	/**
	 * We do not support write operation on a fd if it is not added to
	 * the mpq, please use the system call API directly in these cases.
	 **/
	if (f != NULL) {
		struct mp_queue *q = __mpq_get_or_this (f->q);
		if (q != NULL) {
			uintptr_t argv [5];

			argv [0] = (uintptr_t)&err;
			argv [1] = (uintptr_t)f;
			argv [2] = (uintptr_t)buf;
			argv [3] = (uintptr_t)len;
			argv [4] = (uintptr_t)flags;
			if (__mpq_call_argv (q, -1, "____target_q_send", ____target_q_send, 5, argv) < 0)
				err = aosl_errno;
			
			__mpq_put_or_this (q);
		}

		iofd_put (f);
	}

	return_err (err);
}

static isize_t ____sendto (struct iofd *f, const void *buf, size_t len, int flags,
							const aosl_sockaddr_t *dest_addr)
{
	w_buffer_t *node;
	struct sendto_args *args;
	isize_t err;

	if (len > FD_MAX_WBUF_SIZE)
		return -AOSL_EMSGSIZE;

	if (w_queue_space (&f->w_q) < len)
		return -AOSL_EAGAIN;

	if (f->w_q.head != NULL || (f->flags & IOFD_NOT_READY) != 0) {
		err = 0;
		goto __queue_it;
	}
	err = aosl_hal_sk_sendto (iofd_fobj (f)->fd, buf, len, flags, dest_addr);
	if (err <= 0) {
		return aosl_hal_set_error(err);
	}

	if ((size_t)err < len) {
__queue_it:
		node = (w_buffer_t *)aosl_malloc (sizeof (w_buffer_t) + AOSL_I_ALIGN_PTR (len - err) + sizeof (struct sendto_args));
		if (node == NULL)
			return -AOSL_ENOMEM;

		memcpy (node + 1, (char *)buf + err, len - err);
		node->w_data = node + 1;
		node->w_tail = (char *)(node + 1) + (len - err);
		node->w_extra_size = sizeof (struct sendto_args);

		args = (struct sendto_args *)AOSL_P_ALIGN_PTR (node->w_tail);
		args->flags = flags;
		memcpy (&args->addr, dest_addr, sizeof(args->addr));
		w_queue_add (&f->w_q, node);
	}

	return len;
}

static void ____target_q_sendto (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	isize_t *err_p = (isize_t *)argv [0];
	struct iofd *f = (struct iofd *)argv [1];
	const void *buf = (const void *)argv [2];
	size_t len = (size_t)argv [3];
	int flags = (int)argv [4];
	const aosl_sockaddr_t *dest_addr = (const aosl_sockaddr_t *)argv [5];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	*err_p = ____sendto (f, buf, len, flags, dest_addr);
}

__export_in_so__ isize_t aosl_sendto (aosl_fd_t fd, const void *buf, size_t len, int flags, const aosl_sockaddr_t *dest_addr)
{
	struct iofd *f;
	isize_t err = -AOSL_EINVAL;

	f = iofd_get (fd);
	/**
	 * We do not support write operation on a fd if it is not added to
	 * the mpq, please use the system call API directly in these cases.
	 **/
	if (f != NULL) {
		struct mp_queue *q = __mpq_get_or_this (f->q);
		if (q != NULL) {
			uintptr_t argv [7];

			argv [0] = (uintptr_t)&err;
			argv [1] = (uintptr_t)f;
			argv [2] = (uintptr_t)buf;
			argv [3] = (uintptr_t)len;
			argv [4] = (uintptr_t)flags;
			argv [5] = (uintptr_t)dest_addr;
			if (__mpq_call_argv (q, -1, "____target_q_sendto", ____target_q_sendto, 6, argv) < 0)
				err = aosl_errno;
			
			__mpq_put_or_this (q);
		}

		iofd_put (f);
	}

	return_err (err);
}

__export_in_so__ int aosl_ip_sk_addr_init_with_port (aosl_sk_addr_t *sk_addr, uint16_t af, unsigned short port)
{
	switch (af) {
	case AOSL_AF_INET:
		memset (&sk_addr->in, 0, sizeof sk_addr->in);
		sk_addr->in.sin_port = aosl_htons (port);
		break;
#if defined(CONFIG_AOSL_IPV6)
	case AOSL_AF_INET6:
		memset (&sk_addr->in6, 0, sizeof sk_addr->in6);
		sk_addr->in6.sin6_port = aosl_htons (port);
		break;
#endif
	default:
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	sk_addr->sa.sa_family = af;
	return 0;
}

__export_in_so__ int aosl_bind_port_only (aosl_fd_t sk, uint16_t af, unsigned short port)
{
	aosl_sk_addr_t sk_addr;

	switch (af) {
	case AOSL_AF_INET:
		sk_addr.in.sin_addr.s_addr = 0;
		sk_addr.in.sin_port = aosl_htons (port);
		break;
#if defined(CONFIG_AOSL_IPV6)
	case AOSL_AF_INET6:
		memset (&sk_addr.in6, 0, sizeof sk_addr.in6);
		sk_addr.in6.sin6_port = aosl_htons (port);
		break;
#endif
	default:
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	sk_addr.sa.sa_family = af;
	int err = aosl_hal_sk_bind (sk, &sk_addr.sa);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return 0;
}

__export_in_so__ int aosl_bind_device (aosl_fd_t sockfd, const char *if_name)
{
	if (if_name == NULL || if_name[0] == '\0') {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}
	int err = aosl_hal_sk_bind_device (sockfd, if_name);
	if (err < 0) {
		return aosl_hal_set_error(err);
	}
	return 0;
}

__export_in_so__ void aosl_ip_addr_init (aosl_ip_addr_t *addr)
{
	addr->v4.sin_family = AOSL_AF_UNSPEC;
	addr->v6.sin6_family = AOSL_AF_UNSPEC;
}

__export_in_so__ int aosl_ip_sk_bind (const aosl_ip_sk_t *sk, const aosl_ip_addr_t *addr)
{
	int binded;
	int err;

	binded = 0;
	if (!aosl_fd_invalid (sk->v4) && addr->v4.sin_family != AOSL_AF_UNSPEC) {
		err = aosl_hal_sk_bind (sk->v4, (const aosl_sockaddr_t *)&addr->v4);
		if (err == 0) {
			binded++;
		} else {
			aosl_hal_set_error(err);
		}
	}

#ifdef CONFIG_AOSL_IPV6
	if (!aosl_fd_invalid (sk->v6) && addr->v6.sin6_family != AOSL_AF_UNSPEC) {
		err = aosl_hal_sk_bind (sk->v6, (const aosl_sockaddr_t *)&addr->v6);
		if (err == 0) {
			binded++;
		} else {
			aosl_hal_set_error(err);
		}
	}
#endif

	if (binded > 0)
		return binded;

	return -1;
}

__export_in_so__ int aosl_mpq_ip_sk_connect (const aosl_ip_sk_t *sk, const aosl_sockaddr_t *dest_addr,
										int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
									aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...)
{
	aosl_fd_t fd;
#ifdef CONFIG_AOSL_IPV6
	aosl_sk_addr_t addr;
#endif
	va_list args;
	int err;

	switch (dest_addr->sa_family) {
	case AOSL_AF_INET:
		if (!aosl_fd_invalid (sk->v4)) {
			fd = sk->v4;
			goto __connect;
		}

#ifdef CONFIG_AOSL_IPV6
		if (!aosl_fd_invalid (sk->v6)) {
			if (ipv6_sk_addr_from_ipv4 (&addr.sa, dest_addr) < 0)
				goto __err;

			fd = sk->v6;
			dest_addr = &addr.sa;
			goto __connect;
		}
#endif
		goto __err;
#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		if (!aosl_fd_invalid (sk->v6)) {
			fd = sk->v6;
			goto __connect;
		}

		if (!aosl_fd_invalid (sk->v4)) {
			if (ipv6_sk_addr_to_ipv4 (&addr.sa, dest_addr) < 0)
				goto __err;

			fd = sk->v4;
			dest_addr = &addr.sa;
			goto __connect;
		}
		goto __err;
#endif
	default:
		goto __err;
	}

__connect:
	va_start (args, argc);
	err = __mpq_connect_args (fd, dest_addr, timeo, max_pkt_size, chk_pkt_f, data_f, event_f, argc, args);
	va_end (args);

	return_err (err);

__err:
	return_err (-AOSL_EINVAL);
}

__export_in_so__ int aosl_mpq_ip_sk_connect_on_q (aosl_mpq_t qid, const aosl_ip_sk_t *sk,
						const aosl_sockaddr_t *dest_addr, int timeo, size_t max_pkt_size,
			aosl_check_packet_t chk_pkt_f, aosl_fd_data_t data_f, aosl_fd_event_t event_f,
																	uintptr_t argc, ...)
{
	aosl_fd_t fd;
#ifdef CONFIG_AOSL_IPV6
	aosl_sk_addr_t addr;
#endif
	va_list args;
	int err;

	switch (dest_addr->sa_family) {
	case AOSL_AF_INET:
		if (!aosl_fd_invalid (sk->v4)) {
			fd = sk->v4;
			goto __connect;
		}

#ifdef CONFIG_AOSL_IPV6
		if (!aosl_fd_invalid (sk->v6)) {
			if (ipv6_sk_addr_from_ipv4 (&addr.sa, dest_addr) < 0)
				goto __err;

			fd = sk->v6;
			dest_addr = &addr.sa;
			goto __connect;
		}
#endif
		goto __err;
#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		if (!aosl_fd_invalid (sk->v6)) {
			fd = sk->v6;
			goto __connect;
		}

		if (!aosl_fd_invalid (sk->v4)) {
			if (ipv6_sk_addr_to_ipv4 (&addr.sa, dest_addr) < 0)
				goto __err;

			fd = sk->v4;
			dest_addr = &addr.sa;
			goto __connect;
		}
		goto __err;
#endif
	default:
		goto __err;
	}

__connect:
	va_start (args, argc);
	err = __mpq_connect_on_q_args (qid, fd, dest_addr, timeo, max_pkt_size, chk_pkt_f, data_f, event_f, argc, args);
	va_end (args);

	return_err (err);

__err:
	return_err (-AOSL_EINVAL);
}

__export_in_so__ int aosl_ipv6_sk_addr_from_ipv4 (aosl_sockaddr_t *sk_addr_v6, const aosl_sockaddr_t *sk_addr_v4)
{
#ifdef CONFIG_AOSL_IPV6
	return ipv6_sk_addr_from_ipv4 (sk_addr_v6, sk_addr_v4);
#else
	return -1;
#endif
}

__export_in_so__ int aosl_ipv6_sk_addr_to_ipv4 (aosl_sockaddr_t *sk_addr_v4, const aosl_sockaddr_t *sk_addr_v6)
{
#ifdef CONFIG_AOSL_IPV6
	return ipv6_sk_addr_to_ipv4 (sk_addr_v4, sk_addr_v6);
#else
	return -1;
#endif
}

__export_in_so__ isize_t aosl_ip_sk_sendto (const aosl_ip_sk_t *sk, const void *buf, size_t len, int flags, const aosl_sockaddr_t *dest_addr)
{
	aosl_fd_t fd;
#ifdef CONFIG_AOSL_IPV6
	aosl_sk_addr_t addr;
#endif

	switch (dest_addr->sa_family) {
	case AOSL_AF_INET:
		if (!aosl_fd_invalid (sk->v4)) {
			fd = sk->v4;
			goto __send;
		}

#ifdef CONFIG_AOSL_IPV6
		if (!aosl_fd_invalid (sk->v6)) {
			if (ipv6_sk_addr_from_ipv4 (&addr.sa, dest_addr) < 0)
				goto __err;

			fd = sk->v6;
			dest_addr = &addr.sa;
			goto __send;
		}
#endif
		goto __err;
#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		if (!aosl_fd_invalid (sk->v6)) {
			fd = sk->v6;
			goto __send;
		}

		if (!aosl_fd_invalid (sk->v4)) {
			if (ipv6_sk_addr_to_ipv4 (&addr.sa, dest_addr) < 0)
				goto __err;

			fd = sk->v4;
			dest_addr = &addr.sa;
			goto __send;
		}
		goto __err;
#endif
	default:
		goto __err;
	}

__send:
	return aosl_sendto (fd, buf, len, flags, dest_addr);

__err:
	aosl_errno = AOSL_EINVAL;
	return -1;
}

__export_in_so__ void aosl_ip_sk_close (aosl_ip_sk_t *sk)
{
	if (!aosl_fd_invalid (sk->v4)) {
		__iofd_close (sk->v4);
		sk->v4 = AOSL_INVALID_FD;
	}

#ifdef CONFIG_AOSL_IPV6
	if (!aosl_fd_invalid (sk->v6)) {
		__iofd_close (sk->v6);
		sk->v6 = AOSL_INVALID_FD;
	}
#endif
}

__export_in_so__ const char *aosl_sockaddr_str(const aosl_sockaddr_t *addr, char *addr_buf, size_t buf_len)
{
	switch (addr->sa_family) {
	case AOSL_AF_INET:
		k_inet_ntop (AOSL_AF_INET, &addr->sin_addr, addr_buf, buf_len);
		break;

#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		k_inet_ntop (AOSL_AF_INET6, &addr->sin6_addr, addr_buf, buf_len);
		break;
#endif
	default:
		snprintf (addr_buf, buf_len, "<Unknown af %d>", addr->sa_family);
		break;
	}

	return addr_buf;
}

__export_in_so__ const char *aosl_inet_addr_str (int af, const void *addr, char *addr_buf, size_t buf_len)
{
	switch (af) {
	case AOSL_AF_INET:
		k_inet_ntop (AOSL_AF_INET, addr, addr_buf, buf_len);
		break;

#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		k_inet_ntop (AOSL_AF_INET6, addr, addr_buf, buf_len);
		break;
#endif
	default:
		snprintf (addr_buf, buf_len, "<Unknown af %d>", af);
		break;
	}

	return addr_buf;
}

__export_in_so__ const char *aosl_ip_sk_addr_str (const aosl_sk_addr_t *addr, char *addr_buf, size_t buf_len)
{
	switch (addr->sa.sa_family) {
	case AOSL_AF_INET:
		k_inet_ntop (AOSL_AF_INET, &addr->in.sin_addr, addr_buf, buf_len);
		break;

#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		k_inet_ntop (AOSL_AF_INET6, &addr->in6.sin6_addr, addr_buf, buf_len);
		break;
#endif
	default:
		snprintf (addr_buf, buf_len, "<Unknown af %d>", addr->sa.sa_family);
		break;
	}

	return addr_buf;
}

__export_in_so__ unsigned short aosl_ip_sk_addr_port (const aosl_sk_addr_t *addr)
{
	switch (addr->sa.sa_family) {
	case AOSL_AF_INET:
		return aosl_ntohs (addr->in.sin_port);
#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		return aosl_ntohs (addr->in6.sin6_port);
#endif
	default:
		break;
	}

	return 0;
}

__export_in_so__ int aosl_sk_addr_ip_equal (const aosl_sockaddr_t *addr1, const aosl_sockaddr_t *addr2)
{
	const aosl_sk_addr_t *a1 = (const aosl_sk_addr_t *)addr1;
	const aosl_sk_addr_t *a2 = (const aosl_sk_addr_t *)addr2;

	if (a1->sa.sa_family != a2->sa.sa_family)
		return 0;

	switch (a1->sa.sa_family) {
	case AOSL_AF_UNSPEC:
		return 1;
	case AOSL_AF_INET:
		return (int)(a1->in.sin_addr.s_addr == a2->in.sin_addr.s_addr);
#ifdef CONFIG_AOSL_IPV6
	case AOSL_AF_INET6:
		return !memcmp (&a1->in6.sin6_addr, &a2->in6.sin6_addr, sizeof (aosl_in6_addr_t));
#endif
	default:
		break;
	}

	return 0;
}

__export_in_so__ int aosl_inet_addr_from_string (void *addr, const char *str_addr)
{
#ifdef CONFIG_AOSL_IPV6
	if (strchr (str_addr, ':') != NULL) {
		if (k_inet_pton (AOSL_AF_INET6, str_addr, addr) != 1)
			return 0;

		return sizeof (aosl_in6_addr_t);
	}
#endif

	if (k_inet_pton (AOSL_AF_INET, str_addr, addr) != 1)
		return 0;

	return sizeof (aosl_in_addr_t);
}

__export_in_so__ int aosl_ip_sk_addr_from_string (aosl_sk_addr_t *sk_addr, const char *str_addr, uint16_t port)
{
#ifdef CONFIG_AOSL_IPV6
	if (strchr (str_addr, ':') != NULL) {
		/**
		 * This is very important for XNU kernel, if we do not do this,
		 * then sendto would return error EHOSTUNREACH.
		 **/
		memset (&sk_addr->in6, 0, sizeof (aosl_sockaddr_in6_t));

		if (k_inet_pton (AOSL_AF_INET6, str_addr, &sk_addr->in6.sin6_addr.s6_addr_v) != 1)
			return 0;

		sk_addr->in6.sin6_family = AOSL_AF_INET6;
		sk_addr->in6.sin6_port = aosl_htons (port);
		return sizeof (aosl_sockaddr_in6_t);
	}
#endif

	/**
	 * This is very important for XNU kernel, if we do not do this,
	 * then sendto would return error EHOSTUNREACH.
	 **/
	memset (&sk_addr->in, 0, sizeof (aosl_sockaddr_in_t));

	if (k_inet_pton (AOSL_AF_INET, str_addr, &sk_addr->in.sin_addr.s_addr) != 1)
		return 0;

	sk_addr->in.sin_family = AOSL_AF_INET;
	sk_addr->in.sin_port = aosl_htons (port);
	return sizeof (aosl_sockaddr_in_t);
}
