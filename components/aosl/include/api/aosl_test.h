/***************************************************************************
 * Module:	AOSL test header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_TEST_H__
#define __AOSL_TEST_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the AOSL built-in self-test suite.
 **/
extern __aosl_api__ void aosl_test (void);

#ifdef __cplusplus
}
#endif



#endif /* __AOSL_TEST_H__ */