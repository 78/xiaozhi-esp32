/***************************************************************************
 * Module:	atomic
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __KERNEL_ATOMIC_H__
#define __KERNEL_ATOMIC_H__

#include <hal/aosl_hal_atomic.h>

typedef intptr_t atomic_t;
typedef intptr_t atomic_intptr_t;

#define atomic_read(v) aosl_hal_atomic_read(v)
#define atomic_set(v, i) aosl_hal_atomic_set(v, i)
#define atomic_add(i, v) aosl_hal_atomic_add(i, v)
#define atomic_sub(i, v) aosl_hal_atomic_sub(i, v)
#define atomic_inc(v) aosl_hal_atomic_inc(v)
#define atomic_dec(v) aosl_hal_atomic_dec(v)
#define atomic_add_return(i, v) aosl_hal_atomic_add(i, v)
#define atomic_sub_return(i, v) aosl_hal_atomic_sub(i, v)
#define atomic_inc_return(v)  (atomic_add(1, (v)))
#define atomic_dec_return(v)  (atomic_sub(1, (v)))
#define atomic_inc_and_test(v) (atomic_inc_return((v)) == 0)
#define atomic_dec_and_test(v) (atomic_dec_return((v)) == 0)
#define atomic_cmpxchg(v, old, new) aosl_hal_atomic_cmpxchg(v, old, new)
#define atomic_xchg(v, new) aosl_hal_atomic_xchg(v, new)


#define atomic_intptr_read(v) aosl_hal_atomic_read(v)
#define atomic_intptr_set(v, i) aosl_hal_atomic_set(v, i)
#define atomic_intptr_add(i, v) aosl_hal_atomic_add(i, v)
#define atomic_intptr_sub(i, v) aosl_hal_atomic_sub(i, v)
#define atomic_intptr_inc(v) aosl_hal_atomic_inc(v)
#define atomic_intptr_dec(v) aosl_hal_atomic_dec(v)
#define atomic_intptr_add_return(i, v) aosl_hal_atomic_add(i, v)
#define atomic_intptr_sub_return(i, v) aosl_hal_atomic_sub(i, v)
#define atomic_intptr_inc_return(v)  (atomic_add(1, (v)))
#define atomic_intptr_dec_return(v)  (atomic_sub(1, (v)))
#define atomic_intptr_inc_and_test(v) (atomic_intptr_inc_return((v)) == 0)
#define atomic_intptr_dec_and_test(v) (atomic_intptr_dec_return((v)) == 0)
#define atomic_intptr_cmpxchg(v, old, new) aosl_hal_atomic_cmpxchg(v, old, new)
#define atomic_intptr_xchg(v, new) aosl_hal_atomic_xchg(v, new)

#endif /* __KERNEL_ATOMIC_H__ */
