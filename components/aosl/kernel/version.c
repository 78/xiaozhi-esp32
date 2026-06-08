/***************************************************************************
 * Module:	Agora SD-RTN RTC SDK version implementations.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <kernel/kernel.h>
#include <api/aosl_version.h>

#ifndef AOSL_GIT_BRANCH
#define AOSL_GIT_BRANCH "Unknown_Branch"
#endif

#ifndef AOSL_GIT_COMMIT
#define AOSL_GIT_COMMIT "Unknown_Commit"
#endif

__export_in_so__ const char *aosl_get_git_branch (void)
{
	return AOSL_GIT_BRANCH;
}

__export_in_so__ const char *aosl_get_git_commit (void)
{
	return AOSL_GIT_COMMIT;
}