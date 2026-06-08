/***************************************************************************
 * Module:	Multiplex queue header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_MPQ_H__
#define __AOSL_MPQ_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_thread.h>
#include <api/aosl_time.h>
#include <api/aosl_ref.h>

#ifdef __cplusplus
extern "C" {
#endif



/**
 * @brief AOSL performance callback function proto type.
 * Parameters:
 *     f_name: the functionality name, which was specified when
 *             queue/call something, it might be NULL if not
 *             specified a name
 *  free_only: 0 for normal exec, and other values for free only
 *    wait_us: the wait time in queue before executing(microseconds)
 *    exec_us: the executing time(microseconds)
 * Return Value:
 *    none.
 **/
typedef void (*aosl_perf_f_t) (const char *f_name, int free_only, uint32_t wait_us, uint32_t exec_us);

/**
 * @brief Set/Unset the AOSL performance callback function.
 * Note: this callback function MUST BE set/unset before
 *       creating any mpq or after all mpq have exited.
 * Parameters:
 *     perf_f: the performance callback function, NULL for unset
 * Return Value:
 *          0: success
 *         <0: failure with errno set
 * Remarks:
 *    If invoking this function when any mpq is active,
 *    then it will return -1 with errno set to AOSL_EPERM.
 **/
extern __aosl_api__ int aosl_perf_set_callback (aosl_perf_f_t perf_f);

typedef intptr_t aosl_mpq_t;

#define AOSL_MPQ_INVALID ((aosl_mpq_t)-1)

/** @brief Check if an mpq id is invalid. */
#define aosl_mpq_invalid(mpq) (((int16_t)(mpq)) < 0)

/**
 * @brief The initialize callback function of the mp queue create function.
 * Parameters:
 *        arg: the input parameter
 * Return Value:
 *         <0: error occurs
 *           0: ok, no error
 **/
typedef int (*aosl_mpq_init_t) (void *arg);

/**
 * @brief The finalize callback function of the mp queue destroy function.
 * Parameters:
 *        arg: the input parameter
 **/
typedef void (*aosl_mpq_fini_t) (void *arg);


/* I think this is big enough */
#define MPQ_MAX_SIZE 1000000

/**
 * @brief Create an multiplex queue.
 * Parameter:
 *      pri: the priority of the mpq, 0 for system default
 *           #define AOSL_THRD_PRI_DEFAULT 0
 *           #define AOSL_THRD_PRI_LOW 1
 *           #define AOSL_THRD_PRI_NORMAL 2
 *           #define AOSL_THRD_PRI_HIGH 3
 *           #define AOSL_THRD_PRI_HIGHEST 4
 *           #define AOSL_THRD_PRI_RT 5
 *      max: specify the max queue size, must > 0 and <= MPQ_MAX_SIZE
 *     name: the queue name
 *     init: the initialize callback function
 *     fini: the finalize callback function
 *     arg: the parameter passed to init callback
 * Return value:
 *     the queue object just created, <0 when failed.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpq_create (int pri, int stack_size, int max, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);

// qflags definitions
#define AOSL_MPQ_FLAG_NONBLOCK   0x00000001
#define AOSL_MPQ_FLAG_SIGP_EVENT 0x00000002
#define AOSL_MPQ_FLAG_DESTROY_NOT_ALLOWED  0x80000000  // internal

/**
 * @brief Create an multiplex queue with specified flags.
 * Parameter:
 *    flags: specify the queue flags
 *      pri: the priority of the mpq, 0 for system default
 *           #define AOSL_THRD_PRI_DEFAULT 0
 *           #define AOSL_THRD_PRI_LOW 1
 *           #define AOSL_THRD_PRI_NORMAL 2
 *           #define AOSL_THRD_PRI_HIGH 3
 *           #define AOSL_THRD_PRI_HIGHEST 4
 *           #define AOSL_THRD_PRI_RT 5
 *      max: specify the max queue size, must > 0 and <= MPQ_MAX_SIZE
 *     name: the queue name
 *     init: the initialize callback function
 *     fini: the finalize callback function
 *      arg: the parameter passed to init callback
 * Return value:
 *     the queue object just created, <0 when failed.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpq_create_flags (int flags, int pri, int stack_size, int max, const char *name, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);

#define BITOP_OR 0
#define BITOP_AND 1
#define BITOP_XOR 2

extern __aosl_api__ int aosl_mpq_change_flags (aosl_mpq_t qid, int bit_op, int bits);

extern __aosl_api__ int aosl_mpq_get_flags (aosl_mpq_t qid);

/* Get this multiplex queue id, if not exists, then just return -1 */
extern __aosl_api__ aosl_mpq_t aosl_mpq_this (void);

/**
 * @brief Get the current multiplex queue id, if not exists yet, then create it for the
 * current running thread with default queue size and return it.
 * Generally, this function is only used in the non-mpq thread, such as the main
 * thread.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpq_current (void);

/**
 * @brief Get the N-th argument of the current running queued function (argv version).
 * Parameters:
 *     n: which argument you want to get, the first arg is 0;
 *   arg: the argument variable address to save the argument value;
 * Return value:
 *    <0: error occured, and errno indicates which error;
 *     0: call successful, and '*arg' is value of the N-th argument;
 **/
extern __aosl_api__ int aosl_mpq_run_func_arg (uintptr_t n, uintptr_t *arg);

/**
 * @brief Get the data and length of the current running queued function (data version).
 * Parameters:
 *    len_p: return the queued *_data function data length if not NULL;
 *   data_p: return the queued *_data function data pointer if not NULL;
 * Return value:
 *    <0: error occured, and errno indicates which error;
 *     0: call successful;
 **/
extern __aosl_api__ int aosl_mpq_run_func_data (size_t *len_p, void **data_p);

/**
 * @brief Get the done mpq id of the current running queued function.
 * Parameters:
 *     None.
 * Return value:
 *    <0: error occured or the current running function has no done mpq id;
 *   >=0: running function's done mpq id;
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpq_run_func_done_qid (void);

/**
 * @brief Checking whether the refobj specified by robj is just running on the
 * running mpq.
 * Parameters:
 *       robj: the ref object
 * Return value:
 *          0: Not running on this mpq now
 *     None-0: Just running on this mpq now
 **/
extern __aosl_api__ int aosl_mpq_running_refobj (aosl_refobj_t robj);

/**
 * @brief Get the arg of the running mpq object which was specified when creating.
 * Parameters:
 *      None.
 * Return value:
 *    The arg.
 **/
extern __aosl_api__ void *aosl_mpq_get_q_arg (void);

/**
 * @brief Set the arg of the running mpq object.
 * Parameters:
 *      arg: the arg
 * Return value:
 *    <0: error occured, and errno indicates which error;
 *     0: call successful;
 **/
extern __aosl_api__ int aosl_mpq_set_q_arg (void *arg);


/**
 * @brief The queue function prototype, something like the standard 'main' function.
 * Parameter:
 *    queued_ts_p: pointer of milliseconds timestamp of the func being queued
 *                 We employ a 'pointer' instead of the timestamp value here 
 *                       just for only passing one value on the 32bit system
 *           robj: when this parameter is AOSL_FREE_ONLY_OBJ, you can only
 *                 free the relative resources, MUST NOT DO ANYTHING ELSE;
 *           argc: specify the argv array elements count
 *           argv: array for passing variable args
 * Return value:
 *       none.
 **/
typedef void (*aosl_mpq_func_argv_t) (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc, uintptr_t argv []);

/**
 * @brief Another queue function prototype for passing a chunk of data.
 * Parameter:
 *    queued_ts_p: pointer of milliseconds timestamp of the func being queued
 *                 We employ a 'pointer' instead of the timestamp value here 
 *                       just for only passing one value on the 32bit system.
 *           robj: when this parameter is AOSL_FREE_ONLY_OBJ, you can only
 *                 free the relative resources, MUST NOT DO ANYTHING ELSE;
 *            len: the passed data length
 *           data: the data itself
 * Return value:
 *     none.
 **/
typedef void (*aosl_mpq_func_data_t) (const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, size_t len, void *data);

/**
 * @brief Queue a function with args for invoking by the target thread which monitoring
 * the queue.
 * Parameter:
 *        tq: the target queue id
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
extern __aosl_api__ int aosl_mpq_queue (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);

/* The synchronous version, the target f must have been invoked when this function returns */
extern __aosl_api__ int aosl_mpq_call (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);

/**
 * @brief Run the specified function on the specified mpq in async or sync mode:
 * 1. identical to aosl_mpq_queue if the target q is not the same as the running q;
 * 2. identical to aosl_mpq_call if the target q is just the running q;
 **/
extern __aosl_api__ int aosl_mpq_run (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, ...);


/**
 * @brief The same as 'aosl_mpq_queue' except this function takes a 'va_list' arg for the
 * variable args rather than '...'.
 **/
extern __aosl_api__ int aosl_mpq_queue_args (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);

/* The synchronous version, the target f must have been invoked when this function returns */
extern __aosl_api__ int aosl_mpq_call_args (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);

/**
 * @brief Run the specified function on the specified mpq in async or sync mode:
 * 1. identical to aosl_mpq_queue_args if the target q is not the same as the running q;
 * 2. identical to aosl_mpq_call_args if the target q is just the running q;
 **/
extern __aosl_api__ int aosl_mpq_run_args (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);

/**
 * @brief The same as 'aosl_mpq_queue_args' except this function takes a 'uintptr_t *' arg for the argv
 * rather than 'va_list args'.
 **/
extern __aosl_api__ int aosl_mpq_queue_argv (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);

/* The synchronous version, the target f must have been invoked when this function returns */
extern __aosl_api__ int aosl_mpq_call_argv (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);

/**
 * @brief Run the specified function on the specified mpq in async or sync mode:
 * 1. identical to aosl_mpq_queue_argv if the target q is not the same as the running q;
 * 2. identical to aosl_mpq_call_argv if the target q is just the running q;
 **/
extern __aosl_api__ int aosl_mpq_run_argv (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_argv_t f, uintptr_t argc, uintptr_t *argv);


/**
 * @brief Queue a function with a chunk of data for invoking by the target thread which monitoring
 * the queue.
 * Parameter:
 *        tq: the target queue id
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
 *        <0: indicates error, check errno for detail
 *         0: successful.
 **/
extern __aosl_api__ int aosl_mpq_queue_data (aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

/* The synchronous version, the target f must have been invoked when this function returns */
extern __aosl_api__ int aosl_mpq_call_data (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

/**
 * @brief Run the specified function on the specified mpq in async or sync mode:
 * 1. identical to aosl_mpq_queue_data if the target q is not the same as the running q;
 * 2. identical to aosl_mpq_call_data if the target q is just the running q;
 **/
extern __aosl_api__ int aosl_mpq_run_data (aosl_mpq_t q, aosl_ref_t ref, const char *f_name, aosl_mpq_func_data_t f, size_t len, void *data);

/**
 * @brief Start aosl main mpq, only a single main mpq allowed.
 **/
extern __aosl_api__ int aosl_main_start (int pri, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);

/**
 * @brief Get the aosl main mpq id, and return -1 for not exist.
 **/
extern __aosl_api__ aosl_mpq_t aosl_mpq_main (void);

/**
 * @brief Check whether the aosl main mpq started.
 **/
#define aosl_main_started() (!aosl_mpq_invalid (aosl_mpq_main ()))


/**
 * @brief Get the queued function invocations count.
 * Parameters:
 *      q: the queue object id
 * Return value:
 *     <0: indicates error, check errno for detail
 *    >=0: the queued function invocations count
 */
extern __aosl_api__ int aosl_mpq_queued_count (aosl_mpq_t q);

/**
 * @brief Get the last load/idle costs in micro seconds of this mpq
 * Parameters:
 *   load_p: the variable for saving the last load costs(us), NULL for not care
 *   idle_p: the variable for saving the last idle costs(us), NULL for not care
 * Return value:
 *     <0: indicates error, check errno for detail
 *      0: the last load/idle costs was/were saved to *load_p, *idle_p
 */
extern __aosl_api__ int aosl_mpq_last_costs (aosl_ts_t *load_p, aosl_ts_t *idle_p);

/**
 * @brief Get the current running counters of this mpq
 * Parameters:
 *   funcs_count_p: the variable for saving the current running funcs count, NULL for not care
 *  timers_count_p: the variable for saving the current running timers count, NULL for not care
 *     fds_count_p: the variable for saving the current running fds count, NULL for not care
 * Return value:
 *     <0: indicates error, check errno for detail
 *      0: the running counters was/were saved to *funcs_count_p, *timers_count_p, *fds_count_p
 */
extern __aosl_api__ int aosl_mpq_exec_counters (uint64_t *funcs_count_p, uint64_t *timers_count_p, uint64_t *fds_count_p);

/**
 * @brief Invoking this function will enter the infinite run loop of current thread's multiplex queue.
 * Generally, this function is only used in the non-mpq thread, such as the main thread.
 **/
extern __aosl_api__ void aosl_mpq_loop (void);

/**
 * @brief Check whether the running mpq has been destroyed, this is useful when
 * there is a loop in some processing, and we can determine whether we
 * should exit the loop ASAP.
 * Return value:
 *              0: this mpq is not been destroyed;
 *   Other values: this mpq has been destroyed;
 **/
extern __aosl_api__ int aosl_mpq_this_destroyed (void);

/**
 * @brief Check whether the running mpq is the aosl main mpq.
 * Return value:
 *       non-zero: the running mpq is the main mpq;
 *              0: the running mpq is not the main mpq;
 **/
extern __aosl_api__ int aosl_mpq_is_main (void);

/**
 * @brief Destroy an multiplex queue object.
 * Parameter:
 *        mpq_id: the queue id to be destroyed
 **/
extern __aosl_api__ int aosl_mpq_destroy (aosl_mpq_t q);


/**
 * @brief Destroy an multiplex queue object and wait until done.
 * Parameter:
 *        mpq_id: the queue id to be destroyed
 **/
extern __aosl_api__ int aosl_mpq_destroy_wait (aosl_mpq_t mpq_id);

/**
 * @brief Wait an multiplex queue object to be destroyed. Note
 * that this function will not do the destroy action, and
 * just wait for the queue to be destroyed.
 * Parameter:
 *        mpq_id: the queue id to wait
 **/
extern __aosl_api__ int aosl_mpq_wait (aosl_mpq_t mpq_id);

/**
 * @brief Let the aosl main multiplex queue thread exit, but not wait.
 **/
extern __aosl_api__ int aosl_main_exit (void);

/**
 * @brief Let the aosl main multiplex queue thread exit, and wait done.
 **/
extern __aosl_api__ int aosl_main_exit_wait (void);

/**
 * @brief Wait the aosl main multiplex queue thread to exit.
 * Note that this function will not let the aosl main thread exit,
 * just wait for the queue thread to exit.
 * Parameter:
 *        mpq_id: the queue id to wait
 **/
extern __aosl_api__ int aosl_main_wait (void);


typedef const struct _aosl_stack_ *aosl_stack_id_t;
#define AOSL_STACK_INVALID NULL

/**
 * @brief Check if a stack id is invalid.
 * @param [in] id  the stack id to check
 */
#define aosl_stack_invalid(id) ((id) == AOSL_STACK_INVALID)

/**
 * @brief Define and initialize a stack-local stack id variable.
 * @param [in] id  the variable name for the stack id
 */
#define aosl_define_stack(id) aosl_stack_id_t id = (aosl_stack_id_t)&id


/**
 * @brief Shrink the resources allocated dynamically by AOSL.
 * Only need to do this when you do not use AOSL again but
 * the program does not exit now.
 **/
extern __aosl_api__ void aosl_shrink_resources (void);


#ifdef __cplusplus
}
#endif

#endif /* __AOSL_MPQ_H__ */