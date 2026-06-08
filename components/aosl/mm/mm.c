/***************************************************************************
 * Module:	memory management relatives implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef CONFIG_AOSL_MEM_STAT
#undef CONFIG_AOSL_MEM_DUMP
#endif

#include <stdlib.h>
#include <string.h>
#include <kernel/kernel.h>
#include <kernel/rbtree.h>
#include <api/aosl_mm.h>
#include <hal/aosl_hal_memory.h>

#define UNUSED(expr) (void)(expr)

#define MALLOC    aosl_hal_malloc
#define FREE      aosl_hal_free
#define REALLOC   aosl_hal_realloc

#if defined(CONFIG_AOSL_MEM_STAT)

#include <kernel/thread.h>
struct mm_ptr_node {
	struct aosl_rb_node node;
	void *ptr;  // key
	size_t size;

#ifdef CONFIG_AOSL_MEM_DUMP
	const char *func;
	int line;
#endif
};

#define MM_IGNORE_NODE_CHECK(node)                                             \
{                                                                              \
	if (strstr(node->func, "__mpqp_") ||                                         \
		strstr(node->func, "__q_")    ||                                           \
		!strcmp(node->func, "____add_f") ||                                        \
		!strcmp(node->func, "__create_timer_on_q") ||                              \
		!strcmp(node->func, "expand_fdtable_locked")) {                            \
		return 0;                                                                  \
	}                                                                            \
}

static int __mm_ptr_cmp (struct aosl_rb_node *node1, struct aosl_rb_node *node2, va_list args)
{
	struct mm_ptr_node *rb_entry = aosl_rb_entry (node1, struct mm_ptr_node, node);
	void *ptr;

	if (node2 != NULL) {
		ptr = aosl_rb_entry (node2, struct mm_ptr_node, node)->ptr;
	} else {
		ptr = va_arg (args, void *);
	}

	if (rb_entry->ptr > ptr)
		return 1;

	if (rb_entry->ptr < ptr)
		return -1;

	return 0;
}

static int __mm_ptr_walk (struct aosl_rb_node *node, void *arg)
{
#ifdef CONFIG_AOSL_MEM_DUMP
	uint32_t* index = (uint32_t *)arg;
	struct mm_ptr_node *ptr_node = aosl_rb_entry(node, struct mm_ptr_node, node);

#if 1 // ignore some no care
	MM_IGNORE_NODE_CHECK(ptr_node);
#endif

	aosl_log(AOSL_LOG_CRIT, "[%4u]ptr_node: ptr=%p size=%7u %s:%d\n", (*index)++,
					 ptr_node->ptr, (uint32_t)ptr_node->size, ptr_node->func, ptr_node->line);
#endif
	return 0;
}

#ifdef CONFIG_AOSL_MEM_DUMP
struct mm_pos_node {
	struct aosl_rb_node node;
	const char *func; // key1
	int line;         // key2
	int cnts;
	size_t size;
};

struct mm_walk_arg {
	uint32_t index;
	char *buf;
	int len;
};

static int __mm_pos_cmp (struct aosl_rb_node *node1, struct aosl_rb_node *node2, va_list args)
{
	struct mm_pos_node tmp_node;
	struct mm_pos_node *pos_node1 = aosl_rb_entry (node1, struct mm_pos_node, node);
	struct mm_pos_node *pos_node2 = NULL;

	// get node2
	if (node2) {
		pos_node2 = aosl_rb_entry (node2, struct mm_pos_node, node);
	} else {
		tmp_node.func = va_arg (args, const char*);
		tmp_node.line = va_arg (args, int);
		pos_node2 = &tmp_node;
	}

	// cmp
	int cmp_func = strcmp(pos_node1->func, pos_node2->func);
	if (cmp_func != 0) {
		return cmp_func > 0 ? 1 : -1;
	}
	if (pos_node1->line == pos_node2->line) {
		return 0;
	}
	if (pos_node1->line > pos_node2->line)
		return 1;
	else
		return -1;
}

static int __mm_pos_walk (struct aosl_rb_node *node, void *arg)
{
	uint32_t *index = (uint32_t *)arg;
	struct mm_pos_node *pos_node = aosl_rb_entry(node, struct mm_pos_node, node);
#if 1 // ignore some no care
	MM_IGNORE_NODE_CHECK(pos_node);
#endif
	aosl_log(AOSL_LOG_CRIT, "[%3u]pos_node: cnts=%4d size=%7u %s:%d\n", (*index)++,
					 pos_node->cnts, (uint32_t)pos_node->size, pos_node->func, pos_node->line);
	return 0;
}

static int __mm_pos_walk_r (struct aosl_rb_node *node, void *arg)
{
	int ret = 0;
	struct mm_walk_arg *mm_arg = (struct mm_walk_arg *)arg;
	if (mm_arg->len < 1) {
		return 1;
	}
	struct mm_pos_node *pos_node = aosl_rb_entry(node, struct mm_pos_node, node);
#if 1 // ignore some no care
	MM_IGNORE_NODE_CHECK(pos_node);
#endif
	ret = snprintf(mm_arg->buf, mm_arg->len, "\"pos_node: cnts=%4d size=%7u %s:%d\",\n",
					 			 pos_node->cnts, (uint32_t)pos_node->size, pos_node->func, pos_node->line);
	if (ret >= mm_arg->len || ret <= 0) {
		mm_arg->buf[0] = '\0';
		return 1;
	}
	mm_arg->len -= ret;
	mm_arg->buf += ret;
	mm_arg->index++;
	return 0;
}

static struct aosl_rb_root __mm_pos_tree = {NULL, __mm_pos_cmp, 0};
#endif

static struct aosl_rb_root __mm_ptr_tree = {NULL, __mm_ptr_cmp, 0};
static k_lock_t __lock = {0};

static int __mem_check_inited = 0;
static int __mem_check_enable = 1;
static size_t __mem_used = 0;

#ifdef CONFIG_AOSL_MEM_DUMP
static void mm_insert_ptr_dbg (void *ptr, size_t size, const char *func, int line)
#else
static void mm_insert_ptr (void *ptr, size_t size)
#endif
{
	if (!__mem_check_inited || !__mem_check_enable) {
		return;
	}
	if (!ptr) {
		return;
	}
	struct mm_ptr_node *ptr_node = (struct mm_ptr_node *)MALLOC (sizeof(struct mm_ptr_node));
	ptr_node->ptr = ptr;
	ptr_node->size = size;
#ifdef CONFIG_AOSL_MEM_DUMP
	ptr_node->func = func;
	ptr_node->line = line;
#endif

	k_lock_lock (&__lock);
	__mem_used += size + sizeof(struct mm_ptr_node);
	// insert ptr node
	aosl_rb_insert_node (&__mm_ptr_tree, &ptr_node->node);

	// insert or update pos node
#ifdef CONFIG_AOSL_MEM_DUMP
	if (func != NULL && line != 0) {
		struct mm_pos_node *pos_node = NULL;
		struct aosl_rb_node *rb_node = aosl_find_rb_node(&__mm_pos_tree, NULL, func, line);
		if (rb_node) {
			pos_node = aosl_rb_entry(rb_node, struct mm_pos_node, node);
		} else {
			pos_node = (struct mm_pos_node *)MALLOC (sizeof(struct mm_pos_node));
			pos_node->func = func;
			pos_node->line = line;
			pos_node->cnts = 0;
			pos_node->size = 0;
			aosl_rb_insert_node(&__mm_pos_tree, &pos_node->node);
			__mem_used += sizeof(struct mm_pos_node);
		}
		pos_node->cnts++;
		pos_node->size += size;
	}
#endif

	k_lock_unlock (&__lock);
}

#ifdef CONFIG_AOSL_MEM_DUMP
static void mm_insert_ptr (void *ptr, size_t size)
{
	mm_insert_ptr_dbg(ptr, size, NULL, 0);
}
#endif

static void *mm_remove_ptr (void *ptr)
{
	if (!__mem_check_inited || !__mem_check_enable) {
		return ptr;
	}

	if (!ptr) {
		return ptr;
	}

	struct aosl_rb_node *rb_node = NULL;
	struct mm_ptr_node *ptr_node = NULL;
#ifdef CONFIG_AOSL_MEM_DUMP
	struct mm_pos_node *pos_node = NULL;
#endif

	k_lock_lock (&__lock);

	// get ptr node
	rb_node = aosl_rb_remove (&__mm_ptr_tree, NULL, ptr);
	if (rb_node != NULL) {
		ptr_node = aosl_rb_entry (rb_node, struct mm_ptr_node, node);
		__mem_used -= ptr_node->size + sizeof(struct mm_ptr_node);
	}

	// get pos node
#ifdef CONFIG_AOSL_MEM_DUMP
if (ptr_node && ptr_node->func && ptr_node->line) {
	rb_node = aosl_find_rb_node(&__mm_pos_tree, NULL, ptr_node->func, ptr_node->line);
	if (rb_node != NULL) {
		pos_node = aosl_rb_entry(rb_node, struct mm_pos_node, node);
		pos_node->cnts--;
		pos_node->size -= ptr_node->size;
		if (pos_node->cnts == 0) {
			aosl_rb_remove(&__mm_pos_tree, rb_node);
			__mem_used -= sizeof(struct mm_pos_node);
		}
	}
}
#endif

	// free ptr node
	if (ptr_node != NULL) {
		FREE (ptr_node);
	}

	// free pos node
#ifdef CONFIG_AOSL_MEM_DUMP
	if (pos_node != NULL && pos_node->cnts == 0) {
		FREE (pos_node);
	}
#endif

	k_lock_unlock (&__lock);

	return ptr;
}
#endif

__export_in_so__ size_t aosl_memused(void)
{
#ifdef CONFIG_AOSL_MEM_STAT
	return __mem_used;
#else
	return 0;
#endif
}

__export_in_so__ void aosl_memdump(void)
{
#ifdef CONFIG_AOSL_MEM_DUMP
	uint32_t index = 0;

	if (!__mem_check_inited || !__mem_check_enable) {
		return;
	}

	k_lock_lock (&__lock);

#if 0 // dump ptr node info
	aosl_log(AOSL_LOG_CRIT, "=== START used mem ptr node dump. total cnts=%u size=%u ===\n",
						(uint32_t)__mm_ptr_tree.count, (uint32_t)__mem_used);
	aosl_rb_traverse_ldr(&__mm_ptr_tree, __mm_ptr_walk, &index);
	aosl_log(AOSL_LOG_CRIT, "=== END   used mem ptr node dump. total cnts=%u size=%u ===\n",
						(uint32_t)__mm_ptr_tree.count, (uint32_t)__mem_used);
#endif

#if 1 // dump pos node info
	aosl_log(AOSL_LOG_CRIT, "=== START used mem pos node dump. total cnts=%u size=%u ===\n",
					 (uint32_t)__mm_pos_tree.count, (uint32_t)__mem_used);
	aosl_rb_traverse_ldr(&__mm_pos_tree, __mm_pos_walk, &index);
	aosl_log(AOSL_LOG_CRIT, "=== END   used mem pos node dump. total cnts=%u size=%u ===\n",
					 (uint32_t)__mm_pos_tree.count, (uint32_t)__mem_used);
#endif

	k_lock_unlock (&__lock);
#else
	return;
#endif
}

__export_in_so__ int  aosl_memdump_r(int cnts[2], char *buf, int len)
{
#ifdef CONFIG_AOSL_MEM_DUMP
	if (!__mem_check_inited || !__mem_check_enable) {
		return -1;
	}

	if (len <= 0) {
		return -1;
	}
	struct mm_walk_arg arg = {0};
	int vlen = 0; // valid printed len(exclude null)
	arg.buf = buf;
	arg.len = len;
	if (__mem_check_inited) {
		k_lock_lock (&__lock);
	}
	aosl_rb_traverse_ldr(&__mm_pos_tree, __mm_pos_walk_r, &arg);
	cnts[0] = __mm_pos_tree.count;  // total
	cnts[1] = arg.index;            // walk
	if (__mem_check_inited) {
		k_lock_unlock (&__lock);
	}

	// -1: for \0
	// -1: for \n
	if (arg.len < len) {
		vlen = len - arg.len - 1 - 1;
		if (buf[vlen] == ',') {
			buf[vlen] = '\0';
		}
	}
	buf[vlen+1] = '\0';
	return 0;
#else
	UNUSED (cnts);
	UNUSED (buf);
	UNUSED (len);
	return -1;
#endif
}

void k_mm_init (void)
{
#ifdef CONFIG_AOSL_MEM_STAT
	k_lock_init (&__lock);
	__mem_used = 0;
	__mem_check_inited = 1;
#endif
}

void k_mm_fini (void)
{
#ifdef CONFIG_AOSL_MEM_STAT
	aosl_memdump();

	k_lock_lock (&__lock);
	aosl_rb_clear(&__mm_ptr_tree, struct mm_ptr_node, node, FREE);
#ifdef CONFIG_AOSL_MEM_DUMP
	aosl_rb_clear(&__mm_pos_tree, struct mm_pos_node, node, FREE);
#endif
	__mem_used = 0;
	__mem_check_inited = 0;
	k_lock_unlock (&__lock);
	k_lock_destroy (&__lock);
#endif
}

__export_in_so__ void *aosl_malloc_impl (size_t size)
{
	void *p = MALLOC (size);

	if (!AOSL_IS_ALIGNED_PTR (p))
		abort ();

	if (!p) {
		aosl_log(AOSL_LOG_EMERG, "aosl_malloc %d byte failed\n", (int)size);
		abort();
	}

#ifdef CONFIG_AOSL_MEM_STAT
	mm_insert_ptr (p, size);
#endif

	return p;
}

__export_in_so__ void aosl_free_impl (void *ptr)
{
#ifdef CONFIG_AOSL_MEM_STAT
	mm_remove_ptr (ptr);
#endif

	FREE (ptr);
}

__export_in_so__ void *aosl_calloc_impl (size_t nmemb, size_t size)
{
	size_t __size;
	void *__ptr;

	__size = nmemb * size;
	__ptr = aosl_malloc_impl (__size);
	if (__ptr != NULL)
		memset (__ptr, 0, __size);

	return __ptr;
}

__export_in_so__ void *aosl_realloc_impl (void *ptr, size_t size)
{
	void *new_ptr = REALLOC (ptr, size);
#ifdef CONFIG_AOSL_MEM_STAT
	if (new_ptr) {
		mm_remove_ptr (ptr);
		mm_insert_ptr (new_ptr, size);
	}
#endif

	return new_ptr;
}

__export_in_so__ char *aosl_strdup_impl (const char *s)
{
	if (s != NULL) {
		size_t len = strlen (s);
		char *new = (char *)aosl_malloc_impl (len + 1);
		if (new != NULL) {
			memcpy (new, s, len);
			new [len] = '\0';
		}

		return new;
	}

	return NULL;
}

#ifdef CONFIG_AOSL_MEM_DUMP
__export_in_so__ void *aosl_malloc_impl_dbg (size_t size, const char *func, int line)
{
	void *p = MALLOC (size);

	if (!AOSL_IS_ALIGNED_PTR (p))
		abort ();

	if (!p) {
		aosl_log(AOSL_LOG_EMERG, "aosl_malloc %d byte failed\n", (int)size);
		abort();
	}

	mm_insert_ptr_dbg (p, size, func, line);
	return p;
}

__export_in_so__ void *aosl_calloc_impl_dbg (size_t nmemb, size_t size, const char *func, int line)
{
	size_t __size;
	void *__ptr;

	__size = nmemb * size;
	__ptr = aosl_malloc_impl_dbg (__size, func, line);
	if (__ptr != NULL)
		memset (__ptr, 0, __size);

	return __ptr;
}

__export_in_so__ void *aosl_realloc_impl_dbg (void *ptr, size_t size, const char *func, int line)
{
	void *new_ptr = REALLOC (ptr, size);
	if (new_ptr) {
		mm_remove_ptr (ptr);
		mm_insert_ptr_dbg (new_ptr, size, func, line);
	}

	return new_ptr;
}

__export_in_so__ char *aosl_strdup_impl_dbg (const char *s, const char *func, int line)

{
	if (s != NULL) {
		size_t len = strlen (s);
		char *new = (char *)aosl_malloc_impl_dbg (len + 1, func, line);
		if (new != NULL) {
			memcpy (new, s, len);
			new [len] = '\0';
		}

		return new;
	}

	return NULL;
}
#endif // end CONFIG_AOSL_MEM_DUMP