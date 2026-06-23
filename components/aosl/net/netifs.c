/***************************************************************************
 * Module:	Interface info cache for implementation file
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
#include <api/aosl_mm.h>
#include <kernel/err.h>
#include <kernel/list.h>
#include <api/aosl_mpq_net.h>
#include <api/aosl_route.h>


struct netif_node {
	struct aosl_list_head node;

	aosl_netif_t netif;
};

#define IFINFO_HASH_SIZE 16
static struct aosl_list_head __netifs_hash [IFINFO_HASH_SIZE];
static struct aosl_list_head __free_netifs = AOSL_LIST_HEAD_INIT (__free_netifs);


static __inline__ struct netif_node *__netif_node_by_index (unsigned int idx)
{
	int hash;
	struct netif_node *netif = NULL;

	hash = idx & (IFINFO_HASH_SIZE - 1);
	aosl_list_for_each_entry_t (struct netif_node, netif, &__netifs_hash [hash], node) {
		if (netif->netif.if_index == (int)idx)
			return netif;
	}

	return NULL;
}

aosl_netif_t *netif_by_index (int idx)
{
	struct netif_node *netif;

	if (idx < 0)
		return NULL;

	netif = __netif_node_by_index (idx);
	if (netif != NULL)
		return &netif->netif;

	return NULL;
}

int update_netifs (int del, int ifindex, ...)
{
	int hash;
	struct netif_node *node;
	va_list args;
	const char *ifname;
	int iftype;

	if (ifindex < 0)
		return -AOSL_EINVAL;

	node = __netif_node_by_index (ifindex);

	if (del) {
		if (node != NULL) {
			aosl_list_del (&node->node);
			aosl_list_add_tail (&node->node, &__free_netifs);
			return 0;
		}

		return -AOSL_ENOENT;
	}

	va_start (args, ifindex);
	ifname = va_arg (args, const char *);
	iftype = va_arg (args, int);
	va_end (args);

	if (node != NULL) {
		node->netif.if_type = iftype;

		if (ifname != NULL) {
			if (strcmp (node->netif.if_name, ifname))
				snprintf (node->netif.if_name, sizeof node->netif.if_name, "%s", ifname);
		} else {
			node->netif.if_name [0] = '\0';
		}

		return 0;
	}

	node = aosl_list_remove_head_entry (&__free_netifs, struct netif_node, node);
	if (node == NULL) {
		node = aosl_malloc (sizeof (struct netif_node));
		if (node == NULL)
			return -AOSL_ENOMEM;
	}

	node->netif.if_index = ifindex;
	node->netif.if_type = iftype;
	if (ifname != NULL) {
		snprintf (node->netif.if_name, sizeof node->netif.if_name, "%s", ifname);
	} else {
		node->netif.if_name [0] = '\0';
	}

	hash = ifindex & (IFINFO_HASH_SIZE - 1);
	aosl_list_add_tail (&node->node, &__netifs_hash [hash]);
	return 0;
}

void netifs_hash_init (void)
{
	int i;
	for (i = 0; i < IFINFO_HASH_SIZE; i++) {
		aosl_list_head_init (&__netifs_hash [i]);
	}
}

void netifs_hash_fini (void)
{
	int i;
	struct netif_node *node;

	for (i = 0; i < IFINFO_HASH_SIZE; i++) {
		for (;;) {
			node = aosl_list_remove_head_entry (&__netifs_hash [i], struct netif_node, node);
			if (node == NULL)
				break;

			aosl_free (node);
		}
	}

	for (;;) {
		node = aosl_list_remove_head_entry (&__free_netifs, struct netif_node, node);
		if (node == NULL)
			break;

		aosl_free (node);
	}
}