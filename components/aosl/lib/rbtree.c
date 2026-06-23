/***************************************************************************
 * Module:	Red-Black tree implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <kernel/kernel.h>
#include <kernel/rbtree.h>


#define	AOSL_RB_RED 0
#define	AOSL_RB_BLACK 1

#define rb_parent(r)   ((struct aosl_rb_node *)((r)->rb_parent_color & ~3))
#define rb_color(r)   ((r)->rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0)

static inline void rb_set_parent (struct aosl_rb_node *rb, struct aosl_rb_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (uintptr_t) p;
}
static inline void rb_set_color (struct aosl_rb_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

#define RB_EMPTY_ROOT(root)	((root)->aosl_rb_node == NULL)
#define RB_EMPTY_NODE(node)	(rb_parent(node) == node)
#define RB_CLEAR_NODE(node)	(rb_set_parent(node, node))

__export_in_so__ void aosl_rb_root_init (struct aosl_rb_root *root, aosl_rb_node_cmp_t cmp)
{
	root->rb_node = NULL;
	root->rb_cmp = cmp;
	root->count = 0;
}

static void __rb_rotate_left (struct aosl_rb_node *node, struct aosl_rb_root *root)
{
	struct aosl_rb_node *right = node->rb_right;
	struct aosl_rb_node *parent = rb_parent (node);

	if ((node->rb_right = right->rb_left))
		rb_set_parent (right->rb_left, node);
	right->rb_left = node;

	rb_set_parent (right, parent);

	if (parent) {
		if (node == parent->rb_left)
			parent->rb_left = right;
		else
			parent->rb_right = right;
	} else
		root->rb_node = right;
	rb_set_parent (node, right);
}

static void __rb_rotate_right (struct aosl_rb_node *node, struct aosl_rb_root *root)
{
	struct aosl_rb_node *left = node->rb_left;
	struct aosl_rb_node *parent = rb_parent (node);

	if ((node->rb_left = left->rb_right))
		rb_set_parent (left->rb_right, node);
	left->rb_right = node;

	rb_set_parent (left, parent);

	if (parent) {
		if (node == parent->rb_right)
			parent->rb_right = left;
		else
			parent->rb_left = left;
	} else
		root->rb_node = left;
	rb_set_parent (node, left);
}

void aosl_rb_insert_color (struct aosl_rb_node *node, struct aosl_rb_root *root)
{
	struct aosl_rb_node *parent, *gparent;

	while ((parent = rb_parent (node)) && rb_is_red (parent)) {
		gparent = rb_parent (parent);

		if (parent == gparent->rb_left) {
			{
				register struct aosl_rb_node *uncle = gparent->rb_right;

				if (uncle && rb_is_red (uncle)) {
					rb_set_black (uncle);
					rb_set_black (parent);
					rb_set_red (gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->rb_right == node) {
				register struct aosl_rb_node *tmp;

				__rb_rotate_left (parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black (parent);
			rb_set_red (gparent);
			__rb_rotate_right (gparent, root);
		} else {
			{
				register struct aosl_rb_node *uncle = gparent->rb_left;

				if (uncle && rb_is_red (uncle)) {
					rb_set_black (uncle);
					rb_set_black (parent);
					rb_set_red (gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->rb_left == node) {
				register struct aosl_rb_node *tmp;

				__rb_rotate_right (parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black (parent);
			rb_set_red (gparent);
			__rb_rotate_left (gparent, root);
		}
	}

	rb_set_black (root->rb_node);
	root->count++;
}

static void __rb_erase_color (struct aosl_rb_node *node, struct aosl_rb_node *parent, struct aosl_rb_root *root)
{
	struct aosl_rb_node *other;

	while ((!node || rb_is_black (node)) && node != root->rb_node) {
		if (parent->rb_left == node) {
			other = parent->rb_right;
			if (rb_is_red (other)) {
				rb_set_black (other);
				rb_set_red (parent);
				__rb_rotate_left (parent, root);
				other = parent->rb_right;
			}
			if ((!other->rb_left || rb_is_black (other->rb_left)) &&
				(!other->rb_right || rb_is_black (other->rb_right))) {
				rb_set_red (other);
				node = parent;
				parent = rb_parent (node);
			} else {
				if (!other->rb_right || rb_is_black (other->rb_right)) {
					struct aosl_rb_node *o_left;

					if ((o_left = other->rb_left))
						rb_set_black (o_left);
					rb_set_red (other);
					__rb_rotate_right (other, root);
					other = parent->rb_right;
				}
				rb_set_color (other, rb_color (parent));
				rb_set_black (parent);
				if (other->rb_right)
					rb_set_black (other->rb_right);
				__rb_rotate_left (parent, root);
				node = root->rb_node;
				break;
			}
		} else {
			other = parent->rb_left;
			if (rb_is_red (other)) {
				rb_set_black (other);
				rb_set_red (parent);
				__rb_rotate_right (parent, root);
				other = parent->rb_left;
			}
			if ((!other->rb_left || rb_is_black (other->rb_left)) &&
				(!other->rb_right || rb_is_black (other->rb_right))) {
				rb_set_red (other);
				node = parent;
				parent = rb_parent (node);
			} else {
				if (!other->rb_left || rb_is_black (other->rb_left)) {
					register struct aosl_rb_node *o_right;

					if ((o_right = other->rb_right))
						rb_set_black (o_right);
					rb_set_red (other);
					__rb_rotate_left (other, root);
					other = parent->rb_left;
				}
				rb_set_color (other, rb_color (parent));
				rb_set_black (parent);
				if (other->rb_left)
					rb_set_black (other->rb_left);
				__rb_rotate_right (parent, root);
				node = root->rb_node;
				break;
			}
		}
	}
	if (node)
		rb_set_black (node);
}

__export_in_so__ void aosl_rb_erase (struct aosl_rb_root *root, struct aosl_rb_node *node)
{
	struct aosl_rb_node *child, *parent;
	int color;

	if (!node->rb_left)
		child = node->rb_right;
	else if (!node->rb_right)
		child = node->rb_left;
	else {
		struct aosl_rb_node *old = node, *left;

		node = node->rb_right;
		while ((left = node->rb_left) != NULL)
			node = left;
		child = node->rb_right;
		parent = rb_parent (node);
		color = rb_color (node);

		if (child)
			rb_set_parent (child, parent);
		if (parent == old) {
			parent->rb_right = child;
			parent = node;
		} else
			parent->rb_left = child;

		node->rb_parent_color = old->rb_parent_color;
		node->rb_right = old->rb_right;
		node->rb_left = old->rb_left;

		if (rb_parent (old)) {
			if (rb_parent (old)->rb_left == old)
				rb_parent (old)->rb_left = node;
			else
				rb_parent (old)->rb_right = node;
		} else
			root->rb_node = node;

		rb_set_parent (old->rb_left, node);
		if (old->rb_right)
			rb_set_parent (old->rb_right, node);
		goto color;
	}

	parent = rb_parent (node);
	color = rb_color (node);

	if (child)
		rb_set_parent (child, parent);
	if (parent) {
		if (parent->rb_left == node)
			parent->rb_left = child;
		else
			parent->rb_right = child;
	} else
		root->rb_node = child;

  color:
	if (color == AOSL_RB_BLACK)
		__rb_erase_color (child, parent, root);
	root->count--;
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
struct aosl_rb_node *aosl_rb_first (struct aosl_rb_root *root)
{
	struct aosl_rb_node *n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

struct aosl_rb_node *aosl_rb_last (struct aosl_rb_root *root)
{
	struct aosl_rb_node *n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

struct aosl_rb_node *aosl_rb_next (struct aosl_rb_node *node)
{
	struct aosl_rb_node *parent;

	if (rb_parent (node) == node)
		return NULL;

	/* If we have a right-hand child, go down and then left as far
	   as we can. */
	if (node->rb_right) {
		node = node->rb_right;
		while (node->rb_left)
			node = node->rb_left;
		return node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	while ((parent = rb_parent (node)) && node == parent->rb_right)
		node = parent;

	return parent;
}

struct aosl_rb_node *aosl_rb_prev (struct aosl_rb_node *node)
{
	struct aosl_rb_node *parent;

	if (rb_parent (node) == node)
		return NULL;

	/* If we have a left-hand child, go down and then right as far
	   as we can. */
	if (node->rb_left) {
		node = node->rb_left;
		while (node->rb_right)
			node = node->rb_right;
		return node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	   is a right-hand child of its parent */
	while ((parent = rb_parent (node)) && node == parent->rb_left)
		node = parent;

	return parent;
}

void aosl_rb_replace_node (struct aosl_rb_node *victim, struct aosl_rb_node *new, struct aosl_rb_root *root)
{
	struct aosl_rb_node *parent = rb_parent (victim);

	/* Set the surrounding nodes to point to the replacement */
	if (parent) {
		if (victim == parent->rb_left)
			parent->rb_left = new;
		else
			parent->rb_right = new;
	} else {
		root->rb_node = new;
	}
	if (victim->rb_left)
		rb_set_parent (victim->rb_left, new);
	if (victim->rb_right)
		rb_set_parent (victim->rb_right, new);

	/* Copy the pointers/colour from the victim to the replacement */
	*new = *victim;
}


__export_in_so__ struct aosl_rb_node **aosl_vfind_rb_links (struct aosl_rb_root *root, struct aosl_rb_node **rb_parent,
					struct aosl_rb_node **pprev, struct aosl_rb_node **pnext, struct aosl_rb_node *node, va_list args)
{
	struct aosl_rb_node **__rb_link = &root->rb_node;
	struct aosl_rb_node *__rb_parent = NULL;
	struct aosl_rb_node *rb_prev = NULL;
	struct aosl_rb_node *rb_next = NULL;

	while (*__rb_link) {
		int r;
		va_list tmpargs;
		__rb_parent = *__rb_link;

		va_copy (tmpargs, args);
		r = root->rb_cmp (__rb_parent, node, tmpargs);
		va_end (tmpargs);
		if (r > 0) {
			rb_next = __rb_parent;
			__rb_link = &__rb_parent->rb_left;
		} else {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		}
	}

	if (rb_parent)
		*rb_parent = __rb_parent;

	if (pprev)
		*pprev = rb_prev;

	if (pnext)
		*pnext = rb_next;

	return __rb_link;
}

__export_in_so__ struct aosl_rb_node **aosl_find_rb_links (struct aosl_rb_root *root, struct aosl_rb_node **rb_parent,
							struct aosl_rb_node **pprev, struct aosl_rb_node **pnext, struct aosl_rb_node *node, ...)
{
	struct aosl_rb_node **__rb_link = &root->rb_node;
	struct aosl_rb_node *__rb_parent = NULL;
	struct aosl_rb_node *rb_prev = NULL;
	struct aosl_rb_node *rb_next = NULL;

	while (*__rb_link) {
		int r;
		va_list args;
		__rb_parent = *__rb_link;

		va_start (args, node);
		r = root->rb_cmp (__rb_parent, node, args);
		va_end (args);
		if (r > 0) {
			rb_next = __rb_parent;
			__rb_link = &__rb_parent->rb_left;
		} else {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		}
	}

	if (rb_parent)
		*rb_parent = __rb_parent;

	if (pprev)
		*pprev = rb_prev;

	if (pnext)
		*pnext = rb_next;

	return __rb_link;
}

__export_in_so__ struct aosl_rb_node *aosl_vfind_rb_node (struct aosl_rb_root *root, struct aosl_rb_node *node, va_list args)
{
	struct aosl_rb_node *__rb_node = root->rb_node;

	while (__rb_node) {
		int r;
		va_list tmpargs;

		va_copy (tmpargs, args);
		r = root->rb_cmp (__rb_node, node, tmpargs);
		va_end (tmpargs);
		if (r == 0)
			return __rb_node;

		if (r > 0) {
			__rb_node = __rb_node->rb_left;
		} else {
			__rb_node = __rb_node->rb_right;
		}
	}

	return NULL;
}

__export_in_so__ struct aosl_rb_node *aosl_find_rb_node (struct aosl_rb_root *root, struct aosl_rb_node *node, ...)
{
	struct aosl_rb_node *__rb_node = root->rb_node;

	while (__rb_node) {
		int r;
		va_list args;

		va_start (args, node);
		r = root->rb_cmp (__rb_node, node, args);
		va_end (args);
		if (r == 0)
			return __rb_node;

		if (r > 0) {
			__rb_node = __rb_node->rb_left;
		} else {
			__rb_node = __rb_node->rb_right;
		}
	}

	return NULL;
}

__export_in_so__ void aosl_rb_insert_node (struct aosl_rb_root *root, struct aosl_rb_node *node, ...)
{
	va_list args;
	struct aosl_rb_node **rb_link, *rb_parent;
	va_start (args, node);
	rb_link = aosl_vfind_rb_links (root, &rb_parent, NULL, NULL, node, args);
	va_end (args);
	rb_insert (root, node, rb_link, rb_parent);
}

__export_in_so__ struct aosl_rb_node *aosl_rb_remove (struct aosl_rb_root *root, struct aosl_rb_node *node, ...)
{
	va_list args;
	struct aosl_rb_node *aosl_rb_node;

	va_start (args, node);
	aosl_rb_node = aosl_vfind_rb_node (root, node, args);
	va_end (args);

	if (aosl_rb_node != NULL)
		aosl_rb_erase (root, aosl_rb_node);

	return aosl_rb_node;
}

static void __rb_traverse_dlr (struct aosl_rb_node *node, int *done_p, aosl_rb_walk_func_t func, void *arg)
{
	if (node != NULL) {
		if (done_p == NULL || !*done_p) {
			int done = func (node, arg);
			if (done_p != NULL)
				*done_p = done;
		}

		if (done_p == NULL || !*done_p)
			__rb_traverse_dlr (node->rb_left, done_p, func, arg);

		if (done_p == NULL || !*done_p)
			__rb_traverse_dlr (node->rb_right, done_p, func, arg);
	}
}

__export_in_so__ void aosl_rb_traverse_dlr (struct aosl_rb_root *root, aosl_rb_walk_func_t func, void *arg)
{
	int done = 0;
	__rb_traverse_dlr (root->rb_node, &done, func, arg);
}

static void __rb_traverse_ldr (struct aosl_rb_node *node, int *done_p, aosl_rb_walk_func_t func, void *arg)
{
	if (node != NULL) {
		if (done_p == NULL || !*done_p)
			__rb_traverse_ldr (node->rb_left, done_p, func, arg);

		if (done_p == NULL || !*done_p) {
			int done = func (node, arg);
			if (done_p != NULL)
				*done_p = done;
		}

		if (done_p == NULL || !*done_p)
			__rb_traverse_ldr (node->rb_right, done_p, func, arg);
	}
}

__export_in_so__ void aosl_rb_traverse_ldr (struct aosl_rb_root *root, aosl_rb_walk_func_t func, void *arg)
{
	int done = 0;
	__rb_traverse_ldr (root->rb_node, &done, func, arg);
}

static void __rb_traverse_lrd (struct aosl_rb_node *node, int *done_p, aosl_rb_walk_func_t func, void *arg)
{
	if (node != NULL) {
		if (done_p == NULL || !*done_p)
			__rb_traverse_lrd (node->rb_left, done_p, func, arg);

		if (done_p == NULL || !*done_p)
			__rb_traverse_lrd (node->rb_right, done_p, func, arg);

		if (done_p == NULL || !*done_p) {
			int done = func (node, arg);
			if (done_p != NULL)
				*done_p = done;
		}
	}
}

__export_in_so__ void aosl_rb_traverse_lrd (struct aosl_rb_root *root, aosl_rb_walk_func_t func, void *arg)
{
	int done = 0;
	__rb_traverse_lrd (root->rb_node, &done, func, arg);
}