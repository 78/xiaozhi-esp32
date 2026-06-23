/***************************************************************************
 * Module:	swab header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef _UAPI_KERNEL_SWAB_H
#define _UAPI_KERNEL_SWAB_H

#include <kernel/types.h>
#include <kernel/compiler.h>

#define aosl___constant_swab16(x) ((uint16_t)(				\
	(((uint16_t)(x) & (uint16_t)0x00ffU) << 8) |			\
	(((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))

#define aosl___constant_swab32(x) ((uint32_t)(				\
	(((uint32_t)(x) & (uint32_t)0x000000ff) << 24) |		\
	(((uint32_t)(x) & (uint32_t)0x0000ff00) <<  8) |		\
	(((uint32_t)(x) & (uint32_t)0x00ff0000) >>  8) |		\
	(((uint32_t)(x) & (uint32_t)0xff000000) >> 24)))

#define aosl___constant_swab64(x) ((uint64_t)(				\
	(((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) |	\
	(((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) |	\
	(((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) |	\
	(((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) <<  8) |	\
	(((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >>  8) |	\
	(((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) |	\
	(((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) |	\
	(((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56)))

static inline uint16_t aosl__fswab16(uint16_t val)
{
	return aosl___constant_swab16(val);

}

static inline uint32_t aosl__fswab32(uint32_t val)
{
	return aosl___constant_swab32(val);
}

static inline uint64_t aosl__fswab64(uint64_t val)
{
	return aosl___constant_swab64(val);
}

#define aosl__swab16(x)	aosl__fswab16(x)

#define aosl__swab32(x)	aosl__fswab32(x)

#define aosl__swab64(x)	aosl__fswab64(x)

static inline uint16_t aosl__swab16p(const uint16_t *p)
{
	return aosl__swab16(*p);
}

static inline uint32_t aosl__swab32p(const uint32_t *p)
{
	return aosl__swab32(*p);
}

static inline uint64_t aosl__swab64p(const uint64_t *p)
{
	return aosl__swab64(*p);
}

static inline void aosl__swab16s(uint16_t *p)
{
	*p = aosl__swab16p(p);
}

static inline void aosl__swab32s(uint32_t *p)
{
	*p = aosl__swab32p(p);
}

static inline void aosl__swab64s(uint64_t *p)
{
	*p = aosl__swab64p(p);
}


#define aosl_swab16 aosl__swab16
#define aosl_swab32 aosl__swab32
#define aosl_swab64 aosl__swab64
#define aosl_swab16p aosl__swab16p
#define aosl_swab32p aosl__swab32p
#define aosl_swab64p aosl__swab64p
#define aosl_swab16s aosl__swab16s
#define aosl_swab32s aosl__swab32s
#define aosl_swab64s aosl__swab64s


static __inline__ void aosl__bswap_mem (void *dst, const void *src, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++)
		((char *)dst) [i] = ((const char *)src) [len - i - 1];
}

#endif /* _UAPI_KERNEL_SWAB_H */