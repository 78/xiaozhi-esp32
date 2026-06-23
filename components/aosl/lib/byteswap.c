/***************************************************************************
 * Module:	bswap implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_byteswap.h>
#include <kernel/swab.h>

__export_in_so__ uint32_t aosl_bswap_32 (uint32_t v)
{
	return aosl_swab32 (v);
}

__export_in_so__ uint64_t aosl_bswap_64 (uint64_t v)
{
	return aosl_swab64 (v);
}