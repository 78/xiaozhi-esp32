/***************************************************************************
 * Module:	rbtree header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __KERNEL_RBTREE_H__
#define __KERNEL_RBTREE_H__


#include <api/aosl_rbtree.h>


extern void aosl_rb_insert_color (struct aosl_rb_node *, struct aosl_rb_root *);

/* Find logical next and previous nodes in a tree */
extern struct aosl_rb_node *aosl_rb_next (struct aosl_rb_node *);
extern struct aosl_rb_node *aosl_rb_prev (struct aosl_rb_node *);
extern struct aosl_rb_node *aosl_rb_first (struct aosl_rb_root *);
extern struct aosl_rb_node *aosl_rb_last (struct aosl_rb_root *);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void aosl_rb_replace_node (struct aosl_rb_node *victim, struct aosl_rb_node *new_node, struct aosl_rb_root *root);

static inline void
rb_link_node (struct aosl_rb_node *node, struct aosl_rb_node *parent, struct aosl_rb_node **rb_link)
{
	node->rb_parent_color = (uintptr_t) parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node;
}

static __inline__ void rb_insert (struct aosl_rb_root *root, struct aosl_rb_node *node,
			struct aosl_rb_node **rb_link, struct aosl_rb_node *rb_parent)
{
	rb_link_node (node, rb_parent, rb_link);
	aosl_rb_insert_color (node, root);
}


#endif /* __KERNEL_RBTREE_H__ */
