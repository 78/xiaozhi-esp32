#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <hal/aosl_hal_socket.h>
#include <hal/aosl_hal_errno.h>
#include <api/aosl_log.h>
#include <api/aosl_mpq_net.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "lwip/netif.h"
#include "esp_netif.h"

static int conv_domain_to_os(enum aosl_socket_domain domain)
{
	switch (domain) {
	case AOSL_AF_UNSPEC:
		return AF_UNSPEC;
	case AOSL_AF_INET:
		return AF_INET;
	case AOSL_AF_INET6:
		return AF_INET6;
	default:
		return AF_UNSPEC;
	}
}

static int conv_type_to_os(enum aosl_socket_type type)
{
	switch (type) {
	case AOSL_SOCK_STREAM:
		return SOCK_STREAM;
	case AOSL_SOCK_DGRAM:
		return SOCK_DGRAM;
	default:
		return -1;
	}
}

static int conv_proto_to_os(enum aosl_socket_proto proto)
{
	switch (proto) {
	case AOSL_IPPROTO_TCP:
		return IPPROTO_TCP;
	case AOSL_IPPROTO_UDP:
		return IPPROTO_UDP;
	case AOSL_IPPROTO_AUTO:
		return 0;
	default:
		return 0;
	}
}

static void conv_addr_to_os(const aosl_sockaddr_t *ah_addr, struct sockaddr *os_addr)
{
	switch (ah_addr->sa_family) {
	case AOSL_AF_INET: {
		struct sockaddr_in *v4 = (struct sockaddr_in *)os_addr;
		v4->sin_family = AF_INET;
		v4->sin_port = ah_addr->sa_port;
		v4->sin_addr.s_addr = ah_addr->sin_addr;
		break;
	}
	case AOSL_AF_INET6: {
		struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)os_addr;
		v6->sin6_family = AF_INET6;
		v6->sin6_port = ah_addr->sa_port;
		v6->sin6_flowinfo = ah_addr->sin6_flowinfo;
		v6->sin6_scope_id = ah_addr->sin6_scope_id;
		memcpy(&v6->sin6_addr, &ah_addr->sin6_addr, 16);
		break;
	}
	default:
		return;
	}
}

static void conv_addr_to_aosl(const struct sockaddr *os_addr, aosl_sockaddr_t *ah_addr)
{
	if (!os_addr || !ah_addr) {
		return;
	}
	switch (os_addr->sa_family) {
	case AF_INET: {
		const struct sockaddr_in *v4 = (const struct sockaddr_in *)os_addr;
		ah_addr->sa_family = AOSL_AF_INET;
		ah_addr->sa_port = v4->sin_port;
		ah_addr->sin_addr = v4->sin_addr.s_addr;
		break;
	}
	case AF_INET6: {
		const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *)os_addr;
		ah_addr->sa_family = AOSL_AF_INET6;
		ah_addr->sa_port = v6->sin6_port;
		ah_addr->sin6_flowinfo = v6->sin6_flowinfo;
		ah_addr->sin6_scope_id = v6->sin6_scope_id;
		memcpy(&ah_addr->sin6_addr, &v6->sin6_addr, 16);
		break;
	}
	default:
		return;
	}
}

static int get_addrlen(int af)
{
	switch (af) {
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	default:
		return -1;
	}
}

aosl_fd_t aosl_hal_sk_socket(enum aosl_socket_domain domain, enum aosl_socket_type type, enum aosl_socket_proto protocol)
{
	int n_domain = conv_domain_to_os(domain);
	int n_type = conv_type_to_os(type);
	int n_proto = conv_proto_to_os(protocol);
	int fd = lwip_socket(n_domain, n_type, n_proto);
	if (fd < 0) {
		return AOSL_INVALID_FD;
	}
	return (aosl_fd_t)fd;
}

int aosl_hal_sk_bind(int sockfd, const aosl_sockaddr_t *addr)
{
	struct sockaddr_in6 com_addr = { 0 };
	struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
	int af = conv_domain_to_os(addr->sa_family);
	socklen_t addrlen = get_addrlen(af);
	conv_addr_to_os(addr, n_addr);
	int ret = lwip_bind(sockfd, n_addr, addrlen);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		AOSL_LOG_ERR("bind errno convert: %d -> %d", orig_errno, ret);
		return ret;
	}
	return 0;
}

int aosl_hal_sk_bind_device(int sockfd, const char *if_name)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name) - 1);
	int ret = lwip_setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr));
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		AOSL_LOG_ERR("setsockopt(SO_BINDTODEVICE) errno convert: %d -> %d", orig_errno, ret);
		return ret;
	}
	return 0;
}

int aosl_hal_sk_listen(int sockfd, int backlog)
{
	int ret = lwip_listen(sockfd, backlog);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("listen errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	}
	return 0;
}

aosl_fd_t aosl_hal_sk_accept(aosl_fd_t sockfd, aosl_sockaddr_t *addr)
{
	struct sockaddr_in6 com_addr = { 0 };
	struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
	socklen_t addrlen = sizeof(com_addr);
	int ret = lwip_accept(sockfd, n_addr, &addrlen);
	if (ret < 0) {
		int orig_errno = errno;
		int hal_err = aosl_hal_errno_convert(orig_errno);
		if (hal_err == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("accept errno convert: %d -> %d", orig_errno, hal_err);
		}
		return AOSL_INVALID_FD;
	} else {
		conv_addr_to_aosl(n_addr, addr);
	}
	return (aosl_fd_t)ret;
}

int aosl_hal_sk_connect(int sockfd, const aosl_sockaddr_t *addr)
{
	struct sockaddr_in6 com_addr = { 0 };
	struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
	int af = conv_domain_to_os(addr->sa_family);
	socklen_t addrlen = get_addrlen(af);
	conv_addr_to_os(addr, n_addr);
	int ret = lwip_connect(sockfd, n_addr, addrlen);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("connect errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	}
	return 0;
}

int aosl_hal_sk_close(int sockfd)
{
	return close(sockfd);
}

int aosl_hal_sk_send(int sockfd, const void *buf, size_t len, int flags)
{
	int ret = lwip_send(sockfd, buf, len, flags);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("send errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	}
	return ret;
}

int aosl_hal_sk_recv(int sockfd, void *buf, size_t len, int flags)
{
	int ret = lwip_recv(sockfd, buf, len, flags);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("recv errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	}
	return ret;
}

int aosl_hal_sk_sendto(int sockfd, const void *buffer, size_t length, int flags, const aosl_sockaddr_t *dest_addr)
{
	struct sockaddr_in6 com_addr = { 0 };
	struct sockaddr *n_dest_addr = (struct sockaddr *)&com_addr;
	int af = conv_domain_to_os(dest_addr->sa_family);
	socklen_t addrlen = get_addrlen(af);
	conv_addr_to_os(dest_addr, n_dest_addr);
	int ret = lwip_sendto(sockfd, buffer, length, flags, n_dest_addr, addrlen);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("sendto errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	}
	return ret;
}

int aosl_hal_sk_recvfrom(int sockfd, void *buffer, size_t length, int flags, aosl_sockaddr_t *src_addr)
{
	struct sockaddr_in6 com_addr = { 0 };
	struct sockaddr *n_src_addr = (struct sockaddr *)&com_addr;
	socklen_t addrlen = sizeof(com_addr);
	int ret = lwip_recvfrom(sockfd, buffer, length, flags, n_src_addr, &addrlen);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("recvfrom errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	} else {
		conv_addr_to_aosl(n_src_addr, src_addr);
	}
	return ret;
}

int aosl_hal_sk_read(int sockfd, void *buf, size_t count)
{
	int ret = lwip_read(sockfd, buf, count);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("read errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	}
	return ret;
}

int aosl_hal_sk_write(int sockfd, const void *buf, size_t count)
{
	int ret = lwip_write(sockfd, buf, count);
	if (ret < 0) {
		int orig_errno = errno;
		ret = aosl_hal_errno_convert(orig_errno);
		if (ret == AOSL_HAL_RET_EHAL) {
			AOSL_LOG_ERR("write errno convert: %d -> %d", orig_errno, ret);
		}
		return ret;
	}
	return ret;
}

int aosl_hal_sk_set_nonblock(int sockfd)
{
	int oflags = fcntl(sockfd, F_GETFL);
	if (oflags < 0) {
		return -1;
	}
	oflags |= O_NONBLOCK;
	int err = lwip_fcntl(sockfd, F_SETFL, oflags);
	if (err < 0) {
		return -1;
	}

	return 0;
}

int aosl_hal_sk_get_local_ip(aosl_sockaddr_t *addr)
{
	esp_netif_ip_info_t ip_info = { 0 };
	esp_netif_t *netif = NULL;
	esp_netif_t *default_netif = NULL;
	int min_prio = INT_MAX;

	for (netif = esp_netif_next_unsafe(NULL); netif != NULL; netif = esp_netif_next_unsafe(netif)) {
		int prio = esp_netif_get_route_prio(netif);
		if (prio < min_prio) {
			min_prio = prio;
			default_netif = netif;
		}
	}
	if (default_netif == NULL) {
		default_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	}
	if (default_netif == NULL) {
		default_netif = esp_netif_next_unsafe(NULL);
	}
	if (default_netif == NULL) {
		return -1;
	}

	if (esp_netif_get_ip_info(default_netif, &ip_info) != ESP_OK) {
		return -1;
	}

	addr->sa_family = AOSL_AF_INET;
	addr->sa_port = 0;
	addr->sin_addr = ip_info.ip.addr;
	return 0;
}

int aosl_hal_sk_get_sockname(int sockfd, aosl_sockaddr_t *addr)
{
	struct sockaddr_in6 com_addr = { 0 };
	struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
	socklen_t addrlen = sizeof(com_addr);
	int err = getsockname(sockfd, n_addr, &addrlen);
	if (err == 0) {
		conv_addr_to_aosl(n_addr, addr);
	}
	return err;
}

int aosl_hal_gethostbyname(const char *hostname, aosl_sockaddr_t *addrs, int addr_count)
{
	struct addrinfo *res = NULL;
	int count = 0;
	struct addrinfo hints = {
		.ai_flags = AI_ADDRCONFIG,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = IPPROTO_UDP,
		.ai_addrlen = 0,
		.ai_addr = NULL,
		.ai_canonname = NULL,
		.ai_next = NULL,
	};

	int err = getaddrinfo(hostname, NULL, &hints, &res);
	if (err != 0) {
		return 0;
	}

	struct addrinfo *ai = NULL;
	for (ai = res; ai != NULL && count < addr_count; ai = ai->ai_next) {
		if (ai->ai_addr == NULL || ai->ai_addrlen == 0) continue;
		switch (ai->ai_family) {
		case AF_INET:
		case AF_INET6:
			conv_addr_to_aosl(ai->ai_addr, &addrs[count]);
			count++;
			break;
		default:
			break;
		}
	}

	if (res != NULL) {
		freeaddrinfo(res);
	}

	return count;
}
