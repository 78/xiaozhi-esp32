/***************************************************************************
 * Module:	Multiplex queue timer header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __MPQ_TIMER_H__
#define __MPQ_TIMER_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_time.h>
#include <api/aosl_mpq.h>


#ifdef __cplusplus
extern "C" {
#endif


/* The timer object type */
typedef int aosl_timer_t;

#define AOSL_MPQ_TIMER_INVALID ((aosl_timer_t)-1)

/** @brief Check if a timer id is invalid. */
#define aosl_mpq_timer_invalid(timer_id) (((int16_t)(timer_id)) < 0)

/* The proto for a timer argument destructor function. */
typedef void (*aosl_obj_dtor_t) (uintptr_t argc, uintptr_t argv []);

/* The proto for a timer-callback function. */
typedef void (*aosl_timer_func_t) (aosl_timer_t timer_id, const aosl_ts_t *now_p, uintptr_t argc, uintptr_t argv []);


#define AOSL_INVALID_TIMER_INTERVAL ((uintptr_t)(-1))

/**
 * @brief Create a periodic timer on the current mpq. The timer is initially inactive
 * and must be scheduled with aosl_mpq_resched_timer to start firing.
 * @param [in] interval  the timer interval in milliseconds
 * @param [in] func      the callback function invoked on each tick
 * @param [in] dtor      the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc      the number of variable arguments
 * @param [in] ...       variable arguments passed to the callback
 * @return               the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_create_timer (uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Create a periodic timer on the specified mpq.
 * @param [in] qid       the target mpq id
 * @param [in] interval  the timer interval in milliseconds
 * @param [in] func      the callback function invoked on each tick
 * @param [in] dtor      the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc      the number of variable arguments
 * @param [in] ...       variable arguments passed to the callback
 * @return               the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_create_timer_on_q (aosl_mpq_t qid, uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Create a one-shot timer on the current mpq. The timer is initially inactive
 * and must be scheduled with aosl_mpq_resched_oneshot_timer to fire.
 * @param [in] func  the callback function invoked when the timer fires
 * @param [in] dtor  the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc  the number of variable arguments
 * @param [in] ...   variable arguments passed to the callback
 * @return           the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_create_oneshot_timer (aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Create a one-shot timer on the specified mpq.
 * @param [in] qid   the target mpq id
 * @param [in] func  the callback function invoked when the timer fires
 * @param [in] dtor  the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc  the number of variable arguments
 * @param [in] ...   variable arguments passed to the callback
 * @return           the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_create_oneshot_timer_on_q (aosl_mpq_t qid, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Create and immediately schedule a periodic timer on the current mpq.
 * @param [in] interval  the timer interval in milliseconds
 * @param [in] func      the callback function invoked on each tick
 * @param [in] dtor      the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc      the number of variable arguments
 * @param [in] ...       variable arguments passed to the callback
 * @return               the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_set_timer (uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Create and immediately schedule a periodic timer on the specified mpq.
 * @param [in] qid       the target mpq id
 * @param [in] interval  the timer interval in milliseconds
 * @param [in] func      the callback function invoked on each tick
 * @param [in] dtor      the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc      the number of variable arguments
 * @param [in] ...       variable arguments passed to the callback
 * @return               the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_set_timer_on_q (aosl_mpq_t qid, uintptr_t interval, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Create and immediately schedule a one-shot timer on the current mpq.
 * @param [in] expire_time  the absolute tick time at which the timer fires
 * @param [in] func         the callback function invoked when the timer fires
 * @param [in] dtor         the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc         the number of variable arguments
 * @param [in] ...          variable arguments passed to the callback
 * @return                  the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_set_oneshot_timer (aosl_ts_t expire_time, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Create and immediately schedule a one-shot timer on the specified mpq.
 * @param [in] qid          the target mpq id
 * @param [in] expire_time  the absolute tick time at which the timer fires
 * @param [in] func         the callback function invoked when the timer fires
 * @param [in] dtor         the destructor called when the timer is destroyed (may be NULL)
 * @param [in] argc         the number of variable arguments
 * @param [in] ...          variable arguments passed to the callback
 * @return                  the timer id, use aosl_mpq_timer_invalid() to check for failure
 **/
extern __aosl_api__ aosl_timer_t aosl_mpq_set_oneshot_timer_on_q (aosl_mpq_t qid, aosl_ts_t expire_time, aosl_timer_func_t func, aosl_obj_dtor_t dtor, uintptr_t argc, ...);

/**
 * @brief Get the interval of a periodic timer.
 * @param [in]  timer_id    the timer id
 * @param [out] interval_p  pointer to receive the interval value
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_timer_interval (aosl_timer_t timer_id, uintptr_t *interval_p);

/**
 * @brief Check whether a timer is currently active (scheduled).
 * @param [in]  timer_id  the timer id
 * @param [out] active_p  pointer to receive the active state: 1 for active, 0 for inactive
 * @return                0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_timer_active (aosl_timer_t timer_id, int *active_p);

/**
 * @brief Reschedule a periodic timer with a new interval.
 * Use AOSL_INVALID_TIMER_INTERVAL to keep the current interval unchanged.
 * @param [in] timer_id  the timer id
 * @param [in] interval  the new interval in milliseconds
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_resched_timer (aosl_timer_t timer_id, uintptr_t interval);

/**
 * @brief Reschedule a one-shot timer with a new expiration time.
 * @param [in] timer_id     the timer id
 * @param [in] expire_time  the new absolute tick time at which the timer fires
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_resched_oneshot_timer (aosl_timer_t timer_id, aosl_ts_t expire_time);

/**
 * @brief Cancel a timer, making it inactive. The timer can be rescheduled later.
 * @param [in] timer_id  the timer id
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_cancel_timer (aosl_timer_t timer_id);

/**
 * @brief Get the N-th argument of the timer object specified by timer_id.
 * Parameters:
 *  timer_id: the timer id you want to retrieve the arg
 *         n: which argument you want to get, the first arg is 0;
 *     arg_p: the argument variable address to save the argument value;
 * Return value:
 *        <0: error occured, and errno indicates which error;
 *         0: call successful, and '*arg' is value of the N-th argument;
 **/
extern __aosl_api__ int aosl_mpq_timer_arg (aosl_timer_t timer_id, uintptr_t n, uintptr_t *arg_p);

/**
 * @brief Destroy a timer and invoke its destructor.
 * @param [in] timer_id  the timer id
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_kill_timer (aosl_timer_t timer_id);



#ifdef __cplusplus
}
#endif


#endif /* __MPQ_TIMER_H__ */
