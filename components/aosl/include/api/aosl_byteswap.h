/***************************************************************************
 * Module		:		AOSL byteswap header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_BYTESWAP_H__
#define __AOSL_BYTESWAP_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>

/**
 * @brief Swap the byte order of a 32-bit unsigned integer.
 * @param [in] v  the 32-bit value to byte-swap
 * @return        the byte-swapped value
 **/
extern __aosl_api__ uint32_t aosl_bswap_32 (uint32_t v);

/**
 * @brief Swap the byte order of a 64-bit unsigned integer.
 * @param [in] v  the 64-bit value to byte-swap
 * @return        the byte-swapped value
 **/
extern __aosl_api__ uint64_t aosl_bswap_64 (uint64_t v);

#endif /* __AOSL_BYTESWAP_H__ */