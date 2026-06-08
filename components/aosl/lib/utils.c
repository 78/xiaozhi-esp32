/***************************************************************************
 * Module:	OS relative utilities implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <hal/aosl_hal_utils.h>
#include <api/aosl_types.h>
#include <api/aosl_defs.h>

__export_in_so__ int aosl_get_uuid (char buf [], size_t buf_sz)
{
	return aosl_hal_get_uuid(buf, (int)buf_sz);
}

__export_in_so__ int aosl_os_version (char buf [], size_t buf_sz)
{
	return aosl_hal_os_version(buf, (int)buf_sz);
}

__export_in_so__ int aosl_hwrng_available (void)
{
#if AOSL_HAL_HAVE_HWRNG
	return 1;
#else
	return 0;
#endif
}

__export_in_so__ int aosl_rand_bytes (void *buf, size_t len)
{
#if AOSL_HAL_HAVE_HWRNG
	return aosl_hal_rand_bytes(buf, (int)len);
#else
	return -1;
#endif
}
