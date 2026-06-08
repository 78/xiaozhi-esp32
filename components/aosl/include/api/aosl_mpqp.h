/***************************************************************************
 * Module:	Multiplex queue pool header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __MPQP_H__
#define __MPQP_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>

#include <api/aosl_mpq.h>


#ifdef __cplusplus
extern "C" {
#endif



typedef void *aosl_mpqp_t;


/* This value should be big enough for the pool size */
#define MPQP_MAX_SIZE 65536

/**
 * @brief Create a multiplex queue pool.
 * Parameter:
 *   pool_size: the pool size
 *           pri: the priority of the mpq in this mpq pool, 0 for system default
 *                #define AOSL_THRD_PRI_DEFAULT 0
 *                #define AOSL_THRD_PRI_LOW 1
 *                #define AOSL_THRD_PRI_NORMAL 2
 *                #define AOSL_THRD_PRI_HIGH 3
 *                #define AOSL_THRD_PRI_HIGHEST 4
 *                #define AOSL_THRD_PRI_RT 5
 *           max: specify the max queue size, must not greater than MPQP_MAX_SIZE
 *     max_idles: specify the max idle count before shrinking the pool:
 *                                 <0: no max idle count
 *                ==0 || > 0x7fffffff: invalid
 *                       other values: the max idle count
 *          name: the queue poll name
 *          init: the initialize callback function
 *          fini: the finalize callback function
 *           arg: the parameter passed to init callback
 * Return value:
 *     the queue object just created, NULL when failed.
 **/
extern __aosl_api__ aosl_mpqp_t aosl_mpqp_create (int pool_size, int pri, int stack_size, int max,
    int max_idles, int flags, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);

/**
 * @brief Queue a function to the pool with args for invoking by the target thread which monitoring
 * the corresponding queue object.
 * Parameter:
 *        qp: the queue pool object
 *        dq: the done queue id, -1 for no done notification
 *            If dq is a valid mpq id, then the queued function f will be
 *            queued to the mpq specified by dq, and then got executed again.
 *            It is the responsibility of the function itself to differentiate the
 *            reentrance, such as, identify the reentrance according to one of arg.
 *       ref: the mpq object id for indicating whether the relative operation
 *            should be aborted and only free the relative resources, if the
 *            mpq object specified by ref has been destroyed, then target
 *            function specified by 'f' MUST NOT DO ANYTING ELSE except free
 *            the relative resources.
 *         f: the function
 *      argc: the args count
 *       ...: variable args
 * Return value:
 *        Queued target mpq queue id, checking the result with aosl_mpq_invalid:
 *        invalid qid: indicates error, check errno for detail
 *          valid qid: successful, returns the queue id of selected queue.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpqp_queue (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);

/**
 * @brief The synchronous version, the target f must have been invoked when this function returns.
 * Parameter:
 *        qp: the queue pool object
 *       ref: the mpq object id for indicating whether the relative operation
 *            should be aborted and only free the relative resources, if the
 *            mpq object specified by ref has been destroyed, then target
 *            function specified by 'f' MUST NOT DO ANYTING ELSE except free
 *            the relative resources.
 *         f: the function
 *      argc: the args count
 *       ...: variable args
 * Return value:
 *        Queued target mpq queue id, checking the result with aosl_mpq_invalid:
 *        invalid qid: indicates error, check errno for detail
 *          valid qid: successful, returns the queue id of selected queue.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpqp_call (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);


/**
 * @brief The same as 'aosl_mpqp_queue' except this function takes a 'va_list' arg for the
 * variable args rather than '...'.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpqp_queue_args (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);

/* The synchronous version, the target f must have been invoked when this function returns */
extern __aosl_api__ aosl_mpq_t aosl_mpqp_call_args (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);

/**
 * @brief The same as 'aosl_mpqp_queue' except this function takes a 'uintptr_t *argv' arg for the
 * variable args rather than '...'.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpqp_queue_argv (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);

/* The synchronous version, the target f must have been invoked when this function returns */
extern __aosl_api__ aosl_mpq_t aosl_mpqp_call_argv (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);


/**
 * @brief Queue a function to the pool with a chunk of data for invoking by the target thread which monitoring
 * the queue.
 * Parameter:
 *        qp: the queue pool object
 *        dq: the done queue id, -1 for no done notification
 *            If dq is a valid mpq id, then the queued function f will be
 *            queued to the mpq specified by dq, and then got executed again.
 *            It is the responsibility of the function itself to differentiate the
 *            reentrance, such as, identify the reentrance according to one of arg.
 *       ref: the mpq object id for indicating whether the relative operation
 *            should be aborted and only free the relative resources, if the
 *            mpq object specified by ref has been destroyed, then target
 *            function specified by 'f' MUST NOT DO ANYTING ELSE except free
 *            the relative resources.
 *         f: the function
 *       len: the data length in bytes
 *      data: the data pointer which contains the data(would be copied in)
 * Return value:
 *        Queued target mpq queue id, checking the result with aosl_mpq_invalid:
 *        invalid qid: indicates error, check errno for detail
 *          valid qid: successful, returns the queue id of selected queue.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpqp_queue_data (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

/* The synchronous version, the target f must have been invoked when this function returns */
extern __aosl_api__ aosl_mpq_t aosl_mpqp_call_data (aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);


/**
 * @brief Queue a function to the pool and promise all queues have done
 * the previous queued jobs when invoking the this queued func.
 * Parameter:
 *        qp: the queue pool object
 *        dq: the done queue id, -1 for no done notification
 *            If dq is a valid mpq id, then the queued function f will be
 *            queued to the mpq specified by dq, and then got executed again.
 *            It is the responsibility of the function itself to differentiate the
 *            reentrance, such as, identify the reentrance according to one of arg.
 *       ref: the mpq object id for indicating whether the relative operation
 *            should be aborted and only free the relative resources, if the
 *            mpq object specified by ref has been destroyed, then target
 *            function specified by 'f' MUST NOT DO ANYTING ELSE except free
 *            the relative resources.
 *         f: the function
 *      argc: the args count
 *       ...: variable args
 * Return value:
 *        <0: indicates error, check errno for detail
 *         0: successful.
 **/
extern __aosl_api__ int aosl_mpqp_pool_tail_queue (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);
extern __aosl_api__ int aosl_mpqp_pool_tail_queue_args (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);
extern __aosl_api__ int aosl_mpqp_pool_tail_queue_argv (aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t argv []);


/**
 * @brief Shrink a multiplex queue pool object by 1.
 * Parameter:
 *        qp: the queue pool object to be shrinked
 *      wait: whether to wait for the q exit
 * Return value:
 *        <0: the pool size is only one now, no permission to shrink any more
 *            if want shrink to zero size, please use aosl_mpqp_shrink_all;
 *         0: shrinked 1 q successfully;
 **/
extern __aosl_api__ int aosl_mpqp_shrink (aosl_mpqp_t qp, int wait);

/**
 * @brief Shrink all MPQs in a multiplex queue pool object.
 * Parameter:
 *        qp: the queue pool object to be shrinked
 *      wait: 0 for not wait, other values for wait
 **/
extern __aosl_api__ void aosl_mpqp_shrink_all (aosl_mpqp_t qp, int wait);

/**
 * @brief Get the system CPUP object.
 * Parameter:
 *       none.
 * Return value:
 *       the CPUP object.
 **/
extern __aosl_api__ aosl_mpqp_t aosl_cpup (void);

/**
 * @brief Get the system GPUP object.
 * Parameter:
 *       none.
 * Return value:
 *       the GPUP object.
 **/
extern __aosl_api__ aosl_mpqp_t aosl_gpup (void);

/**
 * @brief Get the system GENP object.
 * Parameter:
 *       none.
 * Return value:
 *       the GENP object.
 **/
extern __aosl_api__ aosl_mpqp_t aosl_genp (void);

/**
 * @brief Get the system LTWP object.
 * Parameter:
 *       none.
 * Return value:
 *       the LTWP object.
 **/
extern __aosl_api__ aosl_mpqp_t aosl_ltwp (void);

/**
 * @brief Allocate an mpq object for dedicate processing some jobs according to
 * the capability of the system. This function is pure different with the
 * aosl_mpq_create* series functions which create a new mpq object anyway,
 * instead, this function returns an mpq object according to the system
 * capability, so the returned mpq id may be identical with some previous
 * return value.
 * Note: once the dedicated jobs have been done, must use aosl_mpq_free
 *       to free the allocated mpq.
 * Parameter:
 *       none.
 * Return value:
 *       the allocated mpq id.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpq_alloc (void);

/**
 * @brief Free an mpq object which returned by aosl_mpq_alloc previously.
 * Note: once the dedicated jobs have been done, must use this function
 *       to free the allocated mpq returned by previous aosl_mpq_alloc.
 * Parameter:
 *       the mpq id.
 * Return value:
 *       0: for success
 *      -1: for failure, check errno for detail
 **/
extern __aosl_api__ int aosl_mpq_free (aosl_mpq_t qid);

/**
 * @brief Destroy a multiplex queue pool object.
 * Parameter:
 *        qp: the queue pool object to be destroyed
 *      wait: 0 for not wait, other values for wait
 **/
extern __aosl_api__ void aosl_mpqp_destroy (aosl_mpqp_t qp, int wait);


#ifdef __cplusplus
}
#endif

#endif /* __MPQP_H__ */