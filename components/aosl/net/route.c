/***************************************************************************
 * Module:	OS independent route relatives implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>

#include <kernel/kernel.h>
#include <kernel/err.h>
#include <kernel/net.h>
#include <kernel/netifs.h>
#include <kernel/thread.h>
#include <api/aosl_mpq.h>
#include <api/aosl_route.h>
#include <kernel/rt_monitor.h>

static aosl_mpq_t netev_subscriber_q = AOSL_MPQ_INVALID;
static aosl_net_ev_func_t netev_f = NULL;
static void *netev_f_arg = NULL;
static k_rwlock_t netev_subscriber_lock;

static aosl_def_rt_t def_rts [2];
static aosl_def_rt_t last_valid_def_rt;

static int __curr = 0;

static aosl_def_rt_t *get_curr_def_rt (void)
{
	return &def_rts [__curr];
}

static aosl_def_rt_t *get_new_def_rt (void)
{
	return &def_rts [!__curr];
}

static void switch_new_to_curr (void)
{
	__curr = !__curr;
}

static int __rt_valid (const aosl_rt_t *rt)
{
	if (rt->netif.if_index < 0)
		return 0;

	switch (rt->gw.sa.sa_family) {
	case AOSL_AF_INET:
#if defined(CONFIG_AOSL_IPV6)
		/* fall throught */
	case AOSL_AF_INET6:
#endif
		return 1;
	default:
		break;
	}

	return 0;
}

static int __def_rt_valid (const aosl_def_rt_t *def_rt)
{
	return __rt_valid (&def_rt->IPv4) || __rt_valid (&def_rt->IPv6);
}

static int __same_rt (const aosl_rt_t *rt1, const aosl_rt_t *rt2)
{
	if (rt1->netif.if_index < 0 && rt2->netif.if_index < 0)
		return 1;

	if (rt1->netif.if_index != rt2->netif.if_index)
		return 0;

	if (rt1->netif.if_type != rt2->netif.if_type)
		return 0;

	return aosl_sk_addr_ip_equal (&rt1->gw.sa, &rt2->gw.sa);
}

static int __same_def_rt (const aosl_def_rt_t *def_rt1, const aosl_def_rt_t *def_rt2)
{
	if (!__same_rt (&def_rt1->IPv4, &def_rt2->IPv4))
		return 0;

#ifdef CONFIG_AOSL_IPV6
	if (!__same_rt (&def_rt1->IPv6, &def_rt2->IPv6)) {
		return 0;
	}
#endif

	return 1;
}

static int __same_def_rt_cnt (const aosl_def_rt_t *def_rt1, const aosl_def_rt_t *def_rt2)
{
	return def_rt1->IPv4.def_rt_cnt == def_rt2->IPv4.def_rt_cnt &&
	       def_rt1->IPv6.def_rt_cnt == def_rt2->IPv6.def_rt_cnt;
}

static void __invalidate_rt (aosl_rt_t *rt)
{
	rt->netif.if_index = -1; /* invalidate the if_index to indicate none */
	rt->gw.sa.sa_family = AOSL_AF_UNSPEC; /* invalidate the sa_family to indicate none */
}

static void __invalidate_def_rt (aosl_def_rt_t *def_rt)
{
	__invalidate_rt (&def_rt->IPv4);
	__invalidate_rt (&def_rt->IPv6);
}

void check_report_def_rt_change_event (aosl_net_ev_func_t f, void *arg)
{
	aosl_def_rt_t *curr_def_rt;
	aosl_def_rt_t *new_def_rt;
	int curr_rt_valid;
	aosl_net_ev_func_t call_fn = NULL;
	aosl_net_ev_t ev = AOSL_NET_EV_NONE;
	aosl_def_rt_t def_rt1;
	aosl_def_rt_t def_rt2;

	__invalidate_def_rt (&def_rt1);
	__invalidate_def_rt (&def_rt2);

	k_rwlock_wrlock (&netev_subscriber_lock);
	curr_def_rt = get_curr_def_rt ();
	new_def_rt = get_new_def_rt ();
	curr_rt_valid = __def_rt_valid (curr_def_rt);
	if (os_get_def_rt (new_def_rt) > 0) {
		if (f != NULL) {
			if (curr_rt_valid) {
				if (!__same_def_rt (new_def_rt, curr_def_rt)) {
					call_fn = f;
					ev = AOSL_NET_EV_NET_SWITCH;
					memcpy (&def_rt1, curr_def_rt, sizeof def_rt1);
					memcpy (&def_rt2, new_def_rt, sizeof def_rt2);
				} else if (!__same_def_rt_cnt(new_def_rt, curr_def_rt)) {
					call_fn = f;
					ev = AOSL_NET_EV_RT_CNT_CHANGED;
					memcpy (&def_rt1, curr_def_rt, sizeof def_rt1);
					memcpy (&def_rt2, new_def_rt, sizeof def_rt2);
				}
			} else {
				if (__def_rt_valid (&last_valid_def_rt) && !__same_def_rt (new_def_rt, &last_valid_def_rt)) {
					call_fn = f;
					ev = AOSL_NET_EV_NET_UP_CHANGED;
					memcpy (&def_rt1, &last_valid_def_rt, sizeof def_rt1);
					memcpy (&def_rt2, new_def_rt, sizeof def_rt2);
				} else {
					call_fn = f;
					ev = AOSL_NET_EV_NET_UP;
					memcpy (&def_rt1, new_def_rt, sizeof def_rt1);
				}
			}
		}

		memcpy (&last_valid_def_rt, new_def_rt, sizeof last_valid_def_rt);
	} else {
		if (f != NULL) {
			if (curr_rt_valid) {
				call_fn = f;
				ev = AOSL_NET_EV_NET_DOWN;
			}
		}
	}
	switch_new_to_curr ();
	k_rwlock_wrunlock (&netev_subscriber_lock);

	if (call_fn != NULL)
		call_fn (ev, arg, &def_rt1, &def_rt2);
}

__export_in_so__ void aosl_init_def_rt (aosl_def_rt_t *def_rt)
{
	def_rt->IPv4.netif.if_index = -1;
	def_rt->IPv4.netif.if_type = -1;
	def_rt->IPv4.netif.if_name [0] = '\0';
	def_rt->IPv4.if_cellnet = 0;
	def_rt->IPv4.if_wireless = 0;
	def_rt->IPv4.def_rt_cnt = 0;
	memset (&def_rt->IPv4.gw, 0, sizeof def_rt->IPv4.gw);
	def_rt->IPv4.gw.sa.sa_family = AOSL_AF_UNSPEC;

	def_rt->IPv6.netif.if_index = -1;
	def_rt->IPv6.netif.if_type = -1;
	def_rt->IPv6.netif.if_name [0] = '\0';
	def_rt->IPv6.if_cellnet = 0;
	def_rt->IPv6.if_wireless = 0;
	def_rt->IPv6.def_rt_cnt = 0;
	memset (&def_rt->IPv6.gw, 0, sizeof def_rt->IPv6.gw);
	def_rt->IPv6.gw.sa.sa_family = AOSL_AF_UNSPEC;
}

__export_in_so__ int aosl_get_default_rt (aosl_def_rt_t *def_rt)
{
	return os_get_def_rt (def_rt);
}

__export_in_so__ void aosl_invalidate_rt (aosl_rt_t *rt)
{
	__invalidate_rt (rt);
}

__export_in_so__ void aosl_invalidate_def_rt (aosl_def_rt_t *def_rt)
{
	__invalidate_def_rt (def_rt);
}

__export_in_so__ int aosl_rt_valid (const aosl_rt_t *rt)
{
	return __rt_valid (rt);
}

__export_in_so__ int aosl_def_rt_valid (const aosl_def_rt_t *def_rt)
{
	return __def_rt_valid (def_rt);
}

__export_in_so__ int aosl_network_is_down ()
{
	const aosl_def_rt_t *def_rt;
	int invalid;

	k_rwlock_rdlock (&netev_subscriber_lock);
	def_rt = get_curr_def_rt ();
	invalid = !__def_rt_valid (def_rt);
	k_rwlock_rdunlock (&netev_subscriber_lock);

	return invalid;
}

__export_in_so__ int aosl_same_rt (const aosl_rt_t *rt1, const aosl_rt_t *rt2)
{
	return __same_rt (rt1, rt2);
}

__export_in_so__ int aosl_same_def_rt (const aosl_def_rt_t *def_rt1, const aosl_def_rt_t *def_rt2)
{
	return __same_def_rt (def_rt1, def_rt2);
}

__export_in_so__ const char *aosl_rt_str (const aosl_rt_t *rt, char buf [], size_t buf_size)
{
	if (__rt_valid (rt)) {
		const char *IPvX = (rt->gw.sa.sa_family == AOSL_AF_INET) ? "IPv4" : "IPv6";
		char gw_str [64];

		aosl_ip_sk_addr_str (&rt->gw, gw_str, sizeof gw_str);
		snprintf (buf, buf_size, "%s: [(%d,%s,%d,cellnet:%d)->%s]", IPvX, rt->netif.if_index, rt->netif.if_name, rt->netif.if_type, rt->if_cellnet, gw_str);
		return buf;
	}

	return "<EMPTY RT>";
}

__export_in_so__ const char *aosl_def_rt_str (const aosl_def_rt_t *def_rt, char buf [], size_t buf_size)
{
	const aosl_rt_t *rt;
	char gw_str [64];
	int len;

	buf [0] = '\0';
	len = 0;

	rt = &def_rt->IPv4;
	if (__rt_valid (rt)) {
		aosl_ip_sk_addr_str (&rt->gw, gw_str, sizeof gw_str);
		snprintf (buf, buf_size, "IPv4: [(%d,%s,%d,cellnet:%d)->%s]", rt->netif.if_index, rt->netif.if_name, rt->netif.if_type, rt->if_cellnet, gw_str);
		len += strlen (buf);
	}

	rt = &def_rt->IPv6;
	if (__rt_valid (rt)) {
		if (len > 0) {
			strcat (buf, "; ");
			len += 2;
		}

		aosl_ip_sk_addr_str (&rt->gw, gw_str, sizeof gw_str);
		snprintf (&buf [len], buf_size - len, "IPv6: [(%d,%s,%d,cellnet:%d)->%s]", rt->netif.if_index, rt->netif.if_name, rt->netif.if_type, rt->if_cellnet, gw_str);
		len += strlen (buf);
	}

	if (len > 0)
		return buf;

	return "<EMPTY DEF_RT>";
}

__export_in_so__ int aosl_is_mobile_net (uint16_t af)
{
	aosl_def_rt_t *def_rt;
	aosl_rt_t *rt;

	k_rwlock_rdlock (&netev_subscriber_lock);
	def_rt = get_curr_def_rt ();
	switch (af) {
	case AOSL_AF_INET:
		rt = &def_rt->IPv4;
		break;
#if defined(CONFIG_AOSL_IPV6)
	case AOSL_AF_INET6:
		rt = &def_rt->IPv6;
		break;
#endif
	default:
		goto __einval;
	}

	if (rt->gw.sa.sa_family != AOSL_AF_UNSPEC) {
		int is_mobile_net = rt->if_cellnet;
		k_rwlock_rdunlock (&netev_subscriber_lock);
		return is_mobile_net;
	}

__einval:
	k_rwlock_rdunlock (&netev_subscriber_lock);
	aosl_errno = AOSL_EINVAL;
	return -1;
}

__export_in_so__ int aosl_ip_sk_create (aosl_ip_sk_t *sk, int type, int protocol)
{
	aosl_def_rt_t *def_rt;
	int v4_def_rt_valid;
#if defined(CONFIG_AOSL_IPV6)
	int v6_def_rt_valid;
#endif
	aosl_fd_t fd;
	int created;

	k_rwlock_rdlock (&netev_subscriber_lock);
	def_rt = get_curr_def_rt ();
	v4_def_rt_valid = __rt_valid (&def_rt->IPv4);
#if defined(CONFIG_AOSL_IPV6)
	v6_def_rt_valid = __rt_valid (&def_rt->IPv6);
#endif
	k_rwlock_rdunlock (&netev_subscriber_lock);

	sk->v4 = AOSL_INVALID_FD;
	sk->v6 = AOSL_INVALID_FD;
	created = 0;

	if (v4_def_rt_valid) {
		fd = aosl_socket (AOSL_AF_INET, type, protocol);
		if (!aosl_fd_invalid (fd)) {
			sk->v4 = fd;
			created++;
		}
	}

#if defined(CONFIG_AOSL_IPV6)
	if (v6_def_rt_valid) {
		fd = aosl_socket (AOSL_AF_INET6, type, protocol);
		if (!aosl_fd_invalid (fd)) {
			sk->v6 = fd;
			created++;
		}
	}
#endif

	if (created > 0)
		return created;

	return -1;
}

void route_clear (void)
{
	aosl_init_def_rt (&def_rts [0]);
	aosl_init_def_rt (&def_rts [1]);
	aosl_init_def_rt (&last_valid_def_rt);
}

__export_in_so__ int aosl_subscribe_net_events (aosl_net_ev_func_t f, void *arg)
{
	int err;
	aosl_mpq_t this_q;

	this_q = aosl_mpq_this ();
	if (aosl_mpq_invalid (this_q)) {
		aosl_errno = AOSL_EINVAL;
		return -1;
	}

	k_rwlock_wrlock (&netev_subscriber_lock);

	if (f != NULL) {
		if (!aosl_mpq_invalid (netev_subscriber_q)) {
			aosl_errno = AOSL_EEXIST;
			err = -1;
			goto __unlock_out;
		}

		err = os_subscribe_net_events (f, arg);
		if (err < 0)
			goto __unlock_out;

		netev_subscriber_q = this_q;
		netev_f = f;
		netev_f_arg = arg;
		k_rwlock_wrunlock (&netev_subscriber_lock);

		/**
		 * Do a network checking before we start monitoring the network
		 * events, this is just for possible lost network events between
		 * the gap of after initialized the AOSL and before subscribing
		 * the network events.
		 **/
		check_report_def_rt_change_event (f, arg);
		return err;
	}

	if (netev_subscriber_q != this_q) {
		aosl_errno = AOSL_EPERM;
		err = -1;
		goto __unlock_out;
	}

	os_unsubscribe_net_events ();
	route_clear ();
	netifs_hash_fini ();
	netev_subscriber_q = AOSL_MPQ_INVALID;
	netev_f = NULL;
	netev_f_arg = NULL;
	err = 0;

__unlock_out:
	k_rwlock_wrunlock (&netev_subscriber_lock);
	return err;
}

void k_route_init (void)
{
	k_rwlock_init (&netev_subscriber_lock);
	route_clear ();
	netifs_hash_init ();
}

void k_route_fini (void)
{
	k_rwlock_destroy (&netev_subscriber_lock);
	route_clear ();
	netifs_hash_init ();
}
