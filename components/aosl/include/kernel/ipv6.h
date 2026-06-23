/***************************************************************************
 * Module:	IPv6 relative definitions
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __NET_IPV6_H__
#define __NET_IPV6_H__

#include <api/aosl_socket.h>
#include <kernel/byteorder/generic.h>


/*
 *	Addr type
 *	
 *	type	-	unicast | multicast
 *	scope	-	local	| site	    | global
 *	v4	-	compat
 *	v4mapped
 *	any
 *	loopback
 */

#define IPV6_ADDR_ANY		0x0000U
#define IPV6_ADDR_UNICAST      	0x0001U	
#define IPV6_ADDR_MULTICAST    	0x0002U	

#define IPV6_ADDR_LOOPBACK	0x0010U
#define IPV6_ADDR_LINKLOCAL	0x0020U
#define IPV6_ADDR_SITELOCAL	0x0040U

#define IPV6_ADDR_COMPATv4	0x0080U

#define IPV6_ADDR_SCOPE_MASK	0x00f0U

#define IPV6_ADDR_MAPPED	0x1000U

#ifndef IPV6_ADDR_MC_SCOPE
/*
 *	Addr scopes
 */
#define IPV6_ADDR_MC_SCOPE(a)	\
	((a)->s6_addr_v[1] & 0x0f)	/* nonstandard */
#endif

#define __IPV6_ADDR_SCOPE_INVALID	-1
#define IPV6_ADDR_SCOPE_NODELOCAL	0x01
#define IPV6_ADDR_SCOPE_LINKLOCAL	0x02
#define IPV6_ADDR_SCOPE_SITELOCAL	0x05
#define IPV6_ADDR_SCOPE_ORGLOCAL	0x08
#define IPV6_ADDR_SCOPE_GLOBAL		0x0e

extern int __ipv6_addr_type (const aosl_in6_addr_t *addr);

static inline int ipv6_addr_type (const aosl_in6_addr_t *addr)
{
	return __ipv6_addr_type (addr) & 0xffff;
}

static inline int ipv6_addr_scope (const aosl_in6_addr_t *addr)
{
	return __ipv6_addr_type (addr) & IPV6_ADDR_SCOPE_MASK;
}

static inline int __ipv6_addr_src_scope (int type)
{
	return (type == IPV6_ADDR_ANY) ? __IPV6_ADDR_SCOPE_INVALID : (type >> 16);
}

static inline int ipv6_addr_src_scope(const aosl_in6_addr_t *addr)
{
	return __ipv6_addr_src_scope (__ipv6_addr_type (addr));
}

static inline int __ipv6_addr_needs_scope_id (int type)
{
	return type & IPV6_ADDR_LINKLOCAL || (type & IPV6_ADDR_MULTICAST && (type & (IPV6_ADDR_LOOPBACK | IPV6_ADDR_LINKLOCAL)));
}

static inline uint32_t ipv6_iface_scope_id (const aosl_in6_addr_t *addr, int iface)
{
	return __ipv6_addr_needs_scope_id (__ipv6_addr_type (addr)) ? iface : 0;
}

static inline int ipv6_addr_cmp (const aosl_in6_addr_t *a1, const aosl_in6_addr_t *a2)
{
	return memcmp (a1, a2, sizeof (aosl_in6_addr_t));
}

static inline int ipv6_masked_addr_cmp (const aosl_in6_addr_t *a1, const aosl_in6_addr_t *m, const aosl_in6_addr_t *a2)
{
	return !!(((a1->s6_addr32_v [0] ^ a2->s6_addr32_v [0]) & m->s6_addr32_v [0]) |
		  ((a1->s6_addr32_v [1] ^ a2->s6_addr32_v [1]) & m->s6_addr32_v [1]) |
		  ((a1->s6_addr32_v [2] ^ a2->s6_addr32_v [2]) & m->s6_addr32_v [2]) |
		  ((a1->s6_addr32_v [3] ^ a2->s6_addr32_v [3]) & m->s6_addr32_v [3]));
}

static inline void ipv6_addr_prefix (aosl_in6_addr_t *pfx, const aosl_in6_addr_t *addr, int plen)
{
	/* caller must guarantee 0 <= plen <= 128 */
	int o = plen >> 3;
	int b = plen & 0x7;

	memset (pfx->s6_addr_v, 0, sizeof (pfx->s6_addr_v));
	memcpy (pfx->s6_addr_v, addr, o);
	if (b != 0)
		pfx->s6_addr_v [o] = addr->s6_addr_v [o] & (0xff00 >> b);
}

static inline void ipv6_addr_prefix_copy (aosl_in6_addr_t *addr, const aosl_in6_addr_t *pfx, int plen)
{
	/* caller must guarantee 0 <= plen <= 128 */
	int o = plen >> 3;
	int b = plen & 0x7;

	memcpy (addr->s6_addr_v, pfx, o);
	if (b != 0) {
		addr->s6_addr_v [o] &= ~(0xff00 >> b);
		addr->s6_addr_v [o] |= (pfx->s6_addr_v [o] & (0xff00 >> b));
	}
}

static inline void __ipv6_addr_set_half (aosl_be32 *addr, aosl_be32 wh, aosl_be32 wl)
{
	addr[0] = wh;
	addr[1] = wl;
}

static inline void ipv6_addr_set (aosl_in6_addr_t *addr, aosl_be32 w1, aosl_be32 w2, aosl_be32 w3, aosl_be32 w4)
{
	__ipv6_addr_set_half (&addr->s6_addr32_v [0], w1, w2);
	__ipv6_addr_set_half (&addr->s6_addr32_v [2], w3, w4);
}

static inline int ipv6_addr_equal (const aosl_in6_addr_t *a1, const aosl_in6_addr_t *a2)
{
	return ((a1->s6_addr32_v[0] ^ a2->s6_addr32_v[0]) |
		(a1->s6_addr32_v[1] ^ a2->s6_addr32_v[1]) |
		(a1->s6_addr32_v[2] ^ a2->s6_addr32_v[2]) |
		(a1->s6_addr32_v[3] ^ a2->s6_addr32_v[3])) == 0;
}

static inline int ipv6_prefix_equal (const aosl_in6_addr_t *addr1, const aosl_in6_addr_t *addr2, unsigned int prefixlen)
{
	const aosl_be32 *a1 = addr1->s6_addr32_v;
	const aosl_be32 *a2 = addr2->s6_addr32_v;
	unsigned int pdw, pbi;

	/* check complete u32 in prefix */
	pdw = prefixlen >> 5;
	if (pdw && memcmp (a1, a2, pdw << 2))
		return 0;

	/* check incomplete u32 in prefix */
	pbi = prefixlen & 0x1f;
	if (pbi && ((a1 [pdw] ^ a2 [pdw]) & aosl__htonl ((0xffffffff) << (32 - pbi))))
		return 0;

	return 1;
}

static inline int ipv6_addr_any (const aosl_in6_addr_t *a)
{
	return (*(aosl_be32 *)&a->s6_addr32_v [0] | a->s6_addr32_v [1] | a->s6_addr32_v [2] | a->s6_addr32_v [3]) == 0;
}

static inline int ipv6_addr_loopback (const aosl_in6_addr_t *a)
{
	/* ::1 */
	return (a->s6_addr32_v [0] | a->s6_addr32_v [1] | a->s6_addr32_v [2] | (a->s6_addr32_v [3] ^ aosl_cpu_to_be32 (1))) == 0;
}

static inline int ipv6_addr_v4mapped (const aosl_in6_addr_t *a)
{
	return (a->s6_addr32_v [0] | a->s6_addr32_v [1] | (a->s6_addr32_v [2] ^ aosl_cpu_to_be32 (0x0000ffff))) == 0u;
}

static inline int ipv6_addr_nat64 (const aosl_in6_addr_t *a)
{
	return ((a->s6_addr32_v [0] ^ aosl_cpu_to_be32 (0x0064ff9b)) | a->s6_addr32_v [1] | a->s6_addr32_v [2]) == 0u;
}

/*
 * Check for a RFC 4843 ORCHID address
 * (Overlay Routable Cryptographic Hash Identifiers)
 */
static inline int ipv6_addr_orchid (const aosl_in6_addr_t *a)
{
	return (a->s6_addr32_v [0] & aosl__htonl (0xfffffff0)) == aosl__htonl (0x20010010);
}

static inline int ipv6_addr_is_multicast (const aosl_in6_addr_t *addr)
{
	return (addr->s6_addr32_v [0] & aosl__htonl (0xFF000000)) == aosl__htonl (0xFF000000);
}

static inline void ipv6_addr_set_v4mapped (const aosl_be32 addr, aosl_in6_addr_t *v4mapped)
{
	ipv6_addr_set (v4mapped, 0, 0, aosl__htonl (0x0000FFFF), addr);
}

extern int ipv6_sk_addr_from_ipv4 (aosl_sockaddr_t *sk_addr_v6, const aosl_sockaddr_t *sk_addr_v4);
extern int ipv6_sk_addr_to_ipv4 (aosl_sockaddr_t *sk_addr_v4, const aosl_sockaddr_t *sk_addr_v6);


#endif /* __NET_IPV6_H__ */