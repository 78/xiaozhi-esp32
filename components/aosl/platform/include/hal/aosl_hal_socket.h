/***************************************************************************
 * Module:	socket hal definitions.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef __AOSL_HAL_SOCKET_H__
#define __AOSL_HAL_SOCKET_H__

#include <stdlib.h>
#include <stdint.h>
#include <hal/aosl_hal_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**** socket(domain, type, protocol) ****/

// socket domain, address/protocol family
enum aosl_socket_domain {
  AOSL_AF_UNSPEC = 0,
  AOSL_AF_INET   = 2,
  AOSL_AF_INET6  = 10,
};

// socket type
enum aosl_socket_type {
  AOSL_SOCK_STREAM = 1,
  AOSL_SOCK_DGRAM  = 2,
};

// socket protocol
enum aosl_socket_proto {
  AOSL_IPPROTO_AUTO = 0,
  AOSL_IPPROTO_TCP = 1,
  AOSL_IPPROTO_UDP = 2,
};

// socket address
typedef struct aosl_sockaddr {
  uint16_t sa_family;  // aosl_socket_domain
  uint16_t sa_port;
  union {
    uint32_t sin_addr;
    uint32_t sin6_flowinfo;
  };
  uint8_t  sin6_addr[16];
  uint32_t sin6_scope_id;
} aosl_sockaddr_t;

/**
 * @brief create a socket
 * @param [in] domain address/protocol family
 * @param [in] type socket type
 * @param [in] protocol socket protocol
 * @return socket file descriptor, or AOSL_INVALID_FD on error
 */
aosl_fd_t aosl_hal_sk_socket(enum aosl_socket_domain domain,
                             enum aosl_socket_type type,
                             enum aosl_socket_proto protocol);
/**
 * @brief   bind a socket
 * @param [in] sockfd socket file descriptor
 * @param [in] addr address to bind to
 * @return 0 on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_bind(aosl_fd_t sockfd, const aosl_sockaddr_t* addr);

/**
 * @brief   bind a socket to a specific network interface
 * @param [in] sockfd socket file descriptor
 * @param [in] if_name name of the network interface
 * @return 0 on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_bind_device(aosl_fd_t sockfd, const char *if_name);

/**
 * @brief   listen for incoming connections
 * @param [in] sockfd socket file descriptor
 * @param [in] backlog maximum length of the pending connections queue
 * @return 0 on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_listen(aosl_fd_t sockfd, int backlog);

/**
 * @brief   accept an incoming connection
 * @param [in] sockfd socket file descriptor
 * @param [out] addr address of the connecting peer
 * @return socket file descriptor on success, AOSL_INVALID_FD on error
 */
aosl_fd_t aosl_hal_sk_accept(aosl_fd_t sockfd, aosl_sockaddr_t *addr);

/**
 * @brief   connect to a remote address
 * @param [in] sockfd socket file descriptor
 * @param [in] addr address to connect to
 * @return 0 on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_connect(aosl_fd_t sockfd, const aosl_sockaddr_t *addr);

/**
 * @brief   close a socket
 * @param [in] sockfd socket file descriptor
 * @return 0 on success, < 0 on error
 */
int aosl_hal_sk_close(aosl_fd_t sockfd);

/**
 * @brief   send data on a socket
 * @param [in] sockfd socket file descriptor
 * @param [in] buf buffer containing data to send
 * @param [in] len length of data to send
 * @param [in] flags flags for sending data
 * @return number of bytes sent on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_send(aosl_fd_t sockfd, const void* buf, size_t len, int flags);

/**
 * @brief   receive data from a socket
 * @param [in] sockfd socket file descriptor
 * @param [out] buf buffer to receive data into
 * @param [in] len length of buffer
 * @param [in] flags flags for receiving data
 * @return number of bytes received on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_recv(aosl_fd_t sockfd, void* buf, size_t len, int flags);

/**
 * @brief   send data to a specific address
 * @param [in] sockfd socket file descriptor
 * @param [in] buffer buffer containing data to send
 * @param [in] length length of data to send
 * @param [in] flags flags for sending data
 * @param [in] dest_addr destination address to send data to
 * @return number of bytes sent on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_sendto(aosl_fd_t sockfd, const void *buffer, size_t length,
                       int flags, const aosl_sockaddr_t *dest_addr);

/**
 * @brief   receive data from a specific address
 * @param [in] sockfd socket file descriptor
 * @param [out] buffer buffer to receive data into
 * @param [in] length length of buffer
 * @param [in] flags flags for receiving data
 * @param [out] src_addr source address of the received data
 * @return number of bytes received on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_recvfrom(aosl_fd_t sockfd, void *buffer, size_t length,
                         int flags, aosl_sockaddr_t *src_addr);

/**
 * @brief   read data from a socket
 * @param [in] sockfd socket file descriptor
 * @param [out] buf buffer to receive data into
 * @param [in] count number of bytes to read
 * @return number of bytes read on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_read(aosl_fd_t sockfd, void *buf, size_t count);

/**
 * @brief   write data to a socket
 * @param [in] sockfd socket file descriptor
 * @param [in] buf buffer containing data to write
 * @param [in] count number of bytes to write
 * @return number of bytes written on success, < 0 on error. should use aosl_hal_errno_convert to get error code
 */
int aosl_hal_sk_write(aosl_fd_t sockfd, const void *buf, size_t count);

/**
 * @brief   set a socket to non-blocking mode
 * @param [in] sockfd socket file descriptor
 * @return 0 on success, < 0 on error
 */
int aosl_hal_sk_set_nonblock(aosl_fd_t sockfd);

/**
 * @brief   get the local IP address of the default network interface
 * @param [out] addr pointer to aosl_sockaddr_t to store the local IP address
 * @return 0 on success, < 0 on error
 */
int aosl_hal_sk_get_local_ip(aosl_sockaddr_t *addr);

/**
 * @brief   get the local address of a socket
 * @param [in] sockfd socket file descriptor
 * @param [out] addr pointer to aosl_sockaddr_t to store the local address
 * @return 0 on success, < 0 on error
 */
int aosl_hal_sk_get_sockname(aosl_fd_t sockfd, aosl_sockaddr_t *addr);

/**
 * @brief   resolve a hostname to IP addresses
 * @param [in] hostname the hostname to resolve
 * @param [out] addrs array to store resolved addresses
 * @param [in] addr_count size of the addrs array
 * @return number of addresses resolved, or 0 on error
 */
int aosl_hal_gethostbyname(const char *hostname, aosl_sockaddr_t *addrs, int addr_count);

#ifdef __cplusplus
}
#endif

#endif /* __AOSL_HAL_SOCKET_H__ */
