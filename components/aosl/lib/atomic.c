/***************************************************************************
 * Module:	AOSL atomic operation API implementations.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <api/aosl_atomic.h>
#include <kernel/atomic.h>

__export_in_so__ intptr_t aosl_atomic_read (const aosl_atomic_t *v)
{
	return atomic_read ((const atomic_t *)v);
}

__export_in_so__ void aosl_atomic_set (aosl_atomic_t *v, intptr_t i)
{
	atomic_set ((atomic_t *)v, i);
}

__export_in_so__ void aosl_atomic_inc (aosl_atomic_t *v)
{
	atomic_inc ((atomic_t *)v);
}

__export_in_so__ void aosl_atomic_dec (aosl_atomic_t *v)
{
	atomic_dec ((atomic_t *)v);
}

__export_in_so__ intptr_t aosl_atomic_add_return (intptr_t i, aosl_atomic_t *v)
{
	return atomic_add_return (i, (atomic_t *)v);
}

__export_in_so__ intptr_t aosl_atomic_sub_return (intptr_t i, aosl_atomic_t *v)
{
	return atomic_sub_return (i, (atomic_t *)v);
}

__export_in_so__ int aosl_atomic_inc_and_test (aosl_atomic_t *v)
{
	return atomic_inc_and_test ((atomic_t *)v);
}

__export_in_so__ int aosl_atomic_dec_and_test (aosl_atomic_t *v)
{
	return atomic_dec_and_test ((atomic_t *)v);
}

__export_in_so__ intptr_t aosl_atomic_cmpxchg (aosl_atomic_t *v, intptr_t old, intptr_t new)
{
	return atomic_cmpxchg ((atomic_t *)v, old, new);
}

__export_in_so__ intptr_t aosl_atomic_xchg (aosl_atomic_t *v, intptr_t new)
{
	return atomic_xchg ((atomic_t *)v, new);
}

__export_in_so__ void aosl_mb (void)
{
	aosl_hal_mb();
}
__export_in_so__ void aosl_rmb (void)
{
	aosl_hal_rmb();
}
__export_in_so__ void aosl_wmb (void)
{
	aosl_hal_wmb();
}
