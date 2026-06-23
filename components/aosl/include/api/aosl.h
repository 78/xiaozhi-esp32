/***************************************************************************
 * Module:	AOSL common header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_API_H__
#define __AOSL_API_H__


#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_atomic.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>
#include <api/aosl_time.h>
#include <api/aosl_mpq_timer.h>
#include <api/aosl_mpq_fd.h>
#include <api/aosl_mpq.h>
#include <api/aosl_mpqp.h>

#include <api/aosl_psb.h>
#include <api/aosl_mpq_net.h>
#include <api/aosl_route.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the AOSL library. Must be called before using any AOSL API.
 **/
extern __aosl_api__ void aosl_ctor (void);

/**
 * Finalize the AOSL library and release all resources.
 * Must be called when AOSL is no longer needed.
 **/
extern __aosl_api__ void aosl_dtor (void);

#ifdef __cplusplus
}
#endif



#endif /* __AOSL_API_H__ */