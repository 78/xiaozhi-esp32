/***************************************************************************
 * Module:	AOSL threading relative internal implementations.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/err.h>
#include <kernel/thread.h>
#include <api/aosl_mm.h>
#include <api/aosl_thread.h>
#include <api/aosl_time.h>


void k_raw_rwlock_init (k_raw_rwlock_t *rw)
{
	rwlock_init (rw);
}

void k_raw_rwlock_rdlock (k_raw_rwlock_t *rw)
{
	rwlock_rdlock (rw);
}

int k_raw_rwlock_tryrdlock (k_raw_rwlock_t *rw)
{
	return rwlock_tryrdlock (rw);
}

void k_raw_rwlock_wrlock (k_raw_rwlock_t *rw)
{
	rwlock_wrlock (rw);
}

int k_raw_rwlock_trywrlock (k_raw_rwlock_t *rw)
{
	return rwlock_trywrlock (rw);
}

void k_raw_rwlock_rdunlock (k_raw_rwlock_t *rw)
{
	rwlock_rdunlock (rw);
}

void k_raw_rwlock_wrunlock (k_raw_rwlock_t *rw)
{
	rwlock_wrunlock (rw);
}

void k_raw_rwlock_destroy (k_raw_rwlock_t *rw)
{
	aosl_hal_mutex_destroy (rw->wait_lock);
}