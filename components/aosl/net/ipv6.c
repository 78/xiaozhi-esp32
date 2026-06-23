/***************************************************************************
 * Module:	IPv6 relative implementations
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <string.h>
#include <kernel/err.h>
#include <kernel/types.h>
#include <kernel/byteorder/generic.h>

#include <kernel/kernel.h>
#include <kernel/mp_queue.h>
#include <kernel/ipv6.h>
#include <api/aosl_mm.h>
#include <api/aosl_mpq_net.h>

#define IPV6_ADDR_SCOPE_TYPE(scope)	((scope) << 16)

#define UNUSED(expr) (void)(expr)

static inline unsigned int ipv6_addr_scope2type (unsigned int scope)
{
	switch (scope) {
	case IPV6_ADDR_SCOPE_NODELOCAL:
		return (IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_NODELOCAL) | IPV6_ADDR_LOOPBACK);
	case IPV6_ADDR_SCOPE_LINKLOCAL:
		return (IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_LINKLOCAL) | IPV6_ADDR_LINKLOCAL);
	case IPV6_ADDR_SCOPE_SITELOCAL:
		return (IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_SITELOCAL) | IPV6_ADDR_SITELOCAL);
	}
	return IPV6_ADDR_SCOPE_TYPE (scope);
}

int __ipv6_addr_type (const aosl_in6_addr_t *addr)
{
	aosl_be32 st;
	st = addr->s6_addr32_v [0];

	/* Consider all addresses with the first three bits different of 000 and 111 as unicasts. */
	if ((st & aosl__htonl (0xE0000000)) != aosl__htonl (0x00000000) && (st & aosl__htonl (0xE0000000)) != aosl__htonl (0xE0000000))
		return (IPV6_ADDR_UNICAST | IPV6_ADDR_SCOPE_TYPE (IPV6_ADDR_SCOPE_GLOBAL));

	if ((st & aosl__htonl (0xFF000000)) == aosl__htonl (0xFF000000)) {
		/* multicast */
		/* addr-select 3.1 */
		return (IPV6_ADDR_MULTICAST | ipv6_addr_scope2type (IPV6_ADDR_MC_SCOPE (addr)));
	}

	if ((st & aosl__htonl (0xFFC00000)) == aosl__htonl (0xFE800000))
		return (IPV6_ADDR_LINKLOCAL | IPV6_ADDR_UNICAST | IPV6_ADDR_SCOPE_TYPE (IPV6_ADDR_SCOPE_LINKLOCAL)); /* addr-select 3.1 */

	if ((st & aosl__htonl (0xFFC00000)) == aosl__htonl (0xFEC00000))
		return (IPV6_ADDR_SITELOCAL | IPV6_ADDR_UNICAST | IPV6_ADDR_SCOPE_TYPE (IPV6_ADDR_SCOPE_SITELOCAL)); /* addr-select 3.1 */

	if ((st & aosl__htonl (0xFE000000)) == aosl__htonl (0xFC000000))
		return (IPV6_ADDR_UNICAST | IPV6_ADDR_SCOPE_TYPE (IPV6_ADDR_SCOPE_GLOBAL)); /* RFC 4193 */

	if ((addr->s6_addr32_v [0] | addr->s6_addr32_v [1]) == 0) {
		if (addr->s6_addr32_v [2] == 0) {
			if (addr->s6_addr32_v [3] == 0)
				return IPV6_ADDR_ANY;

			if (addr->s6_addr32_v [3] == aosl__htonl (0x00000001))
				return (IPV6_ADDR_LOOPBACK | IPV6_ADDR_UNICAST | IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_LINKLOCAL)); /* addr-select 3.4 */

			return (IPV6_ADDR_COMPATv4 | IPV6_ADDR_UNICAST | IPV6_ADDR_SCOPE_TYPE (IPV6_ADDR_SCOPE_GLOBAL)); /* addr-select 3.3 */
		}

		if (addr->s6_addr32_v [2] == aosl__htonl (0x0000ffff))
			return (IPV6_ADDR_MAPPED | IPV6_ADDR_SCOPE_TYPE (IPV6_ADDR_SCOPE_GLOBAL));	/* addr-select 3.3 */
	}

	return (IPV6_ADDR_UNICAST | IPV6_ADDR_SCOPE_TYPE (IPV6_ADDR_SCOPE_GLOBAL));	/* addr-select 3.4 */
}

__export_in_so__ int aosl_ipv6_addr_v4_mapped (const aosl_in6_addr_t *a6)
{
	return ipv6_addr_v4mapped (a6);
}

__export_in_so__ int aosl_ipv6_addr_nat64 (const aosl_in6_addr_t *a6)
{
	return ipv6_addr_nat64 (a6);
}

static __inline__ void __set_nat64_prefix (aosl_in6_addr_t *a6)
{
	uint8_t *sin6_addr = (uint8_t *)a6;
	/* uint8_t prefix_nat [] = { 0, 0x64, 0xff, 0x9b, 0, 0, 0, 0, 0, 0, 0, 0 }; */
	memset (a6, 0, sizeof (aosl_in6_addr_t));
	sin6_addr [1] = 0x64;
	sin6_addr [2] = 0xff;
	sin6_addr [3] = 0x9b;
}

int ipv6_sk_addr_from_ipv4 (aosl_sockaddr_t *sk_addr_v6, const aosl_sockaddr_t *sk_addr_v4)
{
	aosl_sockaddr_in6_t *v6_addr = (aosl_sockaddr_in6_t *)sk_addr_v6;
	const aosl_sockaddr_in_t *v4_addr = (aosl_sockaddr_in_t *)sk_addr_v4;
	const aosl_in6_addr_t *v6_prefix = aosl_mpq_get_ipv6_prefix ();
	memset (v6_addr, 0, sizeof (aosl_sockaddr_in6_t));
	v6_addr->sin6_family = AOSL_AF_INET6;
	v6_addr->sin6_port = v4_addr->sin_port;

	if (v6_prefix != NULL) {
		memcpy (&v6_addr->sin6_addr, v6_prefix, 12);
	} else {
		__set_nat64_prefix (&v6_addr->sin6_addr);
	}

	v6_addr->sin6_addr.s6_addr32_v [3] = v4_addr->sin_addr.s_addr;
	return 0;
}

static __inline__ int __ipv6_addr_v4_compatible (const aosl_in6_addr_t *a6)
{
	const aosl_in6_addr_t *v6_prefix = aosl_mpq_get_ipv6_prefix ();
	if (v6_prefix != NULL && ipv6_prefix_equal (a6, v6_prefix, 96))
		return 1;

	return (ipv6_addr_v4mapped (a6) || ipv6_addr_nat64 (a6));
}

static int __sk_addr_ipv4_compatible (const aosl_sockaddr_t *skaddr)
{
	if (skaddr->sa_family == AOSL_AF_INET)
		return 1;

	if (skaddr->sa_family == AOSL_AF_INET6)
		return __ipv6_addr_v4_compatible (&((aosl_sockaddr_in6_t *)skaddr)->sin6_addr);

	return 0;
}

__export_in_so__ int aosl_ipv6_addr_v4_compatible (const aosl_in6_addr_t *a6)
{
	return __ipv6_addr_v4_compatible (a6);
}

int ipv6_sk_addr_to_ipv4 (aosl_sockaddr_t *sk_addr_v4, const aosl_sockaddr_t *sk_addr_v6)
{
	aosl_sockaddr_in_t *v4_addr = (aosl_sockaddr_in_t *)sk_addr_v4;
	const aosl_sockaddr_in6_t *v6_addr = (aosl_sockaddr_in6_t *)sk_addr_v6;

	if (!__sk_addr_ipv4_compatible (sk_addr_v6))
		return -1;

	memset (v4_addr, 0, sizeof (aosl_sockaddr_in_t));
	v4_addr->sin_family = AOSL_AF_INET;
	v4_addr->sin_port = v6_addr->sin6_port;
	v4_addr->sin_addr.s_addr = v6_addr->sin6_addr.s6_addr32_v [3];
	return 0;
}

static void ____q_update_ipv6_prefix (struct mp_queue *q, const aosl_in6_addr_t *a6)
{
	if (a6 != NULL) {
		/**
		 * We only save the IPv6 prefix for neither v4mapped nor nat64 cases,
		 * because we can handle these two special cases without the saved
		 * prefix.
		 **/
		if (!(ipv6_addr_v4mapped (a6) || ipv6_addr_nat64 (a6))) {
			if (q->ipv6_prefix_96 == NULL)
				q->ipv6_prefix_96 = aosl_malloc (12);

			if (q->ipv6_prefix_96 != NULL)
				memcpy (q->ipv6_prefix_96, a6, 12);
		}
	} else {
		if (q->ipv6_prefix_96 != NULL) {
			aosl_free (q->ipv6_prefix_96);
			q->ipv6_prefix_96 = NULL;
		}
	}
}

static void ____target_q_set_ipv6_prefix (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv [])
{
	const aosl_in6_addr_t *a6 = (const aosl_in6_addr_t *)argv [0];
	struct mp_queue *q = THIS_MPQ ();

	UNUSED (queued_ts_p);
	UNUSED (robj);
	UNUSED (argc);

	____q_update_ipv6_prefix (q, a6);
}

__export_in_so__ int aosl_mpq_set_ipv6_prefix_on_q (aosl_mpq_t qid, const aosl_in6_addr_t *a6)
{
	struct mp_queue *q;

	q = __mpq_get (qid);
	if (q == NULL) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	if (q != THIS_MPQ ()) {
		uintptr_t arg = (uintptr_t)a6;
		int err = __mpq_call_argv (q, -1, NULL, ____target_q_set_ipv6_prefix, 1, &arg);
		__mpq_put (q);
		return err;
	}

	____q_update_ipv6_prefix (q, a6);
	__mpq_put (q);
	return 0;
}

__export_in_so__ const aosl_in6_addr_t *aosl_mpq_get_ipv6_prefix (void)
{
	struct mp_queue *q;

	q = THIS_MPQ ();
	if (q != NULL)
		return q->ipv6_prefix_96;

	return NULL;
}