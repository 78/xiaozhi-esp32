/***************************************************************************
 * Module:	Route info monitor for linux implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#if defined(__linux__)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
//#include <net/if.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>

#include <api/aosl_types.h>
#include <api/aosl_mpq_net.h>
#include <api/aosl_route.h>
#include <api/aosl_mpq_fd.h>

#include <kernel/rt_monitor.h>
#include <kernel/kernel.h>
#include <kernel/err.h>
#include <kernel/netifs.h>


static int af_netlink_fd = -1;

static void parse_rtattrs (struct rtattr *rta, int len, struct rtattr **rtas, int rta_count)
{
	int i;

	for (i = 0; i < rta_count; i++)
		rtas [i] = NULL;

	while (RTA_OK (rta, len)) {
		unsigned short type = rta->rta_type;
		if (type < rta_count && rtas [type] == NULL)
			rtas [type] = rta;

		rta = RTA_NEXT (rta,len);
	}
}

static __inline__ int __socket_nl_rt ()
{
	int sk;
	int err;
	struct sockaddr_nl l_addr;

	sk = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sk < 0)
		return sk;

	memset (&l_addr, 0, sizeof l_addr);
	l_addr.nl_family = AF_NETLINK;
	l_addr.nl_groups = 0;
	l_addr.nl_pid = 0;
	err = bind (sk, (struct sockaddr *)&l_addr, sizeof l_addr);
	if (err < 0) {
		close (sk);
		return err;
	}

	return sk;
}

static uint32_t __nlmsg_seq = 0;

static int update_ifinfos (struct nlmsghdr *h)
{
	const char *ifname;
	struct ifinfomsg *ifi = NLMSG_DATA (h);
	struct rtattr *rtas [__IFLA_MAX];
	struct rtattr *rta;

	if (h->nlmsg_type != RTM_NEWLINK && h->nlmsg_type != RTM_DELLINK)
		return 0;

	if (h->nlmsg_len < NLMSG_LENGTH (sizeof (struct ifinfomsg)))
		return -1;

	if (h->nlmsg_type == RTM_DELLINK)
		return update_netifs (1, ifi->ifi_index);

	parse_rtattrs (IFLA_RTA (ifi), IFLA_PAYLOAD (h), rtas, __IFLA_MAX);

	rta = rtas [IFLA_IFNAME];
	if (rta != NULL) {
		ifname = (const char *)RTA_DATA (rta);
	} else {
		ifname = NULL;
	}

	return update_netifs (0, ifi->ifi_index, ifname, ifi->ifi_type);
}

static int afnetlink_init_netifs ()
{
	int sk;
	isize_t err;
	char req [sizeof (struct nlmsghdr) + sizeof (struct rtmsg)];
	struct nlmsghdr *nlh = (struct nlmsghdr *)req;
	struct ifinfomsg *ifm = (struct ifinfomsg *)(nlh + 1);
	uint32_t seq = __nlmsg_seq++;

	sk = __socket_nl_rt ();
	if (sk < 0)
		return sk;

	memset (req, 0, sizeof req);

	nlh->nlmsg_len = sizeof req;
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_seq = seq;

	ifm->ifi_family = AOSL_AF_UNSPEC;

	err = send (sk, req, sizeof req, 0);
	if (err < (int)sizeof req) {
		err = -1;
		goto __close_sk;
	}

	for (;;) {
		char buf [16384];
		struct nlmsghdr *h;
		err = recv (sk, buf, sizeof buf, 0);
		if (err <= 0) {
			err = -1;
			goto __close_sk;
		}

		for (h = (struct nlmsghdr *)buf; NLMSG_OK (h, err); h = NLMSG_NEXT (h, err)) {
			struct nlmsgerr *err_h;

			switch (h->nlmsg_type) {
			case NLMSG_DONE:
				err = 0;
				goto __close_sk;
			case NLMSG_ERROR:
				err_h = (struct nlmsgerr *)NLMSG_DATA (h);
				if (h->nlmsg_len < NLMSG_LENGTH (sizeof (struct nlmsgerr))) {
					err = -AOSL_EMSGSIZE;
					goto __close_sk;
				}

				err = -err_h->error;
				goto __close_sk;
			case RTM_NEWLINK:
				break;
			case RTM_DELLINK:
				break;
			default:
				continue;
			}

			update_ifinfos (h);
		}
	}

__close_sk:
	close (sk);
	return (int)err;
}

static int __get_if_wireless(const char *if_name)
{
	if (!if_name)
		return 0;

#if defined(CONFIG_ANDROID)
	return 1;
#elif defined(__linux__) && defined(SIOCGIWNAME)
	struct ifreq ifr = {0};
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		return 0;
	}
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	if (ioctl(sock, SIOCGIWNAME, &ifr) != -1) {
		close(sock);
		return 1;
	} else {
		close(sock);
		return 0;
	}
#else
	return 0;
#endif
}

/**
 * Return value is whether the default route exists for the specified af.
 *      0: the default route does not exist for the specified af
 *      1: the default route exists for the specified af
 **/
static int __af_get_default_rt (uint16_t af, aosl_rt_t *rt, uint32_t *tb_id_p)
{
	int sk;
	isize_t err;
	int def_rt_exist = 0;
	int def_rt_cnt = 0;
	char req [sizeof (struct nlmsghdr) + sizeof (struct rtmsg)];
	struct nlmsghdr *nlh = (struct nlmsghdr *)req;
	struct rtmsg *rtm = (struct rtmsg *)(nlh + 1);

	/**
	 * This variable must be defined outside the following for loop,
	 * because the rta_oif/rta_gw etc pointers whick pointing to it
	 * and will be used outside the for loop, although this operation
	 * makes no trouble, but we use a memory outside its' scope.
	 **/
	char buf [16384];
	struct rtattr *rta_priority = NULL;
	struct rtattr *rta_oif = NULL;
	struct rtattr *rta_gw = NULL;
	uint32_t min_metric = 0xffffffffu; /* set to the max unsigned integer value */
	uint32_t seq = __nlmsg_seq++;

	aosl_invalidate_rt (rt);

	sk = __socket_nl_rt ();
	if (sk < 0)
		return 0;

	memset (req, 0, sizeof req);

	nlh->nlmsg_len = sizeof req;
	nlh->nlmsg_type = RTM_GETROUTE;
	nlh->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_seq = seq;

	rtm->rtm_family = af;

	err = send (sk, req, sizeof req, 0);
	if (err < (int)sizeof req)
		goto __close_sk;

	for (;;) {
		struct nlmsghdr *h;
		err = recv (sk, buf, sizeof buf, 0);
		if (err <= 0)
			goto __done;

		for (h = (struct nlmsghdr *)buf; NLMSG_OK (h, err); h = NLMSG_NEXT (h, err)) {
			struct rtattr *rtas [__RTA_MAX];
			struct rtmsg *rtm;
			struct nlmsgerr *err_h;

			switch (h->nlmsg_type) {
			case NLMSG_DONE:
				goto __done;
			case NLMSG_ERROR:
				err_h = (struct nlmsgerr *)NLMSG_DATA (h);
				if (h->nlmsg_len < NLMSG_LENGTH (sizeof (struct nlmsgerr))) {
					//aosl_errno = EMSGSIZE;
					goto __done;
				}
				//errno = err_h->error;
				goto __done;
			case RTM_NEWROUTE:
				break;
			case RTM_DELROUTE:
				/* fall through */
			default:
				continue;
			}

			rtm = NLMSG_DATA (h);
			if (rtm->rtm_type != RTN_UNICAST)
				continue;

			switch (rtm->rtm_family) {
			case AF_INET:
				/* fall through */
			case AF_INET6:
				break;
			default:
				continue;
			}

			/* the default route */
			if (rtm->rtm_dst_len == 0) {
				parse_rtattrs (RTM_RTA (rtm), RTM_PAYLOAD (h), rtas, __RTA_MAX);
				/**
				 * Skip the routes without output interface or gateway, this is
				 * very important, otherwise we would get unexpected result for
				 * the multiple route table cases, because some route table may
				 * have rtm_dst_len is zero, but have no RTA_GATEWAY attribute.
				 *
				 * The Xinke system of Zhiyang has a very strange behavior that
				 * the default route has no GW attribute, so we should consider
				 * these cases, if the default route having no GW attribute, we
				 * would clear the GW with zeros.
				 **/
				if (rtas [RTA_OIF] == NULL /*|| rtas [RTA_GATEWAY] == NULL*/)
					continue;

#ifdef CONFIG_AOSL_IPV6
				// TODO(zgx): ignore fe80:: scope link ipv6 route
#endif

				++def_rt_cnt;

				/**
				 * According to studying the linux/xnu kernel source code and testing it many times,
				 * we found that there are many differences between the mechanisms of how the
				 * xnu kernel and the linux kernel handles the default routes:
				 * 1. linux kernel allows many default routes to be added as long as the metrics
				 *	   are different, regardless the scope parameter, metric has a smaller value
				 *	   means this route has a higher priority(fib_info.fib_priority for IPv4 and
				 *	   rt6_info.rt6i_metric for IPv6), we could not add more than one default route
				 *	   with the same metric in linux even they have different scopes, but we can only
				 *	   add one default route with the same metric for IPv4 and IPv6 respectively, so
				 *	   we can retrieve at most 2 different default routes with the same metric in linux.
				 * 2. xnu kernel allows many default routes, but only ONE global default route is
				 *	   allowed, all other default routes must be interface scope(RTF_IFSCOPE),
				 *	   and xnu routing system has no metric parameter. But we can do add a global
				 *	   default route for both IPv4 and IPv6 at the same time, so we also can retrieve
				 *	   at most 2 different default routes with global scope in xnu.
				 *
				 * So, we only care the default route with minimal metric value for linux kernel.
				 **/
				rta_priority = rtas [RTA_PRIORITY];
				if (rta_priority != NULL) {
					uint32_t pri = *(uint32_t *)RTA_DATA (rta_priority);
					if (pri < min_metric) {
						min_metric = pri;
						rta_oif = rtas [RTA_OIF];
						rta_gw = rtas [RTA_GATEWAY];
					}
				} else if (rta_oif == NULL && rta_gw == NULL) {
					rta_oif = rtas [RTA_OIF];
					rta_gw = rtas [RTA_GATEWAY];
				}
			}
		}
	}

__done:
	if (rta_oif != NULL) {
		void *gw_af_addr;
		int oif = *(int *)RTA_DATA (rta_oif);
		aosl_netif_t *nif = netif_by_index (oif);

		rt->netif.if_index = oif;
		if (nif != NULL) {
			rt->netif.if_type = nif->if_type;
			strcpy (rt->netif.if_name, nif->if_name);
#ifdef CONFIG_ANDROID
			rt->if_cellnet = (int)(!!memcmp (nif->if_name, "wlan", 4));
#else
			rt->if_cellnet = 0; /* if_type: ARPHRD_ETHER/ARPHRD_AX25/ARPHRD_IEEE80211, but no cellnet type :-( */
#endif
			rt->if_wireless = __get_if_wireless(nif->if_name);
		} else {
			rt->netif.if_type = -1;
			sprintf (rt->netif.if_name, "%d", oif);
#ifdef CONFIG_ANDROID
			rt->if_cellnet = 1; /* default, is cellular net */
#else
			rt->if_cellnet = 0; /* if_type: ARPHRD_ETHER/ARPHRD_AX25/ARPHRD_IEEE80211, but no cellnet type :-( */
#endif
		}

		if (rta_gw != NULL) {
			if (rtm->rtm_family == AF_INET) {
				gw_af_addr = &rt->gw.in.sin_addr;
			} else {
				gw_af_addr = &rt->gw.in6.sin6_addr;
			}

			memcpy (gw_af_addr, RTA_DATA (rta_gw), RTA_PAYLOAD (rta_gw));
		} else {
			/**
			 * The Xinke system of Zhiyang has a very strange behavior that
			 * the default route has no GW attribute, so we should consider
			 * these cases, if the default route having no GW attribute, we
			 * would clear the GW with zeros.
			 **/
			memset (&rt->gw, 0, sizeof rt->gw);
		}

		rt->gw.sa.sa_family = rtm->rtm_family;
		if (tb_id_p != NULL) {
			{
				*tb_id_p = 0;
			}
		}

#if 0 //def CONFIG_AOSL_IPV6
		// ignore fe80:: scope link route
		if (rt->gw.sa.sa_family == AF_INET6) {
			if (rt->gw.in6.sin6_addr.s6_addr_v[0] == 0xfe && rt->gw.in6.sin6_addr.s6_addr_v[1] == 0x80) {
					aosl_invalidate_rt (rt);
				goto __close_sk;
			}
		}
#endif

		// ignore the case that more than two default routes
		rt->def_rt_cnt = def_rt_cnt > 2 ? 2 : def_rt_cnt;
		def_rt_exist = 1;
	}

__close_sk:
	close (sk);
	return def_rt_exist;
}

/**
 * Return value is the default route count both for IPv4 and IPv6 we retrieved.
 *      0: no default route
 *      1: only one of IPv4 or IPv6 has default route
 *      2: both IPv4 and IPv6 have default route
 **/
int os_get_def_rt (aosl_def_rt_t *def_rt)
{
	int got_v4 = 0, got_v6 = 0;

	got_v4 = __af_get_default_rt (AF_INET, &def_rt->IPv4, NULL);
#ifdef CONFIG_AOSL_IPV6
	got_v6 = __af_get_default_rt (AF_INET6, &def_rt->IPv6, NULL);
#endif

	return got_v4 + got_v6;
}

extern void check_report_def_rt_change_event (aosl_net_ev_func_t f, void *arg);

static void __process_rtmsg (void *buf, size_t len, aosl_net_ev_func_t f, void *arg)
{
	struct nlmsghdr *hdr;
	for (hdr = buf; hdr->nlmsg_type != NLMSG_DONE && NLMSG_OK (hdr, len); hdr = NLMSG_NEXT (hdr, len)) {
		switch (hdr->nlmsg_type) {
		case RTM_NEWLINK:
			/* fall through */
		case RTM_DELLINK:
			update_ifinfos (hdr);
			continue;
		case RTM_NEWROUTE:
			/* fall through */
		case RTM_DELROUTE:
			check_report_def_rt_change_event (f, arg);
			break;
		default:
			continue;
		}
	}
}

static void __on_af_netlink_data (void *data, size_t len, uintptr_t argc, uintptr_t argv [], const aosl_sk_addr_t *addr)
{
	aosl_net_ev_func_t f = (aosl_net_ev_func_t)argv [0];
	void *arg = (void *)argv [1];

	if (len > 0)
		__process_rtmsg (data, len, f, arg);
}

static void __on_af_netlink_event (int fd, int event, uintptr_t argc, uintptr_t argv []);

static int __create_and_attach_af_netlink (aosl_net_ev_func_t f, void *arg)
{
  int sk = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sk >= 0) {
		struct sockaddr_nl addr;
		memset (&addr, 0, sizeof addr);
		addr.nl_family = AF_NETLINK;
		addr.nl_groups = (1 << (RTNLGRP_LINK - 1)) | (1 << (RTNLGRP_IPV4_ROUTE - 1)) | (1 << (RTNLGRP_IPV6_ROUTE - 1));
		addr.nl_pid = 0;

		if (bind (sk, (struct sockaddr *)&addr, sizeof addr) < 0) {
			close (sk);
			goto __close_sk;
		}

		if (aosl_mpq_add_dgram_socket (sk, 16384, __on_af_netlink_data, __on_af_netlink_event, 2, f, arg) < 0) {
			close (sk);
			goto __close_sk;
		}

		af_netlink_fd = sk;
		return 0;
	}

__tag_out:
	return -1;

__close_sk:
	close (sk);
	goto __tag_out;
}

static void __on_af_netlink_event (int fd, int event, uintptr_t argc, uintptr_t argv [])
{
	if (event < 0) {
		aosl_net_ev_func_t f = (aosl_net_ev_func_t)argv [0];
		void *arg = (void *)argv [1];

		/* if an error encountered, recreate the socket */
		aosl_close (fd);
		__create_and_attach_af_netlink (f, arg);
	}
}

int os_subscribe_net_events (aosl_net_ev_func_t f, void *arg)
{
	int err;

	err = __create_and_attach_af_netlink (f, arg);
	if (err < 0)
		return err;

	afnetlink_init_netifs ();
	return err;
}

void os_unsubscribe_net_events (void)
{
	if (!aosl_fd_invalid (af_netlink_fd)) {
		aosl_close (af_netlink_fd);
		af_netlink_fd = -1;
	}
}

#endif // __linux__
