/***************************************************************************
 * Module:	AOSL atomic operation API definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_ATOMIC_H__
#define __AOSL_ATOMIC_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef intptr_t aosl_atomic_t;

/**
 * @brief Atomically read the value of an atomic variable.
 * @param [in] v  pointer to the atomic variable
 * @return        the current value of *v
 **/
extern __aosl_api__ intptr_t aosl_atomic_read (const aosl_atomic_t *v);

/**
 * @brief Atomically set the value of an atomic variable.
 * @param [in,out] v  pointer to the atomic variable
 * @param [in]     i  the value to set
 **/
extern __aosl_api__ void aosl_atomic_set (aosl_atomic_t *v, intptr_t i);

/**
 * @brief Atomically increment an atomic variable by 1.
 * @param [in,out] v  pointer to the atomic variable
 **/
extern __aosl_api__ void aosl_atomic_inc (aosl_atomic_t *v);

/**
 * @brief Atomically decrement an atomic variable by 1.
 * @param [in,out] v  pointer to the atomic variable
 **/
extern __aosl_api__ void aosl_atomic_dec (aosl_atomic_t *v);

/**
 * @brief Atomically add a value and return the result.
 * @param [in]     i  the integer value to add
 * @param [in,out] v  pointer to the atomic variable
 * @return             the new value after addition
 **/
extern __aosl_api__ intptr_t aosl_atomic_add_return (intptr_t i, aosl_atomic_t *v);

/**
 * @brief Atomically subtract a value and return the result.
 * @param [in]     i  the integer value to subtract
 * @param [in,out] v  pointer to the atomic variable
 * @return             the new value after subtraction
 **/
extern __aosl_api__ intptr_t aosl_atomic_sub_return (intptr_t i, aosl_atomic_t *v);

/**
 * @brief Atomically increment and test if the result is zero.
 * @param [in,out] v  pointer to the atomic variable
 * @return             non-zero if the result is zero, 0 otherwise
 **/
extern __aosl_api__ int aosl_atomic_inc_and_test (aosl_atomic_t *v);

/**
 * @brief Atomically decrement and test if the result is zero.
 * @param [in,out] v  pointer to the atomic variable
 * @return             non-zero if the result is zero, 0 otherwise
 **/
extern __aosl_api__ int aosl_atomic_dec_and_test (aosl_atomic_t *v);

/**
 * @brief Atomic compare-and-exchange operation.
 * If the current value of *v equals oldval, set it to newval.
 * @param [in,out] v       pointer to the atomic variable
 * @param [in]     oldval  the expected old value
 * @param [in]     newval  the new value to set if comparison succeeds
 * @return                 the old value before the operation
 **/
extern __aosl_api__ intptr_t aosl_atomic_cmpxchg (aosl_atomic_t *v, intptr_t oldval, intptr_t newval);

/**
 * @brief Atomic exchange operation. Set *v to newval and return the old value.
 * @param [in,out] v       pointer to the atomic variable
 * @param [in]     newval  the new value to set
 * @return                 the old value before the exchange
 **/
extern __aosl_api__ intptr_t aosl_atomic_xchg (aosl_atomic_t *v, intptr_t newval);

/**
 * @brief Full memory barrier. Ensures all memory operations before this call
 * are completed before any memory operations after this call.
 **/
extern __aosl_api__ void aosl_mb (void);

/**
 * @brief Read memory barrier. Ensures all read operations before this call
 * are completed before any read operations after this call.
 **/
extern __aosl_api__ void aosl_rmb (void);

/**
 * @brief Write memory barrier. Ensures all write operations before this call
 * are completed before any write operations after this call.
 **/
extern __aosl_api__ void aosl_wmb (void);


#ifdef __cplusplus
}
#endif


#endif /* __AOSL_ATOMIC_H__ */
