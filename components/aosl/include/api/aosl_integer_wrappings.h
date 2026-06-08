/***************************************************************************
 * Module:	AOSL integer wrapping processing definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_INTEGER_WRAPPINGS_H__
#define __AOSL_INTEGER_WRAPPINGS_H__

#include <api/aosl_types.h>


#ifdef __cplusplus
extern "C" {
#endif


#define aosl_uint8_add(a, b) ((uint8_t)((a) + (b)))
#define aosl_uint16_add(a, b) ((uint16_t)((a) + (b)))
#define aosl_uint32_add(a, b) ((uint32_t)((a) + (b)))
#define aosl_uint64_add(a, b) ((uint64_t)((a) + (b)))

#define aosl_uint8_sub(a, b) ((uint8_t)((a) - (b)))
#define aosl_uint16_sub(a, b) ((uint16_t)((a) - (b)))
#define aosl_uint32_sub(a, b) ((uint32_t)((a) - (b)))
#define aosl_uint64_sub(a, b) ((uint64_t)((a) - (b)))


static __inline__ int aosl_uint16_after (uint16_t a, uint16_t b)
{
	return (int)((int16_t)(a - b) > 0);
}
#define aosl_uint16_before(a, b) aosl_uint16_after (b, a)

static __inline__ int aosl_uint16_after_eq (uint16_t a, uint16_t b)
{
	return (int)((int16_t)(a - b) >= 0);
}
#define aosl_uint16_before_eq(a, b) aosl_uint16_after_eq (b, a)

#define aosl_uint16_in_range(x, a, b) (aosl_uint16_after_eq (x, a) && aosl_uint16_before_eq (x, b))

static __inline__ uint16_t aosl_uint16_prev (uint16_t val)
{
	return (uint16_t)(val - 1);
}

static __inline__ uint16_t aosl_uint16_next (uint16_t val)
{
	return (uint16_t)(val + 1);
}

static __inline__ uint16_t aosl_uint16_dist (uint16_t a, uint16_t b)
{
	if (aosl_uint16_after (a, b))
		return (uint16_t)(a - b);

	return (uint16_t)(b - a);
}

static __inline__ int aosl_uint32_after (uint32_t a, uint32_t b)
{
	return (int)((int32_t)(a - b) > 0);
}
#define aosl_uint32_before(a, b) aosl_uint32_after (b, a)

static __inline__ int aosl_uint32_after_eq (uint32_t a, uint32_t b)
{
	return (int)((int32_t)(a - b) >= 0);
}
#define aosl_uint32_before_eq(a, b) aosl_uint32_after_eq (b, a)

#define aosl_uint32_in_range(x, a, b) (aosl_uint32_after_eq (x, a) && aosl_uint32_before_eq (x, b))

static __inline__ uint32_t aosl_uint32_prev (uint32_t val)
{
	return (uint32_t)(val - 1);
}

static __inline__ uint32_t aosl_uint32_next (uint32_t val)
{
	return (uint32_t)(val + 1);
}

static __inline__ uint32_t aosl_uint32_dist (uint32_t a, uint32_t b)
{
	if (aosl_uint32_after (a, b))
		return (uint32_t)(a - b);

	return (uint32_t)(b - a);
}

static __inline__ int aosl_uint64_after (uint64_t a, uint64_t b)
{
	return (int)((int64_t)(a - b) > 0);
}
#define aosl_uint64_before(a, b) aosl_uint64_after (b, a)

static __inline__ int aosl_uint64_after_eq (uint64_t a, uint64_t b)
{
	return (int)((int64_t)(a - b) >= 0);
}
#define aosl_uint64_before_eq(a, b) aosl_uint64_after_eq (b, a)

#define aosl_uint64_in_range(x, a, b) (aosl_uint64_after_eq (x, a) && aosl_uint64_before_eq (x, b))

static __inline__ uint64_t aosl_uint64_prev (uint64_t val)
{
	return (uint64_t)(val - 1);
}

static __inline__ uint64_t aosl_uint64_next (uint64_t val)
{
	return (uint64_t)(val + 1);
}

static __inline__ uint64_t aosl_uint64_dist (uint64_t a, uint64_t b)
{
	if (aosl_uint64_after (a, b))
		return (uint64_t)(a - b);

	return (uint64_t)(b - a);
}


#if INTPTR_MAX == INT64_MAX
#define aosl_uintptr_add(a, b) aosl_uint64_add (a, b)
#define aosl_uintptr_sub(a, b) aosl_uint64_sub (a, b)
#define aosl_uintptr_after(a, b) aosl_uint64_after (a, b)
#define aosl_uintptr_after_eq(a, b) aosl_uint64_after_eq (a, b)
#define aosl_uintptr_before(a, b) aosl_uint64_before (a, b)
#define aosl_uintptr_before_eq(a, b) aosl_uint64_before_eq (a, b)
#define aosl_uintptr_in_range(x, a, b) aosl_uint64_in_range (x, a, b)
#define aosl_uintptr_prev(val) aosl_uint64_prev (val)
#define aosl_uintptr_next(val) aosl_uint64_next (val)
#define aosl_uintptr_dist(a, b) aosl_uint64_dist (a, b)
#else
#define aosl_uintptr_add(a, b) aosl_uint32_add (a, b)
#define aosl_uintptr_sub(a, b) aosl_uint32_sub (a, b)
#define aosl_uintptr_after(a, b) aosl_uint32_after (a, b)
#define aosl_uintptr_after_eq(a, b) aosl_uint32_after_eq (a, b)
#define aosl_uintptr_before(a, b) aosl_uint32_before (a, b)
#define aosl_uintptr_before_eq(a, b) aosl_uint32_before_eq (a, b)
#define aosl_uintptr_in_range(x, a, b) aosl_uint32_in_range (x, a, b)
#define aosl_uintptr_prev(val) aosl_uint32_prev (val)
#define aosl_uintptr_next(val) aosl_uint32_next (val)
#define aosl_uintptr_dist(a, b) aosl_uint32_dist (a, b)
#endif


#ifdef __cplusplus
}
#endif


#endif /* __AOSL_INTEGER_WRAPPINGS_H__ */