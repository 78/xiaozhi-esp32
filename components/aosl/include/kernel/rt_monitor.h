/***************************************************************************
 * Module:	AOSL netlink/interface helpers for linux header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __LINUX_ROUTE_MONITOR_H__
#define __LINUX_ROUTE_MONITOR_H__

#include <api/aosl_route.h>

#if defined (__linux__)
extern int os_get_def_rt (aosl_def_rt_t *def_rt);
extern int os_subscribe_net_events (aosl_net_ev_func_t f, void *arg);
extern void os_unsubscribe_net_events (void);
#else
static int os_get_def_rt (aosl_def_rt_t *def_rt) { (void)def_rt; return -1; }
static int os_subscribe_net_events (aosl_net_ev_func_t f, void *arg) { (void)f; (void)arg; return -1;}
static void os_unsubscribe_net_events (void) {}
#endif


#endif /* __LINUX_ROUTE_MONITOR_H__ */
