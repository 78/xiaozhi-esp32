/***************************************************************************
 * Module:	AOSL reference object implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <stdlib.h>

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_alloca.h>
#include <api/aosl_mm.h>
#include <api/aosl_time.h>
#include <api/aosl_mpq.h>
#include <api/aosl_ref.h>
#include <kernel/refobj.h>

#include <kernel/thread.h>
#include <kernel/mp_queue.h>
#include <kernel/bitmap.h>
#include <kernel/err.h>
#include <api/aosl_integer_wrappings.h>

#define UNUSED(expr) (void)(expr)

#define STATIC_REFOBJ_ID_POOL_SIZE 8

static k_rwlock_t refobj_table_lock;
static struct refobj **refobj_table = NULL;
static bitmap_t *refobj_id_pool_bits = NULL;
static int refobj_table_size = 0;

/* 0 is the only invalid life id, so init it to 1 */
static uint16_t __refobj_life_id = 1;

void k_refobj_init (void)
{
	int i;
	refobj_id_pool_bits = bitmap_create (STATIC_REFOBJ_ID_POOL_SIZE);
	refobj_table = (struct refobj **)aosl_malloc (sizeof (struct refobj *) * STATIC_REFOBJ_ID_POOL_SIZE);
	if (!refobj_table || !refobj_id_pool_bits) {
		abort ();
	}
	refobj_table_size = STATIC_REFOBJ_ID_POOL_SIZE;
	for (i = 0; i < STATIC_REFOBJ_ID_POOL_SIZE; i++) {
		refobj_table [i] = NULL;
	}

	k_rwlock_init (&refobj_table_lock);
}

void k_refobj_fini(void)
{
	if (refobj_id_pool_bits) {
		bitmap_destroy (refobj_id_pool_bits);
		refobj_id_pool_bits = NULL;
	}
	if (refobj_table) {
		for (int i = 0; i < refobj_table_size; i++) {
			if (refobj_table [i] != NULL) {
				abort ();
			}
		}
		aosl_free (refobj_table);
		refobj_table = NULL;
		refobj_table_size = 0;
	}

	k_rwlock_destroy (&refobj_table_lock);
}

/* The max simultaneous ref objects count we supported */
#define REFOBJ_ID_POOL_MAX_SIZE 20480
#define MIN_REFOBJ_ID 0

static int get_unused_refobj_id (void)
{
	int ref_id;

	k_rwlock_wrlock (&refobj_table_lock);
	ref_id = bitmap_find_first_zero_bit (refobj_id_pool_bits);
	if (ref_id < 0) {
		int new_table_size;
		bitmap_t *new_bits;
		struct refobj **new_table;

		if (refobj_table_size >= REFOBJ_ID_POOL_MAX_SIZE) {
			k_rwlock_wrunlock (&refobj_table_lock);
			return -AOSL_EOVERFLOW;
		}

		new_table_size = refobj_table_size + 8;

		new_bits = bitmap_create (new_table_size);
		if (!new_bits) {
			k_rwlock_wrunlock (&refobj_table_lock);
			return -AOSL_ENOMEM;
		}

		new_table = (struct refobj **)aosl_malloc (sizeof (struct refobj *) * new_table_size);
		if (!new_table) {
			k_rwlock_wrunlock (&refobj_table_lock);
			bitmap_destroy (new_bits);
			return -AOSL_ENOMEM;
		}

		bitmap_copy (new_bits, refobj_id_pool_bits);
		memcpy (new_table, refobj_table, sizeof (struct refobj *) * refobj_table_size);
		memset (new_table + refobj_table_size, 0, (new_table_size - refobj_table_size) * sizeof (struct refobj *));

		bitmap_destroy (refobj_id_pool_bits);
		aosl_free (refobj_table);

		refobj_id_pool_bits = new_bits;
		refobj_table = new_table;
		refobj_table_size = new_table_size;

		ref_id = bitmap_find_first_zero_bit (refobj_id_pool_bits);
		BUG_ON (ref_id < 0);
	}

	bitmap_set (refobj_id_pool_bits, ref_id);
	k_rwlock_wrunlock (&refobj_table_lock);

	return ref_id + MIN_REFOBJ_ID;
}

static void __put_unused_refobj_id (int ref_id)
{
	BUG_ON (ref_id < 0 || ref_id >= refobj_table_size);
	k_rwlock_wrlock (&refobj_table_lock);
	bitmap_clear (refobj_id_pool_bits, ref_id);
	k_rwlock_wrunlock (&refobj_table_lock);
}

static __inline__ aosl_ref_t make_ref_obj_id (int16_t ref_id, uint16_t life_id)
{
	return (aosl_ref_t)(((uint32_t)life_id << 16) | ref_id);
}

static __inline__ int16_t get_ref_id (aosl_ref_t ref_obj_id)
{
	return (int16_t)ref_obj_id;
}

static __inline__ uint16_t get_ref_life_id (aosl_ref_t ref_obj_id)
{
	return (uint16_t)((uint32_t)ref_obj_id >> 16);
}

static void __refobj_id_install (int ref_id, struct refobj *obj)
{
	BUG_ON (ref_id < MIN_REFOBJ_ID);
	BUG_ON (ref_id >= refobj_table_size + MIN_REFOBJ_ID);

	k_rwlock_wrlock (&refobj_table_lock);
	if (ref_id - MIN_REFOBJ_ID < refobj_table_size) {
		if (refobj_table [ref_id - MIN_REFOBJ_ID] != NULL)
			abort ();

		refobj_table [ref_id - MIN_REFOBJ_ID] = obj;
		obj->obj_id = make_ref_obj_id (ref_id, __refobj_life_id);
		__refobj_life_id++;

		/**
		 * 0 is the only invalid life id, so reset it to
		 * 1 if we the life id counter wrapped back.
		 **/
		if (__refobj_life_id == 0)
			__refobj_life_id = 1;
	}
	k_rwlock_wrunlock (&refobj_table_lock);
}

static int __refobj_id_uninstall (int ref_id, struct refobj *obj)
{
	int err;

	BUG_ON (ref_id < MIN_REFOBJ_ID);
	ref_id -= MIN_REFOBJ_ID;
	BUG_ON (ref_id >= refobj_table_size);

	k_rwlock_wrlock (&refobj_table_lock);
	if (refobj_table [ref_id] == obj) {
		refobj_table [ref_id] = NULL;
		err = 0;
	} else {
		err = -AOSL_EINVAL;
	}
	k_rwlock_wrunlock (&refobj_table_lock);
	return err;
}

struct robj_thread_node {
	struct aosl_rb_node rb_node;
	k_thread_t thread_id;
	uint32_t usage;
	uint32_t get_count;
	int32_t lock_count;
	aosl_ts_t active_ts;
};

#define thread_lock_count_init(t) (t)->lock_count = 0
#define thread_rdlock_count(t) ((uint32_t)((t)->lock_count & 0x7fffffff))
#define thread_wrlocked(t) (((t)->lock_count & 0x80000000) != 0)
#define thread_lock_free(t) ((t)->lock_count == 0)
#define thread_inc_rdlock_count(t) (t)->lock_count++
#define thread_dec_rdlock_count(t) (t)->lock_count--
#define thread_set_wrlocked(t) (t)->lock_count = 0x80000000
#define thread_clr_wrlocked(t) (t)->lock_count = 0

#define THREAD_MAX_IDLE_TIME 60000 /* 1 minute */
#define THREAD_CLEANUP_THRESHOLD 64

static __inline__ void __thread_node_get (struct robj_thread_node *thread_node)
{
	thread_node->usage++;
}

static __inline__ void __thread_node_put (struct robj_thread_node *thread_node)
{
	thread_node->usage--;
	if (thread_node->usage == 0)
		aosl_free ((void *)thread_node);
}

static int __check_erase_node (struct aosl_rb_node *node, void *arg)
{
	struct robj_thread_node *thread_node = aosl_rb_entry (node, struct robj_thread_node, rb_node);
	struct refobj *robj = (struct refobj *)arg;
	aosl_ts_t now = aosl_tick_now ();

	if (thread_node->usage == 1 && thread_node->get_count == 0 && thread_lock_free (thread_node)
								&& (int)(now - thread_node->active_ts) >= THREAD_MAX_IDLE_TIME) {
		aosl_rb_erase (&robj->thread_nodes, node);
		__thread_node_put (thread_node);
	}

	return 0;
}

static void refobj_lock_threads_cleanup (struct refobj *robj)
{
	if (robj->thread_nodes.count > THREAD_CLEANUP_THRESHOLD) {
		/**
		 * Using the LRD traversing order to do the cleanup.
		 **/
		k_rwlock_wrlock (&robj->thread_nodes_lock);
		aosl_rb_traverse_lrd (&robj->thread_nodes, __check_erase_node, (void *)robj);
		k_rwlock_wrunlock (&robj->thread_nodes_lock);
	}
}

static int cmp_thread (struct aosl_rb_node *rb_node, struct aosl_rb_node *node, va_list args)
{
	struct robj_thread_node *rb_entry = aosl_rb_entry (rb_node, struct robj_thread_node, rb_node);
	k_thread_t thread_id;

	if (node != NULL) {
		thread_id = aosl_rb_entry (node, struct robj_thread_node, rb_node)->thread_id;
	} else {
		thread_id = va_arg (args, k_thread_t);
	}

	if (rb_entry->thread_id > thread_id)
		return 1;

	if (rb_entry->thread_id < thread_id)
		return -1;

	return 0;
}

static struct robj_thread_node *robj_this_thread_node_get (struct refobj *robj, int create)
{
	struct aosl_rb_node *node;
	struct robj_thread_node *thread_node;
	k_thread_t this_thread;

	this_thread = k_thread_self ();
	k_rwlock_rdlock (&robj->thread_nodes_lock);
	node = aosl_find_rb_node (&robj->thread_nodes, NULL, this_thread);
	if (node != NULL) {
		thread_node = aosl_rb_entry (node, struct robj_thread_node, rb_node);
		__thread_node_get (thread_node); /* Increase the usage inside the read lock */
	} else {
		thread_node = NULL;
	}
	k_rwlock_rdunlock (&robj->thread_nodes_lock);

	if (thread_node == NULL && create) {
		thread_node = (struct robj_thread_node *)aosl_malloc (sizeof *thread_node);
		if (thread_node == NULL)
			abort ();

		thread_node->thread_id = this_thread;
		thread_node->usage = 1;
		thread_node->get_count = 0;
		thread_lock_count_init (thread_node);
		__thread_node_get (thread_node); /* Increase the usage before inserted to the tree */

		k_rwlock_wrlock (&robj->thread_nodes_lock);
		/**
		 * No find the thread node again when we get the write lock,
		 * because the thread node can only be created by the thread
		 * itself, so no racing condition after we released the read
		 * lock and before the write lock.
		 **/
		aosl_rb_insert_node (&robj->thread_nodes, &thread_node->rb_node);
		k_rwlock_wrunlock (&robj->thread_nodes_lock);
	}

	if (thread_node != NULL)
		thread_node->active_ts = aosl_tick_now ();

	return thread_node;
}

static void robj_this_thread_node_put (struct robj_thread_node *thread_node)
{
	__thread_node_put (thread_node);
}

static void refobj_thread_rdlock (struct refobj *robj)
{
	struct robj_thread_node *thread_node;
	uint32_t count;

	thread_node = robj_this_thread_node_get (robj, 1/* create */);
	if (thread_wrlocked (thread_node)) {
		/**
		 * Abort it for finding the potential deadlock bug ASAP.
		 **/
		abort ();
	}

	if (!refobj_is_rdlock_recursive (robj) && thread_rdlock_count (thread_node) > 0) {
		/**
		 * Abort it for finding the potential bug ASAP.
		 **/
		abort ();
	}

	thread_inc_rdlock_count (thread_node);
	count = thread_rdlock_count (thread_node);
	robj_this_thread_node_put (thread_node);
	if (count == 1)
		__refobj_rdlock_raw (robj);
}

static void refobj_thread_rdunlock (struct refobj *robj)
{
	struct robj_thread_node *thread_node;
	uint32_t count;

	thread_node = robj_this_thread_node_get (robj, 0/* !create */);
	if (thread_node == NULL)
		abort ();

	thread_dec_rdlock_count (thread_node);
	/**
	 * The thread_node might be freed by the following cleanup,
	 * so retrieve and save the count first.
	 **/
	count = thread_rdlock_count (thread_node);
	robj_this_thread_node_put (thread_node);
	if (count == 0)
		__refobj_rdunlock_raw (robj);

	/**
	 * This is the chance to do the threads cleanup.
	 **/
	refobj_lock_threads_cleanup (robj);
}

static void refobj_thread_wrlock (struct refobj *robj, int rdlocked)
{
	struct robj_thread_node *thread_node;

	thread_node = robj_this_thread_node_get (robj, 1/* create */);
	if (!thread_lock_free (thread_node)) {
		/**
		 * Abort it for finding the potential deadlock bug ASAP.
		 **/
		abort ();
	}

	thread_set_wrlocked (thread_node);
	robj_this_thread_node_put (thread_node);
	if (rdlocked) {
		__refobj_rd2wrlock_raw (robj);
	} else {
		__refobj_wrlock_raw (robj);
	}
}

static void refobj_thread_wrunlock (struct refobj *robj, int rdlocked)
{
	struct robj_thread_node *thread_node;

	thread_node = robj_this_thread_node_get (robj, 0/* !create */);
	if (thread_node == NULL)
		abort ();

	thread_clr_wrlocked (thread_node);
	robj_this_thread_node_put (thread_node);
	if (rdlocked) {
		__refobj_wr2rdlock_raw (robj);
	} else {
		__refobj_wrunlock_raw (robj);
	}

	/**
	 * This is the chance to do the threads cleanup.
	 **/
	refobj_lock_threads_cleanup (robj);
}

static int refobj_ctor (struct refobj *robj, void *arg, aosl_ref_dtor_t dtor, int modify_async, int rdlock_recursive, int caller_free, va_list args)
{
	UNUSED (args);
	robj->arg = arg;
	robj->dtor = dtor;
	atomic_set (&robj->usage, 1);
	k_rwlock_init (&robj->stop_lock);
	robj->flags = 0;
	if (modify_async)
		refobj_set_modify_async (robj);

	if (rdlock_recursive)
		refobj_set_rdlock_recursive (robj);

	if (caller_free)
		refobj_set_caller_free (robj);

	k_rwlock_init (&robj->thread_nodes_lock);
	aosl_rb_root_init (&robj->thread_nodes, cmp_thread);
	return 0;
}

static void refobj_dtor (struct refobj *robj)
{
	k_rwlock_destroy (&robj->stop_lock);
	k_rwlock_destroy (&robj->thread_nodes_lock);
	while (robj->thread_nodes.rb_node != NULL) {
		struct aosl_rb_node *rb_node = robj->thread_nodes.rb_node;
		struct robj_thread_node *thread_node = aosl_rb_entry (rb_node, struct robj_thread_node, rb_node);

		aosl_rb_erase (&robj->thread_nodes, rb_node);
		__thread_node_put (thread_node);
	}
}

struct refobj_type refobj_type_obj = {
	.obj_size = sizeof (struct refobj),
	.ctor = refobj_ctor,
	.dtor = refobj_dtor,
};

struct refobj *refobj_create (const struct refobj_type *type, void *arg, aosl_ref_dtor_t dtor, int modify_async, int rdlock_recursive, int caller_free, ...)
{
	int err;
	struct refobj *robj;

	if (type->obj_size < sizeof (struct refobj))
		return ERR_PTR (-AOSL_EINVAL);

	err = -AOSL_ENOMEM;
	robj = (struct refobj *)aosl_malloc (type->obj_size);
	if (robj != NULL) {
		va_list args;

		robj->type = type;
		va_start (args, caller_free);
		err = type->ctor (robj, arg, dtor, modify_async, rdlock_recursive, caller_free, args);
		va_end (args);

		if (err < 0)
			goto __err;

		err = get_unused_refobj_id ();
		if (err < 0) {
			type->dtor (robj);
			goto __err;
		}

		__refobj_id_install (err, robj);
		return robj;
	}

__err:
	if (robj != NULL)
		aosl_free (robj);

	return ERR_PTR (err);
}

static void refobj_free (struct refobj *robj)
{
	int ref_id = get_ref_id (robj->obj_id);

	/* Call the upper layer dtor first */
	if (robj->dtor != NULL)
		robj->dtor (robj->arg);

	/**
	 * Call the refobj type dtor, and it is the responsibility
	 * of destructor of the object type to call the destructor
	 * of its' base class.
	 **/
	if (robj->type->dtor != NULL)
		robj->type->dtor (robj);

	/**
	 * We defer the freeing of refobj id to this point
	 * just for catching the potential logic errors
	 * that this refobj id being allocated by another
	 * refobj.
	 * Free it just before we free the refobj itself
	 * should be better for these cases.
	 **/
	__put_unused_refobj_id (ref_id - MIN_REFOBJ_ID);
	aosl_free (robj);
}

static struct refobj *__refobj_get (aosl_ref_t ref_obj_id, int inc_get_count)
{
	int16_t ref_id = get_ref_id (ref_obj_id);
	struct refobj *obj;

	if (ref_id < MIN_REFOBJ_ID)
		return NULL;

	ref_id -= MIN_REFOBJ_ID;

	k_rwlock_rdlock (&refobj_table_lock);
	if (ref_id < refobj_table_size) {
		obj = refobj_table [ref_id];
		if (obj != NULL) {
			if (obj->obj_id == ref_obj_id) {
				atomic_inc (&obj->usage);
			} else {
				obj = NULL;
			}
		}
	} else {
		obj = NULL;
	}
	k_rwlock_rdunlock (&refobj_table_lock);

	if (obj != NULL && inc_get_count && refobj_is_caller_free (obj)) {
		struct robj_thread_node *thread_node = robj_this_thread_node_get (obj, 1/* create */);
		thread_node->get_count++;
		robj_this_thread_node_put (thread_node);
	}

	return obj;
}

static __inline__ void __refobj_put (struct refobj *robj)
{
	if (atomic_dec_and_test (&robj->usage))
		refobj_free (robj);
}

struct refobj *refobj_get (aosl_ref_t ref_obj_id)
{
	return __refobj_get (ref_obj_id, 1/* inc_get_count */);
}

void refobj_put (struct refobj *robj)
{
	if (refobj_is_caller_free (robj)) {
		struct robj_thread_node *thread_node = robj_this_thread_node_get (robj, 0/* !create */);
		if (thread_node == NULL)
			abort ();

		thread_node->get_count--;
		robj_this_thread_node_put (thread_node);
	}

	__refobj_put (robj);
}

int refobj_rdlock (struct refobj *robj)
{
	if (refobj_on_this_q ((aosl_refobj_t)robj) && !refobj_is_modify_async (robj)) {
		/**
		 * If the ref object specified by robj is the current running ref object
		 * and it is not modify async type, then no lock needed, because we have
		 * already held the read lock.
		 **/
		goto __nolock_needed;
	}

	/**
	 * The read lock operation on mpq uses __refobj_rdlock_raw directly, so the previous
	 * 'on this q' checking guarantees that we will not read lock more than once.
	 **/
	refobj_thread_rdlock (robj);
	if (refobj_is_destroyed (robj)) {
		refobj_thread_rdunlock (robj);
		return -AOSL_EPERM;
	}

__nolock_needed:
	return 0;
}

void refobj_rdunlock (struct refobj *robj)
{
	if (refobj_on_this_q ((aosl_refobj_t)robj) && !refobj_is_modify_async (robj)) {
		/**
		 * If the ref object specified by robj is the current running ref object
		 * and it is not modify async type, then do not unlock here, because we
		 * did not rdlock it in the previous pairing lock operation.
		 **/
		return;
	}

	/**
	 * The read unlock operation on mpq uses __refobj_rdunlock_raw directly, so the previous
	 * 'on this q' checking guarantees that we will not read unlock more than once.
	 **/
	refobj_thread_rdunlock (robj);
}

static int refobj_wrlock (struct refobj *robj, int rdlocked)
{
	refobj_thread_wrlock (robj, rdlocked);
	if (refobj_is_destroyed (robj)) {
		refobj_thread_wrunlock (robj, rdlocked);
		if (rdlocked) {
			/**
			 * This should not happen obviously because
			 * we hold the read lock now.
			 **/
			abort ();
		}

		return -AOSL_EPERM;
	}

	return 0;
}

static void refobj_wrunlock (struct refobj *robj, int rdlocked)
{
	refobj_thread_wrunlock (robj, rdlocked);
}

__export_in_so__ aosl_ref_t aosl_ref_create (void *arg, aosl_ref_dtor_t dtor, int modify_async, int recursive, int caller_free)
{
	struct refobj *robj;

	robj = refobj_create (&refobj_type_obj, arg, dtor, modify_async, recursive, caller_free);
	if (IS_ERR_OR_NULL (robj)) {
		aosl_errno = (int)-PTR_ERR (robj);
		return AOSL_REF_INVALID;
	}

	return robj->obj_id;
}

enum refobj_op_type {
	REFOBJ_NOLOCK_OP = 0,
	REFOBJ_RDLOCK_OP,
	REFOBJ_WRLOCK_OP
};

static int __refobj_op_argv (struct refobj *robj, enum refobj_op_type op, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv [])
{
	int err = 0;
	int rdlocked = 0;

	if (op == REFOBJ_WRLOCK_OP) {
		if (refobj_on_this_q ((aosl_refobj_t)robj) && !refobj_is_modify_async (robj)) {
			/**
			 * If the ref object specified by robj is the current running ref object
			 * and it is not modify async type, then trying to write lock it would
			 * lead to deadlock, so just abort it here to report bug.
			 **/
			rdlocked = 1;
		}

		err = refobj_wrlock (robj, rdlocked);
	} else if (op == REFOBJ_RDLOCK_OP) {
		err = refobj_rdlock (robj);
	}

	if (err < 0)
		return err;

	f (robj->arg, argc, argv);

	if (op == REFOBJ_WRLOCK_OP) {
		refobj_wrunlock (robj, rdlocked);
	} else if (op == REFOBJ_RDLOCK_OP) {
		refobj_rdunlock (robj);
	}

	return 0;
}

static int __ref_op_argv (aosl_ref_t ref, enum refobj_op_type op, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv [])
{
	struct refobj *robj;
	int err;

	robj = (struct refobj *)refobj_get (ref);
	if (robj == NULL)
		return -AOSL_EINVAL;

	err = __refobj_op_argv (robj, op, f, argc, argv);

	refobj_put (robj);
	return err;
}

static int __ref_op_args (aosl_ref_t ref, enum refobj_op_type op, aosl_ref_func_t f, uintptr_t argc, va_list args)
{
	uintptr_t *argv = NULL;

	if (argc > 0) {
		uintptr_t l;

		argv = aosl_alloca (sizeof (uintptr_t) * argc);
		for (l = 0; l < argc; l++)
			argv [l] = va_arg (args, uintptr_t);
	}

	return __ref_op_argv (ref, op, f, argc, argv);
}

static int __refobj_op_args (struct refobj *robj, enum refobj_op_type op, aosl_ref_func_t f, uintptr_t argc, va_list args)
{
	uintptr_t *argv = NULL;

	if (aosl_is_free_only (robj))
		return -AOSL_EINVAL;

	if (argc > 0) {
		uintptr_t l;

		argv = aosl_alloca (sizeof (uintptr_t) * argc);
		for (l = 0; l < argc; l++)
			argv [l] = va_arg (args, uintptr_t);
	}

	return __refobj_op_argv (robj, op, f, argc, argv);
}

__export_in_so__ int aosl_ref_hold (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __ref_op_args (ref, REFOBJ_NOLOCK_OP, f, argc, args);
	va_end (args);

	return_err (err);
}

__export_in_so__ int aosl_ref_hold_args (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, va_list args)
{
	return_err (__ref_op_args (ref, REFOBJ_NOLOCK_OP, f, argc, args));
}

__export_in_so__ int aosl_ref_hold_argv (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv [])
{
	return_err (__ref_op_argv (ref, REFOBJ_NOLOCK_OP, f, argc, argv));
}

__export_in_so__ int aosl_ref_read (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __ref_op_args (ref, REFOBJ_RDLOCK_OP, f, argc, args);
	va_end (args);

	return_err (err);
}

__export_in_so__ int aosl_ref_read_args (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, va_list args)
{
	return_err (__ref_op_args (ref, REFOBJ_RDLOCK_OP, f, argc, args));
}

__export_in_so__ int aosl_ref_read_argv (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv [])
{
	return_err (__ref_op_argv (ref, REFOBJ_RDLOCK_OP, f, argc, argv));
}

__export_in_so__ int aosl_ref_write (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __ref_op_args (ref, REFOBJ_WRLOCK_OP, f, argc, args);
	va_end (args);

	return_err (err);
}

__export_in_so__ int aosl_ref_write_args (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, va_list args)
{
	return_err (__ref_op_args (ref, REFOBJ_WRLOCK_OP, f, argc, args));
}

__export_in_so__ int aosl_ref_write_argv (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv [])
{
	return_err (__ref_op_argv (ref, REFOBJ_WRLOCK_OP, f, argc, argv));
}

__export_in_so__ int aosl_refobj_read (aosl_refobj_t robj, aosl_ref_func_t f, uintptr_t argc, ...)
{
	va_list args;
	int err;

	va_start (args, argc);
	err = __refobj_op_args ((struct refobj *)robj, REFOBJ_RDLOCK_OP, f, argc, args);
	va_end (args);

	return_err (err);
}

__export_in_so__ int aosl_refobj_read_args (aosl_refobj_t robj, aosl_ref_func_t f, uintptr_t argc, va_list args)
{
	return_err (__refobj_op_args ((struct refobj *)robj, REFOBJ_RDLOCK_OP, f, argc, args));
}

__export_in_so__ int aosl_refobj_read_argv (aosl_refobj_t robj, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv [])
{
	if (aosl_is_free_only (robj))
		return_err (-AOSL_EINVAL);

	return_err (__refobj_op_argv ((struct refobj *)robj, REFOBJ_RDLOCK_OP, f, argc, argv));
}

__export_in_so__ void *aosl_refobj_arg (aosl_refobj_t ro)
{
	struct refobj *robj = (struct refobj *)ro;
	return robj->arg;
}

__export_in_so__ aosl_ref_t aosl_refobj_id (aosl_refobj_t robj)
{
	struct refobj *ref_obj = (struct refobj *)robj;
	return ref_obj->obj_id;
}

__export_in_so__ int aosl_ref_destroy (aosl_ref_t ref, int do_delete)
{
	int err;
	struct refobj *robj = __refobj_get (ref, 0/* !inc_get_count */);
	struct robj_thread_node *thread_node;
	uint32_t robj_this_thread_get_count = 0;

	if (robj == NULL) {
		err = -AOSL_ENOENT;
		goto __err_out;
	}

	if (refobj_on_this_q ((aosl_refobj_t)robj) && !refobj_is_modify_async (robj)) {
		abort ();
		err = -AOSL_EDEADLK;
		goto __put_out;
	}

	thread_node = robj_this_thread_node_get (robj, 0/* !create */);
	if (thread_node != NULL) {
		if (thread_rdlock_count (thread_node) > 0) {
			/**
			 * This is also a dead lock case, the running thread holds
			 * the read lock now, and want to destroy the ref object,
			 * so just abort here to report the bug.
			 **/
			abort ();
			err = -AOSL_EDEADLK;
			goto __put_out;
		}

		robj_this_thread_get_count = thread_node->get_count;
		robj_this_thread_node_put (thread_node);
	}

	if (do_delete) {
		/* Only uninstall the ref object when do delete */
		err = __refobj_id_uninstall (get_ref_id (ref), robj);
		if (err < 0)
			goto __put_out;
	}

	/**
	 * Check the destroyed first without holding the lock,
	 * do not worry because we will check it again after
	 * we held the lock later if it is not destroyed, and
	 * nobody could change it back from a destroyed state.
	 **/
	if (!refobj_is_destroyed (robj)) {
		/**
		 * We will wait on the ref object lock if some other
		 * thread is just using the ref object, because the
		 * running mpq will hold the read lock when invoking
		 * the queued target function.
		 * So, we are safe enough to free all the objects
		 * after this function returned, if we put the
		 * freeing operations of all other relative objects
		 * those may be used in the target running function
		 * after this function.
		 **/
		__refobj_wrlock_raw (robj);
		err = refobj_is_destroyed (robj) ? -AOSL_EPERM : 0;
		refobj_set_destroyed (robj); /* mark destroyed */
		__refobj_wrunlock_raw (robj);
	} else {
		err = -AOSL_EPERM;
	}

	if (do_delete) {
		/**
		 * Only put and wait when specifying do delete option,
		 * otherwise, we just mark it was destroyed.
		 **/
		__refobj_put (robj);

		/**
		 * If we do this operation here, we can make sure
		 * all the objects would be freed in the calling
		 * thread. But we can only do this if this object
		 * is not modify_async type.
		 **/
		if (refobj_is_caller_free (robj) && !refobj_is_modify_async (robj)) {
			/**
			 * The calling thread might holding the robj
			 * when invoking the destroy func, so please
			 * be careful enough to avoid dead loop here.
			 **/
			while (atomic_read (&robj->usage) > 1 + (int)robj_this_thread_get_count)
				aosl_msleep (1);
		}

		/** 
		 * Do not return -AOSL_EPERM error even the ref object
		 * has been destroyed already when do_delete is
		 * true, because we have a checking in the ref id
		 * table.
		 **/
		err = 0;
	}

__put_out:
	__refobj_put (robj); /* put due to the get operation */

__err_out:
	return_err (err);
}