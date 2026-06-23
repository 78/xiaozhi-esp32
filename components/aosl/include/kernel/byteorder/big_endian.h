/***************************************************************************
 * Module:	big endian
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef _UAPI__KERNEL_BYTEORDER_BIG_ENDIAN_H
#define _UAPI__KERNEL_BYTEORDER_BIG_ENDIAN_H

#ifndef __K_BIG_ENDIAN
#define __K_BIG_ENDIAN 4321
#endif
#ifndef __K_BIG_ENDIAN_BITFIELD
#define __K_BIG_ENDIAN_BITFIELD
#endif

#include <kernel/types.h>
#include <kernel/swab.h>

#define aosl__cpu_to_le64(x) (aosl__swab64((x)))
#define aosl__le64_to_cpu(x) aosl__swab64((uint64_t)(x))
#define aosl__cpu_to_le32(x) (aosl__swab32((x)))
#define aosl__le32_to_cpu(x) aosl__swab32((uint32_t)(x))
#define aosl__cpu_to_le16(x) (aosl__swab16((x)))
#define aosl__le16_to_cpu(x) aosl__swab16((uint16_t)(x))

#define aosl__cpu_to_be64(x) ((uint64_t)(x))
#define aosl__be64_to_cpu(x) ((uint64_t)(x))
#define aosl__cpu_to_be32(x) ((uint32_t)(x))
#define aosl__be32_to_cpu(x) ((uint32_t)(x))
#define aosl__cpu_to_be16(x) ((uint16_t)(x))
#define aosl__be16_to_cpu(x) ((uint16_t)(x))

#define __K_BYTE_ORDER __K_BIG_ENDIAN


#endif /* _UAPI__KERNEL_BYTEORDER_BIG_ENDIAN_H */
