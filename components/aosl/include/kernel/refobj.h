/***************************************************************************
 * Module:	AOSL reference object internal definition file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_REFOBJ_H__
#define __AOSL_REFOBJ_H__


#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_ref.h>
#include <kernel/atomic.h>
#include <kernel/rbtree.h>
#include <kernel/thread.h>


#ifdef __cplusplus
extern "C" {
#endif



struct refobj;

typedef int (*refobj_ctor_t) (struct refobj *robj, void *arg, void (*dtor) (void *arg), int modify_async, int rdlock_recursive, int caller_free, va_list args);
typedef void (*refobj_dtor_t) (struct refobj *robj);

struct refobj_type {
	size_t obj_size;
	refobj_ctor_t ctor;
	refobj_dtor_t dtor;
};

extern struct refobj_type refobj_type_obj;

struct refobj {
	const struct refobj_type *type;
	void *arg;
	aosl_ref_dtor_t dtor;
	aosl_ref_t obj_id;
	atomic_t usage;
	k_rwlock_t stop_lock;
	/**
	 * Employ a dedicated indicator to tell the world this task object
	 * has been destroyed, rather than using the 'usage' count, because
	 * we could not determine whether this task object has already been
	 * destroyed if some pending async operations on the way when the
	 * usage count is greater than 1.
	 **/
#define REFOBJ_DESTROYED 0x80000000
#define REFOBJ_MODIFY_ASYNC 0x40000000
#define REFOBJ_RDLOCK_RECURSIVE 0x20000000
#define REFOBJ_CALLER_FREE 0x10000000
	uint32_t flags;
	k_rwlock_t thread_nodes_lock;
	struct aosl_rb_root thread_nodes;
};

#define refobj_is_destroyed(robj) (((robj)->flags & REFOBJ_DESTROYED) != 0)
#define refobj_set_destroyed(robj) ((robj)->flags |= REFOBJ_DESTROYED)
#define refobj_is_modify_async(robj) (((robj)->flags & REFOBJ_MODIFY_ASYNC) != 0)
#define refobj_set_modify_async(robj) ((robj)->flags |= REFOBJ_MODIFY_ASYNC)
#define refobj_is_rdlock_recursive(robj) (((robj)->flags & REFOBJ_RDLOCK_RECURSIVE) != 0)
#define refobj_set_rdlock_recursive(robj) ((robj)->flags |= REFOBJ_RDLOCK_RECURSIVE)
#define refobj_is_caller_free(robj) (((robj)->flags & REFOBJ_CALLER_FREE) != 0)
#define refobj_set_caller_free(robj) ((robj)->flags |= REFOBJ_CALLER_FREE)

extern struct refobj *refobj_create (const struct refobj_type *type, void *arg, aosl_ref_dtor_t dtor, int modify_async, int rdlock_recursive, int caller_free, ...);

extern struct refobj *refobj_get (aosl_ref_t ref);
extern void refobj_put (struct refobj *robj);

extern int refobj_rdlock (struct refobj *robj);
extern void refobj_rdunlock (struct refobj *robj);

static __inline__ void __refobj_rdlock_raw (struct refobj *robj)
{
	k_rwlock_rdlock (&robj->stop_lock);
}

static __inline__ void __refobj_rdunlock_raw (struct refobj *robj)
{
	k_rwlock_rdunlock (&robj->stop_lock);
}

static __inline__ void __refobj_wrlock_raw (struct refobj *robj)
{
	k_rwlock_wrlock (&robj->stop_lock);
}

static __inline__ void __refobj_wrunlock_raw (struct refobj *robj)
{
	k_rwlock_wrunlock (&robj->stop_lock);
}

static __inline__ void __refobj_rd2wrlock_raw (struct refobj *robj)
{
	k_rwlock_rd2wrlock (&robj->stop_lock);
}

static __inline__ void __refobj_wr2rdlock_raw (struct refobj *robj)
{
	k_rwlock_wr2rdlock (&robj->stop_lock);
}


#ifdef __cplusplus
}
#endif



#endif /* __AOSL_REFOBJ_H__ */