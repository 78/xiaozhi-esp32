/***************************************************************************
 * Module:	rwlock arch generic implementation file.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __ARCH_GENERIC_RWLOCK_H__
#define __ARCH_GENERIC_RWLOCK_H__

#include <stdint.h>
#include <kernel/types.h>
#include <kernel/atomic.h>
#include <kernel/compiler.h>

#if INTPTR_MAX == INT64_MAX
#define RWLOCK_ACTIVE_MASK ((intptr_t)0xffffffff)
#else
#define RWLOCK_ACTIVE_MASK ((intptr_t)0x0000ffff)
#endif

#define RWLOCK_UNLOCKED_VALUE ((intptr_t)0)
#define RWLOCK_ACTIVE_BIAS ((intptr_t)1)
#define RWLOCK_WAITING_BIAS ((intptr_t)-((intptr_t)RWLOCK_ACTIVE_MASK + 1))
#define RWLOCK_ACTIVE_READ_BIAS RWLOCK_ACTIVE_BIAS
#define RWLOCK_ACTIVE_WRITE_BIAS ((intptr_t)(RWLOCK_WAITING_BIAS + RWLOCK_ACTIVE_BIAS))

static inline void __rwlock_rdlock (k_raw_rwlock_t *rw)
{
	if (atomic_intptr_inc_return ((atomic_intptr_t *)&rw->count) <= 0)
		rwlock_read_lock_failed (&rw->count);
}

static inline int __rwlock_tryrdlock (k_raw_rwlock_t *rw)
{
	intptr_t tmp;

	while ((tmp = atomic_intptr_read ((atomic_intptr_t *)&rw->count)) >= 0) {
		if (tmp == atomic_intptr_cmpxchg ((atomic_intptr_t *)&rw->count, tmp,
				   tmp + RWLOCK_ACTIVE_READ_BIAS)) {
			return 1;
		}
	}

	return 0;
}

static inline void __rwlock_wrlock (k_raw_rwlock_t *rw)
{
	intptr_t tmp;

	tmp = atomic_intptr_add_return (RWLOCK_ACTIVE_WRITE_BIAS, (atomic_intptr_t *)&rw->count);
	if (tmp != RWLOCK_ACTIVE_WRITE_BIAS)
		rwlock_write_lock_failed (&rw->count);
}

static inline int __rwlock_trywrlock(k_raw_rwlock_t *rw)
{
	intptr_t tmp;
	tmp = atomic_intptr_cmpxchg ((atomic_intptr_t *)&rw->count, RWLOCK_UNLOCKED_VALUE, RWLOCK_ACTIVE_WRITE_BIAS);
	return tmp == RWLOCK_UNLOCKED_VALUE;
}

static inline void __rwlock_rdunlock (k_raw_rwlock_t *rw)
{
	intptr_t tmp;
	tmp = atomic_intptr_dec_return ((atomic_intptr_t *)&rw->count);
	if (tmp < -1 && (tmp & RWLOCK_ACTIVE_MASK) == 0)
		rwlock_wake (&rw->count);
}

static inline void __rwlock_wrunlock (k_raw_rwlock_t *rw)
{
	if (atomic_intptr_sub_return (RWLOCK_ACTIVE_WRITE_BIAS, (atomic_intptr_t *)&rw->count) < 0)
		rwlock_wake (&rw->count);
}

static inline void __rwlock_wr2rd_lock (k_raw_rwlock_t *rw)
{
	intptr_t tmp;

	tmp = atomic_intptr_add_return (-RWLOCK_WAITING_BIAS, (atomic_intptr_t *)&rw->count);
	if (tmp < 0)
		rwlock_downgrade_wake (&rw->count);
}

static inline void rwlock_atomic_add (intptr_t delta, k_raw_rwlock_t *rw)
{
	atomic_intptr_add (delta, (atomic_intptr_t *)&rw->count);
}

static inline intptr_t rwlock_atomic_update (intptr_t delta, k_raw_rwlock_t *rw)
{
	return atomic_intptr_add_return (delta, (atomic_intptr_t *)&rw->count);
}



#endif /* __ARCH_GENERIC_RWLOCK_H__ */