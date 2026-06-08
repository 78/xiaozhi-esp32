/***************************************************************************
 * Module:	generic endian
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __KERNEL_BYTEORDER_GENERIC_H
#define __KERNEL_BYTEORDER_GENERIC_H

#include <api/aosl_types.h>

// Determine endianness
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)

		#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
				#define COMPILE_TIME_LITTLE_ENDIAN 1
				#define COMPILE_TIME_BIG_ENDIAN 0
		#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
				#define COMPILE_TIME_LITTLE_ENDIAN 0
				#define COMPILE_TIME_BIG_ENDIAN 1
		#else
				#define COMPILE_TIME_LITTLE_ENDIAN 0
				#define COMPILE_TIME_BIG_ENDIAN 0
		#endif

#elif defined(__LITTLE_ENDIAN__) || defined(_LITTLE_ENDIAN) || \
			defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
			defined(__MIPSEL__) || defined(__MIPSEL) || defined(_MIPSEL) || \
			defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_IX86) || \
			defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)

		// Specific macros for little-endian architectures
		#define COMPILE_TIME_LITTLE_ENDIAN 1
		#define COMPILE_TIME_BIG_ENDIAN 0

#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN) || \
			defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
			defined(__MIPSEB__) || defined(__MIPSEB) || defined(_MIPSEB) || \
			defined(__powerpc__) || defined(__POWERPC__) || defined(_M_PPC) || \
			defined(__sparc__) || defined(__sparc)

		// Specific macros for big-endian architectures
		#define COMPILE_TIME_LITTLE_ENDIAN 0
		#define COMPILE_TIME_BIG_ENDIAN 1

#else
#error "Can't recognize cpu byte order"
#endif

#if COMPILE_TIME_LITTLE_ENDIAN
#include <kernel/byteorder/little_endian.h>
#elif COMPILE_TIME_BIG_ENDIAN
#include <kernel/byteorder/big_endian.h>
#else
#error "Can't get endian type"
#endif

/*
 * $/include/kernel/byteorder/generic.h
 * Generic Byte-reordering support
 *
 * The "... p" macros, like le64_to_cpup, can be used with pointers
 * to unaligned data, but there will be a performance penalty on
 * some architectures.  Use get_unaligned for unaligned data.
 *
 * Francois-Rene Rideau <fare@tunes.org> 19970707
 *    gathered all the good ideas from all asm-foo/byteorder.h into one file,
 *    cleaned them up.
 *    I hope it is compliant with non-GCC compilers.
 *    I decided to put __BYTEORDER_HAS_U64__ in byteorder.h,
 *    because I wasn't sure it would be ok to put it in types.h
 *    Upgraded it to 2.1.43
 * Francois-Rene Rideau <fare@tunes.org> 19971012
 *    Upgraded it to 2.1.57
 *    to please Linus T., replaced huge #ifdef's between little/big endian
 *    by nestedly #include'd files.
 * Francois-Rene Rideau <fare@tunes.org> 19971205
 *    Made it to 2.1.71; now a facelift:
 *    Put files under $/include/kernel/byteorder/
 *    Split swab from generic support.
 *
 * TODO:
 *   = Regular kernel maintainers could also replace all these manual
 *    byteswap macros that remain, disseminated among drivers,
 *    after some grep or the sources...
 *   = Linus might want to rename all these macros and files to fit his taste,
 *    to fit his personal naming scheme.
 *   = it seems that a few drivers would also appreciate
 *    nybble swapping support...
 *   = every architecture could add their byteswap macro in asm/byteorder.h
 *    see how some architectures already do (i386, alpha, ppc, etc)
 *   = cpu_to_beXX and beXX_to_cpu might some day need to be well
 *    distinguished throughout the kernel. This is not the case currently,
 *    since little endian, big endian, and pdp endian machines needn't it.
 *    But this might be the case for, say, a port of Linux to 20/21 bit
 *    architectures (and F21 Linux addict around?).
 */

/*
 * The following macros are to be defined by <asm/byteorder.h>:
 *
 * Conversion of long and short int between network and host format
 *	ntohl(uint32_t x)
 *	ntohs(uint16_t x)
 *	htonl(uint32_t x)
 *	htons(uint16_t x)
 * It seems that some programs (which? where? or perhaps a standard? POSIX?)
 * might like the above to be functions, not macros (why?).
 * if that's true, then detect them, and take measures.
 * Anyway, the measure is: define only ___ntohl as a macro instead,
 * and in a separate file, have
 * uintptr_t inline ntohl(x){return ___ntohl(x);}
 *
 * The same for constant arguments
 *	__constant_ntohl(uint32_t x)
 *	__constant_ntohs(uint16_t x)
 *	__constant_htonl(uint32_t x)
 *	__constant_htons(uint16_t x)
 *
 * Conversion of XX-bit integers (16- 32- or 64-)
 * between native CPU format and little/big endian format
 * 64-bit stuff only defined for proper architectures
 *	cpu_to_[bl]eXX(__uXX x)
 *	[bl]eXX_to_cpu(__uXX x)
 *
 * The same, but takes a pointer to the value to convert
 *	cpu_to_[bl]eXXp(__uXX x)
 *	[bl]eXX_to_cpup(__uXX x)
 *
 * The same, but change in situ
 *	cpu_to_[bl]eXXs(__uXX x)
 *	[bl]eXX_to_cpus(__uXX x)
 *
 * See asm-foo/byteorder.h for examples of how to provide
 * architecture-optimized versions
 *
 */
#define aosl_cpu_to_le64 aosl__cpu_to_le64
#define aosl_le64_to_cpu aosl__le64_to_cpu
#define aosl_cpu_to_le32 aosl__cpu_to_le32
#define aosl_le32_to_cpu aosl__le32_to_cpu
#define aosl_cpu_to_le16 aosl__cpu_to_le16
#define aosl_le16_to_cpu aosl__le16_to_cpu

#define aosl_cpu_to_be64 aosl__cpu_to_be64
#define aosl_be64_to_cpu aosl__be64_to_cpu
#define aosl_cpu_to_be32 aosl__cpu_to_be32
#define aosl_be32_to_cpu aosl__be32_to_cpu
#define aosl_cpu_to_be16 aosl__cpu_to_be16
#define aosl_be16_to_cpu aosl__be16_to_cpu

/*
 * They have to be macros in order to do the constant folding
 * correctly - if the argument passed into a inline function
 * it is no longer constant according to gcc..
 */
#define aosl__htonl(x) aosl_cpu_to_be32(x)
#define aosl__htons(x) aosl_cpu_to_be16(x)
#define aosl__ntohl(x) aosl_be32_to_cpu(x)
#define aosl__ntohs(x) aosl_be16_to_cpu(x)

#endif /* __KERNEL_BYTEORDER_GENERIC_H */
