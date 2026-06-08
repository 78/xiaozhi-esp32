/***************************************************************************
 * Module:	Interface info cache for header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_KERNEL_NETIFS_H__
#define __AOSL_KERNEL_NETIFS_H__

#include <api/aosl_types.h>
#include <api/aosl_route.h>

extern void netifs_hash_init (void);
extern void netifs_hash_fini (void);

extern aosl_netif_t *netif_by_index (int idx);
extern int update_netifs (int del, int ifindex, ...);

#endif
