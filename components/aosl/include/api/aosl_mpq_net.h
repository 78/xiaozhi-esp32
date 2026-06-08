/***************************************************************************
 * Module:	Socket helper utils header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_MPQ_NET_H__
#define __AOSL_MPQ_NET_H__

#include <api/aosl_types.h>
#include <api/aosl_socket.h>
#include <api/aosl_defs.h>
#include <api/aosl_mpq.h>
#include <api/aosl_mpq_fd.h>


#ifdef __cplusplus
extern "C" {
#endif

#define AOSL_IPHDR_LEN 20
#define AOSL_UDPHDR_LEN 8
#define AOSL_TCPHDR_LEN 20
#define AOSL_IP_UDP_HDR_LEN (AOSL_IPHDR_LEN + AOSL_UDPHDR_LEN)
#define AOSL_IP_TCP_HDR_LEN (AOSL_IPHDR_LEN + AOSL_TCPHDR_LEN)


typedef union {
	aosl_sockaddr_t sa;
	aosl_sockaddr_in_t in;
	aosl_sockaddr_in6_t in6;
} aosl_sk_addr_t;

typedef struct {
	aosl_fd_t newsk;
	aosl_sk_addr_t addr;
} aosl_accept_data_t;

/**
 * @brief The listen state socket readable callback function type
 * Parameters:
 *    data: the data buffer holding the packet
 *     len: the data length in bytes in the buffer
 *    argc: the args count when adding the fd
 *    argv: the args vector when adding the fd
 *    addr: the socket address data received from
 * Return value:
 *    None.
 **/
typedef void (*aosl_sk_accepted_t) (aosl_accept_data_t *accept_data, size_t len, uintptr_t argc, uintptr_t argv []);

/**
 * @brief The dgram socket received data callback function type
 * Parameters:
 *    data: the data buffer holding the packet
 *     len: the data length in bytes in the buffer
 *    argc: the args count when adding the fd
 *    argv: the args vector when adding the fd
 *    addr: the socket address data received from
 * Return value:
 *    None.
 **/
typedef void (*aosl_dgram_sk_data_t) (void *data, size_t len, uintptr_t argc, uintptr_t argv [], const aosl_sk_addr_t *addr);

/**
 * @brief Create a socket file descriptor.
 * @param [in] domain    the address family (e.g. AF_INET, AF_INET6)
 * @param [in] type      the socket type (e.g. SOCK_STREAM, SOCK_DGRAM)
 * @param [in] protocol  the protocol (usually 0)
 * @return               the socket fd, or AOSL_INVALID_FD on failure
 **/
extern __aosl_api__ aosl_fd_t aosl_socket (int domain, int type, int protocol);

/**
 * @brief Bind a socket to a specific address.
 * @param [in] sockfd  the socket fd
 * @param [in] addr    the address to bind to
 * @return             0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_bind (aosl_fd_t sockfd, const aosl_sockaddr_t *addr);

/**
 * @brief Bind a socket to a specific port on all interfaces.
 * @param [in] sockfd  the socket fd
 * @param [in] af      the address family (AF_INET or AF_INET6)
 * @param [in] port    the port number in host byte order
 * @return             0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_bind_port_only (aosl_fd_t sockfd, uint16_t af, unsigned short port);

/**
 * @brief Bind a socket to a specific network interface by name.
 * @param [in] sockfd   the socket fd
 * @param [in] if_name  the network interface name (e.g. "eth0")
 * @return              0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_bind_device (aosl_fd_t sockfd, const char *if_name);
//extern __aosl_api__ int aosl_getsockname (aosl_fd_t sockfd, aosl_sockaddr_t *addr);
//extern __aosl_api__ int aosl_getpeername (aosl_fd_t sockfd, aosl_sockaddr_t *addr);
//extern __aosl_api__ int aosl_getsockopt (aosl_fd_t sockfd, int level, int optname, void *optval, int *optlen);
//extern __aosl_api__ int aosl_setsockopt (aosl_fd_t sockfd, int level, int optname, const void *optval, int optlen);
/**
 * @brief Get the local socket address of a connected or bound socket.
 * @param [in]  sockfd  the raw socket fd
 * @param [out] addr    pointer to receive the socket address
 * @return              0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_get_sockaddr(aosl_fd_t sockfd, aosl_sockaddr_t *addr);

/**
 * @brief Initiate an async TCP connection on the current mpq.
 * @param [in] fd            the socket fd
 * @param [in] dest_addr     the destination address
 * @param [in] timeo         connection timeout in milliseconds
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_connect (aosl_fd_t fd, const aosl_sockaddr_t *dest_addr,
												int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
											aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Initiate an async TCP connection on the specified mpq.
 * @param [in] qid           the target mpq id
 * @param [in] fd            the socket fd
 * @param [in] dest_addr     the destination address
 * @param [in] timeo         connection timeout in milliseconds
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_connect_on_q (aosl_mpq_t qid, aosl_fd_t fd, const aosl_sockaddr_t *dest_addr,
																		int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
																aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Start listening for incoming connections on the current mpq.
 * @param [in] fd          the socket fd (must be bound)
 * @param [in] backlog     the maximum pending connection queue length
 * @param [in] accepted_f  the callback invoked when a new connection is accepted
 * @param [in] event_f     the event notification callback
 * @param [in] argc        the number of variable arguments
 * @param [in] ...         variable arguments passed to callbacks
 * @return                 0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_listen (aosl_fd_t fd, int backlog, aosl_sk_accepted_t accepted_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Start listening for incoming connections on the specified mpq.
 * @param [in] qid         the target mpq id
 * @param [in] fd          the socket fd (must be bound)
 * @param [in] backlog     the maximum pending connection queue length
 * @param [in] accepted_f  the callback invoked when a new connection is accepted
 * @param [in] event_f     the event notification callback
 * @param [in] argc        the number of variable arguments
 * @param [in] ...         variable arguments passed to callbacks
 * @return                 0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_listen_on_q (aosl_mpq_t qid, aosl_fd_t fd, int backlog,
				aosl_sk_accepted_t accepted_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Add a datagram socket to the current mpq for async I/O.
 * @param [in] fd            the datagram socket fd
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] data_f        the data received callback (includes sender address)
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_add_dgram_socket (aosl_fd_t fd, size_t max_pkt_size, aosl_dgram_sk_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Add a datagram socket to the specified mpq for async I/O.
 * @param [in] qid           the target mpq id
 * @param [in] fd            the datagram socket fd
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] data_f        the data received callback (includes sender address)
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_add_dgram_socket_on_q (aosl_mpq_t qid, aosl_fd_t fd, size_t max_pkt_size,
									aosl_dgram_sk_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Add a connected stream socket to the current mpq for async I/O.
 * @param [in] fd            the stream socket fd
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_add_stream_socket (aosl_fd_t fd, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
													aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Add a connected stream socket to the specified mpq for async I/O.
 * @param [in] qid           the target mpq id
 * @param [in] fd            the stream socket fd
 * @param [in] max_pkt_size  the maximum packet size for the read buffer
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_add_stream_socket_on_q (aosl_mpq_t qid, aosl_fd_t fd, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
																	aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Send data on a connected socket.
 * @param [in] sockfd  the socket fd
 * @param [in] buf     the data buffer
 * @param [in] len     the number of bytes to send
 * @param [in] flags   send flags (e.g. MSG_DONTWAIT)
 * @return             the number of bytes sent, or <0 on failure
 **/
extern __aosl_api__ isize_t aosl_send (aosl_fd_t sockfd, const void *buf, size_t len, int flags);

/**
 * @brief Send data to a specific destination address.
 * @param [in] sockfd     the socket fd
 * @param [in] buf        the data buffer
 * @param [in] len        the number of bytes to send
 * @param [in] flags      send flags
 * @param [in] dest_addr  the destination address
 * @return                the number of bytes sent, or <0 on failure
 **/
extern __aosl_api__ isize_t aosl_sendto (aosl_fd_t sockfd, const void *buf, size_t len, int flags, const aosl_sockaddr_t *dest_addr);


typedef struct {
	aosl_fd_t v4;
	aosl_fd_t v6;
} aosl_ip_sk_t;

static __inline__ void aosl_ip_sk_init (aosl_ip_sk_t *sk)
{
	sk->v4 = AOSL_INVALID_FD;
	sk->v6 = AOSL_INVALID_FD;
}

/**
 * @brief Create a dual-stack (IPv4 + IPv6) socket pair.
 * @param [out] sk        pointer to the ip socket pair to initialize
 * @param [in]  type      the socket type (e.g. SOCK_STREAM, SOCK_DGRAM)
 * @param [in]  protocol  the protocol (usually 0)
 * @return                0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_ip_sk_create (aosl_ip_sk_t *sk, int type, int protocol);

typedef struct {
	aosl_sockaddr_in_t v4;
	aosl_sockaddr_in6_t v6;
} aosl_ip_addr_t;

/**
 * @brief Initialize a dual-stack IP address structure to zero.
 * @param [out] addr  pointer to the IP address structure
 **/
extern __aosl_api__ void aosl_ip_addr_init (aosl_ip_addr_t *addr);

/**
 * @brief Bind a dual-stack socket pair to the specified addresses.
 * @param [in] sk    pointer to the IP socket pair
 * @param [in] addr  pointer to the IPv4/IPv6 address pair
 * @return           0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_ip_sk_bind (const aosl_ip_sk_t *sk, const aosl_ip_addr_t *addr);

/**
 * @brief Initiate an async TCP connection using a dual-stack socket pair on the current mpq.
 * The appropriate socket (v4 or v6) is selected based on dest_addr.
 * @param [in] sk            pointer to the IP socket pair
 * @param [in] dest_addr     the destination address
 * @param [in] timeo         connection timeout in milliseconds
 * @param [in] max_pkt_size  the maximum packet size
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_ip_sk_connect (const aosl_ip_sk_t *sk, const aosl_sockaddr_t *dest_addr,
												int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
										aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Initiate an async TCP connection using a dual-stack socket pair on the specified mpq.
 * @param [in] qid           the target mpq id
 * @param [in] sk            pointer to the IP socket pair
 * @param [in] dest_addr     the destination address
 * @param [in] timeo         connection timeout in milliseconds
 * @param [in] max_pkt_size  the maximum packet size
 * @param [in] chk_pkt_f     the packet completeness check callback
 * @param [in] data_f        the data received callback
 * @param [in] event_f       the event notification callback
 * @param [in] argc          the number of variable arguments
 * @param [in] ...           variable arguments passed to callbacks
 * @return                   0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_ip_sk_connect_on_q (aosl_mpq_t qid, const aosl_ip_sk_t *sk,
							const aosl_sockaddr_t *dest_addr, int timeo, size_t max_pkt_size,
				aosl_check_packet_t chk_pkt_f, aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);

/**
 * @brief Send data to a destination using the appropriate socket from a dual-stack pair.
 * @param [in] sk         pointer to the IP socket pair
 * @param [in] buf        the data buffer
 * @param [in] len        the number of bytes to send
 * @param [in] flags      send flags
 * @param [in] dest_addr  the destination address
 * @return                the number of bytes sent, or <0 on failure
 **/
extern __aosl_api__ isize_t aosl_ip_sk_sendto (const aosl_ip_sk_t *sk, const void *buf, size_t len, int flags, const aosl_sockaddr_t *dest_addr);

/**
 * @brief Close both sockets in a dual-stack socket pair.
 * @param [in] sk  pointer to the IP socket pair
 **/
extern __aosl_api__ void aosl_ip_sk_close (aosl_ip_sk_t *sk);

/**
 * @brief Check if an IPv6 address is an IPv4-mapped address (::ffff:x.x.x.x).
 * @param [in] a6  pointer to the IPv6 address
 * @return         non-zero if mapped, 0 otherwise
 **/
extern __aosl_api__ int aosl_ipv6_addr_v4_mapped (const aosl_in6_addr_t *a6);

/**
 * @brief Check if an IPv6 address is a NAT64 address (64:ff9b::x.x.x.x).
 * @param [in] a6  pointer to the IPv6 address
 * @return         non-zero if NAT64, 0 otherwise
 **/
extern __aosl_api__ int aosl_ipv6_addr_nat64 (const aosl_in6_addr_t *a6);

/**
 * @brief Check if an IPv6 address is an IPv4-compatible address (::x.x.x.x).
 * @param [in] a6  pointer to the IPv6 address
 * @return         non-zero if compatible, 0 otherwise
 **/
extern __aosl_api__ int aosl_ipv6_addr_v4_compatible (const aosl_in6_addr_t *a6);

/**
 * @brief Convert an IPv4 socket address to an IPv4-mapped IPv6 socket address.
 * @param [out] sk_addr_v6  the IPv6 socket address
 * @param [in]  sk_addr_v4  the IPv4 socket address
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_ipv6_sk_addr_from_ipv4 (aosl_sockaddr_t *sk_addr_v6, const aosl_sockaddr_t *sk_addr_v4);

/**
 * @brief Extract the IPv4 address from an IPv4-mapped IPv6 socket address.
 * @param [in]  sk_addr_v6  the IPv6 socket address
 * @param [out] sk_addr_v4  the IPv4 socket address
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_ipv6_sk_addr_to_ipv4 (aosl_sockaddr_t *sk_addr_v6, const aosl_sockaddr_t *sk_addr_v4);


/**
 * @brief Initialize a socket address with the specified address family and port.
 * @param [out] sk_addr  pointer to the socket address to initialize
 * @param [in]  af       the address family (AF_INET or AF_INET6)
 * @param [in]  port     the port number in host byte order
 * @return               0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_ip_sk_addr_init_with_port (aosl_sk_addr_t *sk_addr, uint16_t af, unsigned short port);

/* Structure for describing a resolved sock address information */
typedef struct {
	uint16_t sk_af;
	int sk_type;
	int sk_prot;
	aosl_sk_addr_t sk_addr;
} aosl_sk_addrinfo_t;


/**
 * @brief Compare the IP addresses of two socket addresses for equality.
 * @param [in] addr1  the first socket address
 * @param [in] addr2  the second socket address
 * @return            non-zero if equal, 0 if different
 **/
extern __aosl_api__ int aosl_sk_addr_ip_equal (const aosl_sockaddr_t *addr1, const aosl_sockaddr_t *addr2);

/**
 * @brief Convert a socket address to a human-readable string (ip:port).
 * @param [in]  addr      the socket address
 * @param [out] addr_buf  the output buffer
 * @param [in]  buf_len   the buffer size in bytes
 * @return                pointer to addr_buf on success, or NULL on failure
 **/
extern __aosl_api__ const char *aosl_sockaddr_str(const aosl_sockaddr_t *addr, char *addr_buf, size_t buf_len);

/**
 * @brief Convert a raw IP address to a human-readable string.
 * @param [in]  af        the address family (AF_INET or AF_INET6)
 * @param [in]  addr      pointer to the raw address (in_addr or in6_addr)
 * @param [out] addr_buf  the output buffer
 * @param [in]  buf_len   the buffer size in bytes
 * @return                pointer to addr_buf on success, or NULL on failure
 **/
extern __aosl_api__ const char *aosl_inet_addr_str (int af, const void *addr, char *addr_buf, size_t buf_len);

/**
 * @brief Convert an aosl_sk_addr_t to a human-readable string (ip:port).
 * @param [in]  addr      the socket address union
 * @param [out] addr_buf  the output buffer
 * @param [in]  buf_len   the buffer size in bytes
 * @return                pointer to addr_buf on success, or NULL on failure
 **/
extern __aosl_api__ const char *aosl_ip_sk_addr_str (const aosl_sk_addr_t *addr, char *addr_buf, size_t buf_len);

/**
 * @brief Get the port number from a socket address.
 * @param [in] addr  the socket address union
 * @return           the port number in host byte order
 **/
extern __aosl_api__ unsigned short aosl_ip_sk_addr_port (const aosl_sk_addr_t *addr);

/**
 * @brief Parse an IP address string into a raw address structure.
 * @param [out] addr      the raw address (in_addr or in6_addr)
 * @param [in]  str_addr  the IP address string
 * @return                the address length on success, 0 on failure
 **/
extern __aosl_api__ aosl_socklen_t aosl_inet_addr_from_string (void *addr, const char *str_addr);

/**
 * @brief Parse an IP address string and port into a socket address.
 * @param [out] sk_addr   the socket address
 * @param [in]  str_addr  the IP address string
 * @param [in]  port      the port number in host byte order
 * @return                the address length on success, 0 on failure
 **/
extern __aosl_api__ aosl_socklen_t aosl_ip_sk_addr_from_string (aosl_sk_addr_t *sk_addr, const char *str_addr, uint16_t port);

/**
 * @brief Get the current IPv6 NAT64 prefix used for address synthesis.
 * @return  pointer to the IPv6 prefix, or NULL if not set
 **/
extern __aosl_api__ const aosl_in6_addr_t *aosl_mpq_get_ipv6_prefix (void);

/**
 * @brief Set the IPv6 NAT64 prefix on the specified mpq for address synthesis.
 * @param [in] qid  the target mpq id
 * @param [in] a6   pointer to the IPv6 prefix
 * @return          0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_mpq_set_ipv6_prefix_on_q (aosl_mpq_t qid, const aosl_in6_addr_t *a6);


/**
 * @brief Resolve a hostname asynchronously for TCP connections.
 * Results are delivered via callback f on the specified mpq.
 * @param [in]  hostname    the hostname to resolve
 * @param [in]  port        the port number in host byte order
 * @param [out] addrs       array to receive resolved address info
 * @param [in]  addr_count  the maximum number of addresses to resolve
 * @param [in]  q           the mpq to deliver the callback on
 * @param [in]  f           the callback function
 * @param [in]  argc        the number of variable arguments
 * @param [in]  ...         variable arguments passed to the callback
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_tcp_resolve_host_async (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, ...);

/**
 * @brief Resolve a hostname asynchronously for TCP connections (va_list version).
 * @param [in]  hostname    the hostname to resolve
 * @param [in]  port        the port number in host byte order
 * @param [out] addrs       array to receive resolved address info
 * @param [in]  addr_count  the maximum number of addresses to resolve
 * @param [in]  q           the mpq to deliver the callback on
 * @param [in]  f           the callback function
 * @param [in]  argc        the number of variable arguments
 * @param [in]  args        variable argument list
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_tcp_resolve_host_asyncv (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);

/**
 * @brief Resolve a hostname asynchronously for UDP connections.
 * Results are delivered via callback f on the specified mpq.
 * @param [in]  hostname    the hostname to resolve
 * @param [in]  port        the port number in host byte order
 * @param [out] addrs       array to receive resolved address info
 * @param [in]  addr_count  the maximum number of addresses to resolve
 * @param [in]  q           the mpq to deliver the callback on
 * @param [in]  f           the callback function
 * @param [in]  argc        the number of variable arguments
 * @param [in]  ...         variable arguments passed to the callback
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_udp_resolve_host_async (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, ...);

/**
 * @brief Resolve a hostname asynchronously for UDP connections (va_list version).
 * @param [in]  hostname    the hostname to resolve
 * @param [in]  port        the port number in host byte order
 * @param [out] addrs       array to receive resolved address info
 * @param [in]  addr_count  the maximum number of addresses to resolve
 * @param [in]  q           the mpq to deliver the callback on
 * @param [in]  f           the callback function
 * @param [in]  argc        the number of variable arguments
 * @param [in]  args        variable argument list
 * @return                  0 on success, <0 on failure
 **/
extern __aosl_api__ int aosl_udp_resolve_host_asyncv (const char *hostname, unsigned short port, aosl_sk_addrinfo_t *addrs, size_t addr_count, aosl_mpq_t q, aosl_mpq_func_argv_t f, uintptr_t argc, va_list args);



#ifdef __cplusplus
}
#endif


#endif /* __AOSL_MPQ_NET_H__ */
