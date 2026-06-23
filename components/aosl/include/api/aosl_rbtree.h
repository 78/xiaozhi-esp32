/***************************************************************************
 * Module:	AOSL Red-Black tree header file
 *
 * Copyright (c) 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef	__AOSL_RBTREE_H__
#define	__AOSL_RBTREE_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>


#ifdef __cplusplus
extern "C" {
#endif


struct aosl_rb_node {
	uintptr_t rb_parent_color;
	struct aosl_rb_node *rb_right;
	struct aosl_rb_node *rb_left;
};

typedef int (*aosl_rb_node_cmp_t) (struct aosl_rb_node *rb_node, struct aosl_rb_node *node, va_list args);

struct aosl_rb_root {
	struct aosl_rb_node *rb_node;
	aosl_rb_node_cmp_t rb_cmp;
	uintptr_t count;
};


/**
 * @brief Get the container struct from a red-black tree node pointer.
 * @param [in] ptr     pointer to the aosl_rb_node member
 * @param [in] type    the type of the container struct
 * @param [in] member  the name of the aosl_rb_node member within the struct
 */
#define	aosl_rb_entry(ptr, type, member) container_of(ptr, type, member)

/**
 * @brief Initialize a red-black tree root with the specified comparison function.
 * @param [out] root  pointer to the rb_root to initialize
 * @param [in]  cmp   the node comparison function
 **/
extern __aosl_api__ void aosl_rb_root_init (struct aosl_rb_root *root, aosl_rb_node_cmp_t cmp);


/**
 * @brief Find the insertion point (link pointer) for a node in the tree (va_list version).
 * @param [in]  root       the tree root
 * @param [out] rb_parent  the parent node of the insertion point
 * @param [out] pprev      the logical previous node (or NULL)
 * @param [out] pnext      the logical next node (or NULL)
 * @param [in]  node       the node to find the position for
 * @param [in]  args       additional comparison arguments
 * @return           pointer to the link pointer for insertion, or NULL if duplicate found
 **/
extern __aosl_api__ struct aosl_rb_node **aosl_vfind_rb_links (struct aosl_rb_root *root, struct aosl_rb_node **rb_parent,
	struct aosl_rb_node **pprev, struct aosl_rb_node **pnext, struct aosl_rb_node *node, va_list args);

/**
 * @brief Find the insertion point (link pointer) for a node in the tree.
 * @param [in]  root       the tree root
 * @param [out] rb_parent  the parent node of the insertion point
 * @param [out] pprev      the logical previous node (or NULL)
 * @param [out] pnext      the logical next node (or NULL)
 * @param [in]  node       the node to find the position for
 * @param [in]  ...        additional comparison arguments
 * @return           pointer to the link pointer for insertion, or NULL if duplicate found
 **/
extern __aosl_api__ struct aosl_rb_node **aosl_find_rb_links (struct aosl_rb_root *root, struct aosl_rb_node **rb_parent,
		struct aosl_rb_node **pprev, struct aosl_rb_node **pnext, struct aosl_rb_node *node, ...);

/**
 * @brief Find a node in the tree matching the given key node (va_list version).
 * @param [in] root  the tree root
 * @param [in] node  the key node to search for
 * @param [in] args  additional comparison arguments
 * @return      the matching node, or NULL if not found
 **/
extern __aosl_api__ struct aosl_rb_node *aosl_vfind_rb_node (struct aosl_rb_root *root, struct aosl_rb_node *node, va_list args);

/**
 * @brief Find a node in the tree matching the given key node.
 * @param [in] root  the tree root
 * @param [in] node  the key node to search for
 * @param [in] ...   additional comparison arguments
 * @return      the matching node, or NULL if not found
 **/
extern __aosl_api__ struct aosl_rb_node *aosl_find_rb_node (struct aosl_rb_root *root, struct aosl_rb_node *node, ...);

/**
 * @brief Insert a node into the red-black tree.
 * @param [in,out] root  the tree root
 * @param [in]     node  the node to insert
 * @param [in]     ...   additional comparison arguments
 **/
extern __aosl_api__ void aosl_rb_insert_node (struct aosl_rb_root *root, struct aosl_rb_node *node, ...);

/**
 * @brief Find and remove a matching node from the red-black tree.
 * @param [in,out] root  the tree root
 * @param [in]     node  the key node to find and remove
 * @param [in]     ...   additional comparison arguments
 * @return      the removed node, or NULL if not found
 **/
extern __aosl_api__ struct aosl_rb_node *aosl_rb_remove (struct aosl_rb_root *root, struct aosl_rb_node *node, ...);

/**
 * @brief Erase a specific node from the red-black tree (node must be in the tree).
 * @param [in,out] root  the tree root
 * @param [in]     node  the node to erase
 **/
extern __aosl_api__ void aosl_rb_erase (struct aosl_rb_root *root, struct aosl_rb_node *node);

typedef int (*aosl_rb_walk_func_t) (struct aosl_rb_node *node, void *arg);

/**
 * @brief Traverse the tree in pre-order (root, left, right).
 * @param [in] root  the tree root
 * @param [in] func  callback invoked for each node; return non-zero to stop
 * @param [in] arg   user argument passed to the callback
 **/
extern __aosl_api__ void aosl_rb_traverse_dlr (struct aosl_rb_root *root, aosl_rb_walk_func_t func, void *arg);

/**
 * @brief Traverse the tree in in-order (left, root, right) in sorted order.
 * @param [in] root  the tree root
 * @param [in] func  callback invoked for each node; return non-zero to stop
 * @param [in] arg   user argument passed to the callback
 **/
extern __aosl_api__ void aosl_rb_traverse_ldr (struct aosl_rb_root *root, aosl_rb_walk_func_t func, void *arg);

/**
 * @brief Traverse the tree in post-order (left, right, root).
 * @param [in] root  the tree root
 * @param [in] func  callback invoked for each node; return non-zero to stop
 * @param [in] arg   user argument passed to the callback
 **/
extern __aosl_api__ void aosl_rb_traverse_lrd (struct aosl_rb_root *root, aosl_rb_walk_func_t func, void *arg);

/**
 * @brief Get the next node in sorted order (in-order successor).
 * @param [in] node  the current node
 * @return      the next node, or NULL if this is the last
 **/
extern __aosl_api__ struct aosl_rb_node *aosl_rb_next (struct aosl_rb_node *);

/**
 * @brief Get the previous node in sorted order (in-order predecessor).
 * @param [in] node  the current node
 * @return      the previous node, or NULL if this is the first
 **/
extern __aosl_api__ struct aosl_rb_node *aosl_rb_prev (struct aosl_rb_node *);

/**
 * @brief Get the first (smallest) node in the tree.
 * @param [in] root  the tree root
 * @return      the first node, or NULL if the tree is empty
 **/
extern __aosl_api__ struct aosl_rb_node *aosl_rb_first (struct aosl_rb_root *);

/**
 * @brief Get the last (largest) node in the tree.
 * @param [in] root  the tree root
 * @return      the last node, or NULL if the tree is empty
 **/
extern __aosl_api__ struct aosl_rb_node *aosl_rb_last (struct aosl_rb_root *);

/**
 * @brief Clear all nodes from a red-black tree and free each entry.
 * @param [in,out] root       pointer to the tree root
 * @param [in]     type       the container struct type
 * @param [in]     member     the rb_node member name within the struct
 * @param [in]     type_free  the function to free each entry (e.g. aosl_free)
 **/
#define aosl_rb_clear(root, type, member, type_free)                           \
do {                                                                           \
	struct aosl_rb_node *node;                                                   \
	while ((node = (root)->rb_node) != NULL) {                                   \
		type *pos = aosl_rb_entry(node, type, member);                             \
		aosl_rb_erase(root, node);                                                 \
		type_free(pos);                                                            \
	}                                                                            \
} while(0)


#ifdef __cplusplus
}
#endif

#endif /* __AOSL_RBTREE_H__ */
