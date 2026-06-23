/***************************************************************************
 * Module:	AOSL reference object definition file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_REF_H__
#define __AOSL_REF_H__


#include <api/aosl_types.h>
#include <api/aosl_defs.h>


#ifdef __cplusplus
extern "C" {
#endif



typedef int aosl_ref_t;

#define AOSL_REF_INVALID ((aosl_ref_t)-1)

/** @brief Check if a ref id is invalid. */
#define aosl_ref_invalid(ref) (((int16_t)(ref)) < 0)


/**
 * @brief The reference object destructor function prototype, which invoked when application
 * calling aosl_ref_destroy functions to release resources.
 * Parameter:
 *       arg: the parameter passed in when creating the reference object;
 * Return value:
 *       none.
 **/
typedef void (*aosl_ref_dtor_t) (void *arg);

/**
 * @brief The reference object creating function prototype, which is used to create a ref object.
 * Parameters:
 *              arg: the parameter attached with the reference object;
 *             dtor: the ref object destructor function, which will be invoked when
 *                   the ref object is deleted;
 *     modify_async:
 *            none-0 for async modifying ref type;
 *                 0 for sync modifying ref type;
 *        recursive:
 *            none-0 recursive ref type, which indicates you can read lock more than once
 *                   in one thread context;
 *                 0 for none recursive ref type, you can only read lock once in one thread
 *                   context, otherwise system would abort;
 *      caller_free:
 *            none-0 guarantee the ref object relatives must be freed in the caller thread
 *                 0 the ref object relatives could be freed in any thread
 * Return value:
 *         the ref object id, please use aosl_ref_invalid macro to check whether failed.
 **/
extern __aosl_api__ aosl_ref_t aosl_ref_create (void *arg, aosl_ref_dtor_t dtor, int modify_async, int recursive, int caller_free);


/**
 * @brief The ref object callback function prototype.
 * Parameter:
 *            arg: the ref object argument which was passed in when creating;
 *           argc: specify the argv array elements count, the same as the argc
 *                    when invoking aosl_ref_[get|read|write] functions;
 *           argv: array for passing variable args, the same as the args
 *                    when invoking aosl_task_exec_* functions;
 * Return value:
 *       none.
 **/
typedef void (*aosl_ref_func_t) (void *arg, uintptr_t argc, uintptr_t argv []);

/**
 * @brief Hold the ref object, and invoke the specified callback function.
 * Parameter:
 *            ref: the ref object id;
 *              f: the callback function;
 *           argc: the args count
 *            ...: variable args
 * Return value:
 *            0: success
 *           <0: failure with errno set
 **/
extern __aosl_api__ int aosl_ref_hold (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...);
extern __aosl_api__ int aosl_ref_hold_args (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, va_list args);
extern __aosl_api__ int aosl_ref_hold_argv (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv []);

/**
 * @brief Hold the ref object and read lock it, then invoke the specified callback function.
 * Parameter:
 *            ref: the ref object id;
 *              f: the callback function;
 *           argc: the args count
 *            ...: variable args
 * Return value:
 *            0: success
 *           <0: failure with errno set
 **/
extern __aosl_api__ int aosl_ref_read (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...);
extern __aosl_api__ int aosl_ref_read_args (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, va_list args);
extern __aosl_api__ int aosl_ref_read_argv (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv []);

/**
 * @brief Hold the ref object and write lock it, then invoke the specified callback function.
 * Parameter:
 *            ref: the ref object id;
 *              f: the callback function;
 *           argc: the args count
 *            ...: variable args
 * Return value:
 *            0: success
 *           <0: failure with errno set
 **/
extern __aosl_api__ int aosl_ref_write (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...);
extern __aosl_api__ int aosl_ref_write_args (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, va_list args);
extern __aosl_api__ int aosl_ref_write_argv (aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv []);


typedef void *aosl_refobj_t;

#define AOSL_FREE_ONLY_OBJ ((aosl_refobj_t)(uintptr_t)1)
/** @brief Check if a refobj is the free-only sentinel value. */
#define aosl_is_free_only(robj) ((int)((aosl_refobj_t)(robj) == AOSL_FREE_ONLY_OBJ))

/**
 * @brief Retrieve the ref object arg.
 * Parameters:
 *     robj: the reference object;
 * Return value:
 *     the ref object arg;
 **/
extern __aosl_api__ void *aosl_refobj_arg (aosl_refobj_t robj);

/**
 * @brief Get the ref id of the specified ref object.
 * Parameters:
 *      robj: the reference object;
 * Return value:
 *      the ref id.
 **/
extern __aosl_api__ aosl_ref_t aosl_refobj_id (aosl_refobj_t robj);

/**
 * @brief Make sure read lock the ref object specified by robj, then invoke the specified callback function.
 * Parameter:
 *           robj: the ref object itself;
 *              f: the callback function;
 *           argc: the args count
 *            ...: variable args
 * Return value:
 *            0: success
 *           <0: failure with errno set
 **/
extern __aosl_api__ int aosl_refobj_read (aosl_refobj_t robj, aosl_ref_func_t f, uintptr_t argc, ...);
extern __aosl_api__ int aosl_refobj_read_args (aosl_refobj_t robj, aosl_ref_func_t f, uintptr_t argc, va_list args);
extern __aosl_api__ int aosl_refobj_read_argv (aosl_refobj_t robj, aosl_ref_func_t f, uintptr_t argc, uintptr_t argv []);


/**
 * @brief Destroy the reference object specified by ref.
 * Parameter:
 *         ref: the reference object id
 *   do_delete: 0 for just marking it destroyed
 *              non-0 value for deleting it
 * Return value:
 *        0: success
 *       <0: failed, and errno indicates what error occurs
 **/
extern __aosl_api__ int aosl_ref_destroy (aosl_ref_t ref, int do_delete);



#ifdef __cplusplus
}
#endif



#endif /* __AOSL_REF_H__ */