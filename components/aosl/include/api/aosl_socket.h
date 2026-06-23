/***************************************************************************
 * Module:	AOSL BSD socket definitions header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_SOCKET_H__
#define __AOSL_SOCKET_H__

#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <hal/aosl_hal_socket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aosl_in_addr {
	uint32_t s_addr;
} aosl_in_addr_t;

typedef struct aosl_in6_addr {
	union {
		uint8_t u6_addr8[16];
		uint16_t u6_addr16[8];
		uint32_t u6_addr32[4];
	} u6_addr;
} aosl_in6_addr_t;

#define s_addr_v    s_addr
#define	s6_addr8_v  u6_addr.u6_addr8
#define	s6_addr16_v u6_addr.u6_addr16
#define	s6_addr32_v u6_addr.u6_addr32
#define	s6_addr_v   u6_addr.u6_addr8

typedef struct aosl_sockaddr_in {
	uint16_t sin_family;
	uint16_t sin_port;
	aosl_in_addr_t sin_addr;
	unsigned char sin_zero[8];
} aosl_sockaddr_in_t;

typedef struct aosl_sockaddr_in6 {
	uint16_t sin6_family;
	uint16_t sin6_port;
	uint32_t sin6_flowinfo;
	aosl_in6_addr_t sin6_addr;
	uint32_t sin6_scope_id;
} aosl_sockaddr_in6_t;

/**
typedef struct aosl_sockaddr {
  uint16_t sa_family;
  char     sa_data[14];
} aosl_sockaddr_t;
*/

typedef int aosl_socklen_t;

#define AOSL_INET_ADDRSTRLEN  16
#define AOSL_INET6_ADDRSTRLEN 46
#define AOSL_MAX_ADDRSTRLEN   AOSL_INET6_ADDRSTRLEN

/**
 * @brief Convert a 32-bit integer from host byte order to network byte order.
 * @param [in] x  the 32-bit value in host byte order
 * @return        the value in network byte order (big-endian)
 **/
extern __aosl_api__ uint32_t aosl_htonl(uint32_t x);

/**
 * @brief Convert a 16-bit integer from host byte order to network byte order.
 * @param [in] x  the 16-bit value in host byte order
 * @return        the value in network byte order (big-endian)
 **/
extern __aosl_api__ uint16_t aosl_htons(uint16_t x);

/**
 * @brief Convert a 32-bit integer from network byte order to host byte order.
 * @param [in] x  the 32-bit value in network byte order
 * @return        the value in host byte order
 **/
extern __aosl_api__ uint32_t aosl_ntohl(uint32_t x);

/**
 * @brief Convert a 16-bit integer from network byte order to host byte order.
 * @param [in] x  the 16-bit value in network byte order
 * @return        the value in host byte order
 **/
extern __aosl_api__ uint16_t aosl_ntohs(uint16_t x);

#ifdef __cplusplus
}
#endif

#endif /* __AOSL_SOCKET_H__ */