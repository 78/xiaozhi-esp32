/***************************************************************************
 * Module:	AOSL list implementation header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_LIST_H__
#define __AOSL_LIST_H__


#include <api/aosl_types.h>
#include <api/aosl_defs.h>


#ifdef __cplusplus
extern "C" {
#endif


#define AOSL_LIST_POISON1  ((void *)((uintptr_t)0x00100100 + 1))
#define AOSL_LIST_POISON2  ((void *)((uintptr_t)0x00200200 + 3))


struct aosl_list_head {
	struct aosl_list_head *next;
	struct aosl_list_head *prev;
};


#define AOSL_LIST_HEAD_INIT(name) { &(name), &(name) }

#define AOSL_DEFINE_LIST_HEAD(name) \
	struct aosl_list_head name = AOSL_LIST_HEAD_INIT(name)

static __inline__ void aosl_list_head_init(struct aosl_list_head *list)
{
	list->next = list;
	list->prev = list;
}

static __inline__ void aosl_list_head_poison (struct aosl_list_head *list)
{
	list->next = (struct aosl_list_head *)AOSL_LIST_POISON1;
	list->prev = (struct aosl_list_head *)AOSL_LIST_POISON2;
}

static inline void __aosl_list_add (struct aosl_list_head *new_node, struct aosl_list_head *prev, struct aosl_list_head *next)
{
	next->prev = new_node;
	new_node->next = next;
	new_node->prev = prev;
	prev->next = new_node;
}

/**
 * @brief Add a new entry as the head of the list.
 * Parameters:
 *        new: new entry to be added
 *       head: list head to add it after
 */
static inline void aosl_list_add (struct aosl_list_head *new_node, struct aosl_list_head *head)
{
	__aosl_list_add (new_node, head, head->next);
}


/**
 * @brief Add a new entry as the tail of the list
 * Parameters:
 *        new: new entry to be added
 *       head: list head to add it before
 */
static inline void aosl_list_add_tail (struct aosl_list_head *new_node, struct aosl_list_head *head)
{
	__aosl_list_add (new_node, head->prev, head);
}

static inline void __aosl_list_del (struct aosl_list_head * prev, struct aosl_list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void __aosl_list_del_entry (struct aosl_list_head *entry)
{
	__aosl_list_del (entry->prev, entry->next);
}

static inline void aosl_list_del (struct aosl_list_head *entry)
{
	__aosl_list_del (entry->prev, entry->next);
	aosl_list_head_poison (entry); /* poison the entry */
}

static inline void aosl_list_del_init (struct aosl_list_head *entry)
{
	__aosl_list_del_entry (entry);
	aosl_list_head_init (entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void aosl_list_move(struct aosl_list_head *list, struct aosl_list_head *head)
{
	__aosl_list_del_entry(list);
	aosl_list_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void aosl_list_move_tail(struct aosl_list_head *list,
				  struct aosl_list_head *head)
{
	__aosl_list_del_entry(list);
	aosl_list_add_tail(list, head);
}

static inline int aosl_list_empty (const struct aosl_list_head *head)
{
	return head->next == head;
}

static __inline__ int aosl_list_valid(const struct aosl_list_head *head)
{
	if (!head) return 0;
	if (head->next == AOSL_LIST_POISON1 || head->prev == AOSL_LIST_POISON2) return 0;
	return 1;
}

/**
 * @brief Tests whether a list is empty and not being modified
 * Parameters:
 *       head: the list to test
 * Return Value:
 *       non-zero if the list is empty and not being modified
 *       zero for otherwise
 * Remarks:
 *   Tests whether a list is empty _and_ checks that no other thread
 *   might be in the process of modifying either member (next or prev)
 *   Please be careful that using this function without synchronization
 *   can only be safe if the only activity that can happen to the list
 *   entry is aosl_list_del_init(). Eg. it cannot be used if another
 *   thread could re-add it.
 */
static inline int aosl_list_empty_careful (const struct aosl_list_head *head)
{
	struct aosl_list_head *next = head->next;
	return (next == head) && (next == head->prev);
}

static inline int aosl_list_is_singular (const struct aosl_list_head *head)
{
	return !aosl_list_empty (head) && (head->next == head->prev);
}

static inline void __aosl_list_splice (const struct aosl_list_head *list, struct aosl_list_head *prev, struct aosl_list_head *next)
{
	struct aosl_list_head *first = list->next;
	struct aosl_list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

static inline void aosl_list_splice_tail_init (struct aosl_list_head *list, struct aosl_list_head *head)
{
	if (!aosl_list_empty(list)) {
		__aosl_list_splice(list, head->prev, head);
		aosl_list_head_init(list);
	}
}

#define aosl_list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define aosl_list_first_entry(ptr, type, member) \
	aosl_list_entry((ptr)->next, type, member)

#define aosl_list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define aosl_list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

#define aosl_list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define aosl_list_for_each_prev_safe(pos, n, head) \
	for (pos = (head)->prev, n = pos->prev; \
	     pos != (head); \
	     pos = n, n = pos->prev)

#define aosl_list_for_each_entry_t(type, pos, head, member)				\
	for (pos = aosl_list_entry((head)->next, type, member);	\
	     &pos->member != (head); 	\
	     pos = aosl_list_entry(pos->member.next, type, member))

#define aosl_list_for_each_entry_reverse_t(type, pos, head, member)			\
	for (pos = aosl_list_entry((head)->prev, type, member);	\
	     &pos->member != (head); 	\
	     pos = aosl_list_entry(pos->member.prev, type, member))

#define aosl_list_for_each_entry_safe_t(type, pos, n, head, member)			\
	for (pos = aosl_list_entry((head)->next, type, member),	\
		n = aosl_list_entry(pos->member.next, type, member);	\
	     &pos->member != (head); 					\
	     pos = n, n = aosl_list_entry(n->member.next, type, member))

#define aosl_list_for_each_entry_safe_reverse_t(type, pos, n, head, member)		\
	for (pos = aosl_list_entry((head)->prev, type, member),	\
		n = aosl_list_entry(pos->member.prev, type, member);	\
	     &pos->member != (head); 					\
	     pos = n, n = aosl_list_entry(n->member.prev, type, member))

static __inline__ struct aosl_list_head *aosl_list_head (struct aosl_list_head *head)
{
	if (!aosl_list_empty (head))
		return head->next;

	return NULL;
}

static __inline__ struct aosl_list_head *aosl_list_next (struct aosl_list_head *head, struct aosl_list_head *node)
{
	node = node->next;
	if (node != head)
		return node;

	return NULL;
}

static __inline__ struct aosl_list_head *aosl_list_prev (struct aosl_list_head *head, struct aosl_list_head *node)
{
	node = node->prev;
	if (node != head)
		return node;

	return NULL;
}

static inline struct aosl_list_head *aosl_list_tail (struct aosl_list_head *head)
{
	if (!aosl_list_empty (head))
		return head->prev;

	return NULL;
}

#ifdef __GNUC__
#define aosl_list_head_entry(list, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_head (list); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})

#define aosl_list_next_entry(list, pos, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_next (list, &(pos)->member); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})

#define aosl_list_prev_entry(list, pos, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_prev (list, &(pos)->member); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})

#define aosl_list_tail_entry(list, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_tail (list); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})
#else
#define aosl_list_head_entry(list, type, member) \
	((aosl_list_head (list) != NULL) ? aosl_list_entry (aosl_list_head (list), type, member) : NULL)

#define aosl_list_next_entry(list, pos, type, member) \
	((aosl_list_next (list, &(pos)->member) != NULL) ? aosl_list_entry (aosl_list_next (list, &(pos)->member), type, member) : NULL)

#define aosl_list_prev_entry(list, pos, type, member) \
	((aosl_list_prev (list, &(pos)->member) != NULL) ? aosl_list_entry (aosl_list_prev (list, &(pos)->member), type, member) : NULL)

#define aosl_list_tail_entry(list, type, member) \
	((aosl_list_tail (list) != NULL) ? aosl_list_entry (aosl_list_tail (list), type, member) : NULL)
#endif

static inline struct aosl_list_head *aosl_list_remove_head (struct aosl_list_head *head)
{
	if (!aosl_list_empty (head)) {
		struct aosl_list_head *__$p$ = head->next;
		aosl_list_del (__$p$);
		return __$p$;
	}

	return NULL;
}

static inline struct aosl_list_head *aosl_list_remove_head_init (struct aosl_list_head *head)
{
	if (!aosl_list_empty (head)) {
		struct aosl_list_head *__$p$ = head->next;
		aosl_list_del_init (__$p$);
		return __$p$;
	}

	return NULL;
}

#ifdef __GNUC__
#define aosl_list_remove_head_entry(list, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_remove_head (list); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})

#define aosl_list_remove_head_entry_init(list, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_remove_head_init (list); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})
#else
#define aosl_list_remove_head_entry(list, type, member) \
	((aosl_list_head (list) != NULL) ? aosl_list_entry (aosl_list_remove_head (list), type, member) : NULL)

#define aosl_list_remove_head_entry_init(list, type, member) \
	((aosl_list_head (list) != NULL) ? aosl_list_entry (aosl_list_remove_head_init (list), type, member) : NULL)
#endif

static inline struct aosl_list_head *aosl_list_remove_tail (struct aosl_list_head *head)
{
	if (!aosl_list_empty (head)) {
		struct aosl_list_head *__$p$ = head->prev;
		aosl_list_del (__$p$);
		return __$p$;
	}

	return NULL;
}

static inline struct aosl_list_head *aosl_list_remove_tail_init (struct aosl_list_head *head)
{
	if (!aosl_list_empty (head)) {
		struct aosl_list_head *__$p$ = head->prev;
		aosl_list_del_init (__$p$);
		return __$p$;
	}

	return NULL;
}

#ifdef __GNUC__
#define aosl_list_remove_tail_entry(list, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_remove_tail (list); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})

#define aosl_list_remove_tail_entry_init(list, type, member) \
({ \
	struct aosl_list_head *__$p$ = aosl_list_remove_tail_init (list); \
	type *entry = (__$p$ != NULL) ? aosl_list_entry (__$p$, type, member) : NULL; \
	entry; \
})
#else
#define aosl_list_remove_tail_entry(list, type, member) \
	((aosl_list_tail (list) != NULL) ? aosl_list_entry (aosl_list_remove_tail (list), type, member) : NULL)

#define aosl_list_remove_tail_entry_init(list, type, member) \
	((aosl_list_tail (list) != NULL) ? aosl_list_entry (aosl_list_remove_tail_init (list), type, member) : NULL)
#endif


#ifdef __cplusplus
}
#endif


#endif /* __AOSL_LIST_H__ */