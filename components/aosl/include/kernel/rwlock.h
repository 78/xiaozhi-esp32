/***************************************************************************
 * Module:	rwlock implementation
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __KERNEL_RWLOCK_H__
#define __KERNEL_RWLOCK_H__

#include <api/aosl_types.h>
#include <kernel/list.h>
#include <hal/aosl_hal_thread.h>

extern void rwlock_read_lock_failed (intptr_t *rw_count);
extern void rwlock_write_lock_failed (intptr_t *rw_count);
extern void rwlock_wake (intptr_t *rw_count);
extern void rwlock_downgrade_wake (intptr_t *rw_count);

typedef struct {
	intptr_t count;
	aosl_mutex_t wait_lock;
	struct aosl_list_head wait_list;
} k_raw_rwlock_t;


static inline int rwlock_is_locked (k_raw_rwlock_t *rw)
{
	return rw->count != 0;
}

#include <kernel/rwlock-generic.h>

extern void rwlock_init (k_raw_rwlock_t *rw);

static __inline__ void rwlock_rdlock (k_raw_rwlock_t *rw)
{
	__rwlock_rdlock (rw);
}

static __inline__ int rwlock_tryrdlock (k_raw_rwlock_t *rw)
{
	return __rwlock_tryrdlock (rw);
}

static __inline__ void rwlock_wrlock (k_raw_rwlock_t *rw)
{
	__rwlock_wrlock (rw);
}

static __inline__ int rwlock_trywrlock (k_raw_rwlock_t *rw)
{
	return __rwlock_trywrlock (rw);
}

static __inline__ void rwlock_rdunlock (k_raw_rwlock_t *rw)
{
	__rwlock_rdunlock (rw);
}

static __inline__ void rwlock_wrunlock (k_raw_rwlock_t *rw)
{
	__rwlock_wrunlock (rw);
}

static __inline__ void rwlock_wr2rd_lock (k_raw_rwlock_t *rw)
{
	__rwlock_wr2rd_lock (rw);
}

#endif /* __KERNEL_RWLOCK_H__ */