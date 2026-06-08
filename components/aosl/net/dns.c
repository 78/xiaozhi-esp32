/***************************************************************************
 * Module:	DNS resolve asynchronously helper implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api/aosl_types.h>
#include <api/aosl_alloca.h>
#include <api/aosl_time.h>
#include <api/aosl_mpqp.h>
#include <api/aosl_mpq_net.h>
#include <kernel/kernel.h>
#include <kernel/err.h>
#include <hal/aosl_hal_socket.h>

#define UNUSED(expr) (void)(expr)

#define MAX_DNS_RES_CNT 8
/**
 * Return values:
 *      <0: processed NOTHING, error encountered, should break out
 *       0: processed one, but should stop processing next resolved address
 *      >0: processed one and should continue processing more addresses
 **/
typedef int (*resolve_cbf_t) (int af, int socktype, int prot, aosl_sockaddr_t *addr, unsigned int index, intptr_t timeo, va_list args);

static size_t vhostbyname_timed_do (const char *hostname, unsigned short port, int socktype, int proto, intptr_t timeo, resolve_cbf_t fn, va_list args)
{
	int err;
	size_t count = 0;
	aosl_sockaddr_t addrs[MAX_DNS_RES_CNT] = {0};
	aosl_mpq_t qid = aosl_mpq_this();
  AOSL_LOG(AOSL_LOG_CRIT, "[DNS][q-%d] Start dns request %s:%u", (int)qid, hostname, port);

	int rescnt = aosl_hal_gethostbyname(hostname, addrs, MAX_DNS_RES_CNT);
	if (rescnt > 0) {
		for (int i = 0; i < rescnt && i < MAX_DNS_RES_CNT; i++) {
			aosl_sockaddr_t *addr = &addrs[i];
			addr->sa_port = aosl_htons(port);
			va_list tmp_args;
			va_copy (tmp_args, args);
			err = fn (addr->sa_family, socktype, proto, addr, count, timeo, tmp_args);
			va_end (tmp_args);
			if (err < 0) {
				goto ____out;
			}

			count++;

			if (err == 0)
				goto ____out;
		}
	}

____out:
	AOSL_LOG(AOSL_LOG_CRIT, "[DNS][q-%d] End dns request %s:%u cnt=%d", (int)qid, hostname, port, (int)count);
	return count;
}


static size_t hostbyname_timed_do (const char *hostname, unsigned short port, int socktype, int proto, intptr_t timeo, resolve_cbf_t fn, ...)
{
	size_t count;
	va_list args;

	va_start (args, fn);
	count = vhostbyname_timed_do (hostname, port, socktype, proto, timeo, fn, args);
	va_end (args);
	return count;
}

static int __resolved_an_addr (int af, int socktype, int prot, aosl_sockaddr_t *addr, unsigned int index, intptr_t timeo, va_list args)
{
	aosl_sk_addrinfo_t *addrs = va_arg (args, aosl_sk_addrinfo_t *);
	size_t addr_count = va_arg (args, size_t);

	UNUSED (timeo);

	if (index < (unsigned int)addr_count) {
		aosl_sk_addrinfo_t *sai = &addrs [index];
		sai->sk_af = af;
		sai->sk_type = socktype;
		sai->sk_prot = prot;
		memcpy (&sai->sk_addr, addr, sizeof(aosl_sockaddr_t));
		return 1;
	}

	aosl_errno = AOSL_ENOBUFS;
	return -1;
}

static void __queue_resolve_async_reply (aosl_mpq_t q, aosl_ref_t ref,
				aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv,
		const char *str_addr, size_t resolved_count, aosl_sk_addrinfo_t *addrs)
{
	uintptr_t *this_argv;
	uintptr_t l;

	/**
	 * the 1st arg is the resolving host name;
	 * the 2nd arg is the resolved address count;
	 * the 3rd arg is addrs memory;
	 **/
	this_argv = aosl_alloca (sizeof (uintptr_t) * (3 + argc));
	this_argv [0] = (uintptr_t)str_addr; /* the resolving addr name */
	this_argv [1] = resolved_count; /* the resolved address count */
	this_argv [2] = (uintptr_t)addrs; /* the address memory passed by requester */

	for (l = 0; l < argc; l++)
		this_argv [3 + l] = argv [l];

#ifdef CONFIG_AOSL_IPV6
	/**
	 * Update the ipv6 address prefix possibly
	 */
	for (size_t i = 0; i < resolved_count; i++) {
		if (addrs[i].sk_af == AOSL_AF_INET6) {
			aosl_mpq_set_ipv6_prefix_on_q(q, &addrs[i].sk_addr.in6.sin6_addr);
		}
	}
#endif

	/**
	 * We must consider the case that the requesting 'q' has been destroyed
	 * before we queue back the result, so free the allocated memory here
	 * when queue back the result failed!
	 * Obviously, it should be the requesting thread's responsibility to
	 * free the allocated memory when we queued back the result successfully.
	 **/
	if (aosl_mpq_queue_argv (q, AOSL_MPQ_INVALID, ref, "queue_resolve_async_reply", f, 3 + argc, this_argv) < 0) {
		aosl_ts_t now = aosl_tick_now ();
		f (&now, AOSL_FREE_ONLY_OBJ, 3 + argc, this_argv);
	}
}

static void ____dns_resolve_host (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	size_t count = 0;
	const char *hostname = (const char *)argv [0];
	unsigned short port = (unsigned short)argv [1];
	int sk_type = argv [2];
	int sk_prot = argv [3];
	aosl_sk_addrinfo_t *addrs = (aosl_sk_addrinfo_t *)argv [4];
	size_t addr_count = (size_t)argv [5];
	aosl_mpq_t q = (aosl_mpq_t)argv [6];
	aosl_mpq_func_argv_t f = (aosl_mpq_func_argv_t)argv [7];
	uintptr_t f_argc = argv [8];

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	count = hostbyname_timed_do (hostname, port, sk_type, sk_prot, 0, __resolved_an_addr, addrs, addr_count);

	__queue_resolve_async_reply (q, AOSL_REF_INVALID, f, f_argc, &argv [9], hostname, count, addrs);
}

static int __prot_resolve_host_async_args (const char *hostname, unsigned short port, int sk_type, int sk_prot,
			aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	uintptr_t *argv;
	uintptr_t l;
	aosl_mpq_t qid;

	if (argc > AOSL_VAR_ARGS_MAX) {
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	argv = aosl_alloca (sizeof (uintptr_t) * (9 + argc));
	argv [0] = (uintptr_t)hostname;
	argv [1] = port;
	argv [2] = sk_type;
	argv [3] = sk_prot;
	argv [4] = (uintptr_t)addrs;
	argv [5] = (uintptr_t)addr_count;
	argv [6] = (uintptr_t)q;
	argv [7] = (uintptr_t)f;
	argv [8] = argc;
	for (l = 0; l < argc; l++)
		argv [9 + l] = va_arg (args, uintptr_t);

	qid = aosl_mpqp_queue_argv (aosl_ltwp (), AOSL_MPQ_INVALID, AOSL_REF_INVALID, "____dns_resolve_host", ____dns_resolve_host, 9 + argc, argv);
	if (aosl_mpq_invalid (qid))
		return -1;

	return 0;
}

__export_in_so__ int aosl_tcp_resolve_host_async (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	if (argc > AOSL_VAR_ARGS_MAX) {
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	va_start (args, argc);
	err = __prot_resolve_host_async_args (hostname, port, AOSL_SOCK_STREAM, AOSL_IPPROTO_TCP, addrs, addr_count, q, f, argc, args);
	va_end (args);

	return err;
}

__export_in_so__ int aosl_tcp_resolve_host_asyncv (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	if (argc > AOSL_VAR_ARGS_MAX) {
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	return __prot_resolve_host_async_args (hostname, port, AOSL_SOCK_STREAM, AOSL_IPPROTO_TCP, addrs, addr_count, q, f, argc, args);
}

__export_in_so__ int aosl_udp_resolve_host_async (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	if (argc > AOSL_VAR_ARGS_MAX) {
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	va_start (args, argc);
	err = __prot_resolve_host_async_args (hostname, port, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP, addrs, addr_count, q, f, argc, args);
	va_end (args);

	return err;
}

__export_in_so__ int aosl_udp_resolve_host_asyncv (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args)
{
	if (argc > AOSL_VAR_ARGS_MAX) {
		aosl_errno = AOSL_EPERM;
		return -1;
	}

	return __prot_resolve_host_async_args (hostname, port, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP, addrs, addr_count, q, f, argc, args);
}
