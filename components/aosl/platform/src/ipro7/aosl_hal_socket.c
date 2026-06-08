#include <netdb.h>
#include <lwip/sockets.h>
#include <stdbool.h>

#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_socket.h>
#include <api/aosl_route.h>
#include <api/aosl_log.h>

/* Helper function to detect and get local IP address for 127.0.0.1 translation */
static int get_local_ip_for_loopback(uint32_t *local_ip)
{
  static uint32_t cached_local_ip = 0;
  
  /* Return cached IP if already detected */
  if (cached_local_ip != 0) {
    *local_ip = cached_local_ip;
    return 0;
  }
  
  /* Create a dummy UDP socket to detect local IP */
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    aosl_log(AOSL_LOG_WARNING, "[AOSL_HAL] get_local_ip: failed to create socket");
    return -1;
  }
  
  /* Connect to external IP (doesn't send data, just selects route) */
  struct sockaddr_in dummy_addr = {0};
  dummy_addr.sin_family = AF_INET;
  dummy_addr.sin_port = htons(53); /* DNS port */
  dummy_addr.sin_addr.s_addr = htonl(0x08080808); /* 8.8.8.8 */
  
  if (connect(sock, (struct sockaddr *)&dummy_addr, sizeof(dummy_addr)) < 0) {
    close(sock);
    aosl_log(AOSL_LOG_WARNING, "[AOSL_HAL] get_local_ip: connect failed, errno=%d", errno);
    return -1;
  }
  
  /* Get the local address selected by the kernel */
  struct sockaddr_in local_addr = {0};
  socklen_t addr_len = sizeof(local_addr);
  if (getsockname(sock, (struct sockaddr *)&local_addr, &addr_len) < 0) {
    close(sock);
    aosl_log(AOSL_LOG_WARNING, "[AOSL_HAL] get_local_ip: getsockname failed, errno=%d", errno);
    return -1;
  }
  
  close(sock);
  
  /* Cache the result */
  cached_local_ip = local_addr.sin_addr.s_addr;
  *local_ip = cached_local_ip;
  
  unsigned int ip = ntohl(cached_local_ip);
  aosl_log(AOSL_LOG_INFO, "[AOSL_HAL] Detected local IP: %u.%u.%u.%u (cached for loopback translation)",
           (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF, ip & 0xFF);
  
  return 0;
}

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

static int conv_addr_to_os(const aosl_sockaddr_t *ah_addr, struct sockaddr *os_addr)
{
  if (NULL == ah_addr || NULL == os_addr) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] conv_addr_to_os: NULL pointer (ah_addr=%p, os_addr=%p)", ah_addr, os_addr);
    return -1;
  }

  switch (ah_addr->sa_family) {
    case AOSL_AF_INET: {
      struct sockaddr_in *v4 = (struct sockaddr_in *)os_addr;
      v4->sin_family = AF_INET;
      v4->sin_port = ah_addr->sa_port;
      v4->sin_addr.s_addr = ah_addr->sin_addr;
      aosl_log(AOSL_LOG_DEBUG, "[AOSL_HAL] conv_addr_to_os: IPv4 addr=0x%08x port=%d", 
               (unsigned int)ah_addr->sin_addr, ntohs(ah_addr->sa_port));
      break;
    }
    case AOSL_AF_INET6: {
#if LWIP_IPV6
      struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)os_addr;
      v6->sin6_family = AF_INET6;
      v6->sin6_port = ah_addr->sa_port;
      v6->sin6_flowinfo = ah_addr->sin6_flowinfo;
      v6->sin6_scope_id = ah_addr->sin6_scope_id;
      memcpy(&v6->sin6_addr, &ah_addr->sin6_addr, 16);
      aosl_log(AOSL_LOG_DEBUG, "[AOSL_HAL] conv_addr_to_os: IPv6 port=%d", ntohs(ah_addr->sa_port));
#else
      aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] conv_addr_to_os: IPv6 not supported");
      return -1;
#endif
      break;
    }
    default:
      aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] conv_addr_to_os: unsupported address family %d", ah_addr->sa_family);
      return -1;
  }
  return 0;
}

/* Helper function to translate 127.0.0.1 to actual local IP or INADDR_ANY */
static inline int translate_loopback_addr(struct sockaddr *os_addr, bool for_bind)
{
  if (os_addr->sa_family != AF_INET) {
    return 0; /* Only handle IPv4 */
  }
  
  struct sockaddr_in *v4 = (struct sockaddr_in *)os_addr;
  uint32_t addr_host = ntohl(v4->sin_addr.s_addr);
  
  /* Check if address is 127.0.0.1 (loopback) */
  if ((addr_host & 0xFF000000) == 0x7F000000) {
    /* For embedded lwIP systems without proper loopback routing:
     * - bind: Use INADDR_ANY to accept on all interfaces
     * - send/connect: Keep 127.0.0.1 and let lwIP handle it
     * Note: lwIP with LWIP_NETIF_LOOPBACK enabled should handle 127.0.0.1 correctly
     */
    if (for_bind) {
      /* For bind operations, use INADDR_ANY (0.0.0.0) to accept on all interfaces */
      v4->sin_addr.s_addr = htonl(INADDR_ANY);
      aosl_log(AOSL_LOG_INFO, "[AOSL_HAL] translate_loopback: 127.x.x.x → 0.0.0.0 (INADDR_ANY) for bind");
    } else {
      /* For connect/sendto, keep 127.0.0.1 - lwIP should handle loopback */
      aosl_log(AOSL_LOG_INFO, "[AOSL_HAL] translate_loopback: keeping 127.0.0.1 for send/connect (lwIP loopback)");
    }
  }
  
  return 0;
}

static void conv_addr_to_aosl(const struct sockaddr *os_addr, aosl_sockaddr_t *ah_addr)
{
  if (NULL == os_addr || NULL == ah_addr) {
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
#if LWIP_IPV6
      const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *)os_addr;
      ah_addr->sa_family = AOSL_AF_INET6;
      ah_addr->sa_port = v6->sin6_port;
      ah_addr->sin6_flowinfo = v6->sin6_flowinfo;
      ah_addr->sin6_scope_id = v6->sin6_scope_id;
      memcpy(&ah_addr->sin6_addr, &v6->sin6_addr, 16);
#endif
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
#if LWIP_IPV6
    case AF_INET6:
      return sizeof(struct sockaddr_in6);
#endif
    default:
      return -1;
  }
}

aosl_fd_t aosl_hal_sk_socket(enum aosl_socket_domain domain,
											 enum aosl_socket_type type,
											 enum aosl_socket_proto protocol)
{
  int n_domain = conv_domain_to_os(domain);
  int n_type = conv_type_to_os(type);
  int n_proto = conv_proto_to_os(protocol);
  int fd = socket(n_domain, n_type, n_proto);
  if (fd < 0) {
    return AOSL_INVALID_FD;
  }
  return (aosl_fd_t)fd;
}

int aosl_hal_sk_bind(int sockfd, const aosl_sockaddr_t* addr)
{
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
  struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
  
  // Validate input parameters
  if (aosl_fd_invalid(sockfd)) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_bind: invalid socket fd=%d", sockfd);
    return AOSL_HAL_RET_FAILURE;
  }
  
  if (NULL == addr) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_bind: NULL address pointer");
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Convert address with validation
  int conv_ret = conv_addr_to_os(addr, n_addr);
  if (conv_ret < 0) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_bind: address conversion failed, ret=%d", conv_ret);
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Translate 127.0.0.1 to INADDR_ANY for embedded systems
  translate_loopback_addr(n_addr, true);
  
  socklen_t addrlen = get_addrlen(n_addr->sa_family);
  if (addrlen < 0) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_bind: invalid address length for family %d", n_addr->sa_family);
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Log bind operation for debugging
  if (addr->sa_family == AOSL_AF_INET) {
    unsigned int ip = ntohl(addr->sin_addr);
    aosl_log(AOSL_LOG_INFO, "[AOSL_HAL] aosl_hal_sk_bind: fd=%d binding to %u.%u.%u.%u:%u", 
             sockfd,
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, 
             (ip >> 8) & 0xFF, ip & 0xFF,
             ntohs(addr->sa_port));
  }
  
  int ret = bind(sockfd, n_addr, addrlen);
  if (ret < 0) {
    int orig_errno = errno;
    ret = aosl_hal_errno_convert(orig_errno);
    AOSL_LOG_ERR("bind errno convert: %d -> %d", orig_errno, ret);
    return ret;
  }
  
  aosl_log(AOSL_LOG_DEBUG, "[AOSL_HAL] aosl_hal_sk_bind: bind successful, fd=%d", sockfd);
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
  int ret = listen(sockfd, backlog);
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
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
  struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
  socklen_t addrlen = sizeof(com_addr);
  int ret = accept(sockfd, n_addr, &addrlen);
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
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
  struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
  
  // Validate input parameters
  if (aosl_fd_invalid(sockfd)) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_connect: invalid socket fd=%d", sockfd);
    return AOSL_HAL_RET_FAILURE;
  }
  
  if (NULL == addr) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_connect: NULL address pointer");
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Convert address with validation
  int conv_ret = conv_addr_to_os(addr, n_addr);
  if (conv_ret < 0) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_connect: address conversion failed, ret=%d", conv_ret);
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Translate 127.0.0.1 to actual local IP for embedded systems
  translate_loopback_addr(n_addr, false);
  
  socklen_t addrlen = get_addrlen(n_addr->sa_family);
  if (addrlen < 0) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_connect: invalid address length for family %d", n_addr->sa_family);
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Log connect operation for debugging
  if (addr->sa_family == AOSL_AF_INET) {
    unsigned int ip = ntohl(addr->sin_addr);
    aosl_log(AOSL_LOG_INFO, "[AOSL_HAL] aosl_hal_sk_connect: fd=%d connecting to %u.%u.%u.%u:%u", 
             sockfd,
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, 
             (ip >> 8) & 0xFF, ip & 0xFF,
             ntohs(addr->sa_port));
  }
  
  int ret = connect(sockfd, n_addr, addrlen);
  if (ret < 0) {
    int orig_errno = errno;
    ret = aosl_hal_errno_convert(orig_errno);
    if (ret == AOSL_HAL_RET_EHAL) {
      AOSL_LOG_ERR("connect errno convert: %d -> %d", orig_errno, ret);
    }
    return ret;
  }
  
  aosl_log(AOSL_LOG_DEBUG, "[AOSL_HAL] aosl_hal_sk_connect: connect successful, fd=%d", sockfd);
  return 0;
}

int aosl_hal_sk_close(int sockfd)
{
  return close(sockfd);
}

isize_t aosl_hal_sk_send(int sockfd, const void* buf, size_t len, int flags)
{
  int ret = send(sockfd, buf, len, flags);
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

isize_t aosl_hal_sk_recv(int sockfd, void* buf, size_t len, int flags)
{
  int ret = recv(sockfd, buf, len, flags);
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

int aosl_hal_sk_sendto(int sockfd, const void *buffer, size_t length,
                        int flags, const aosl_sockaddr_t *dest_addr)
{
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
  struct sockaddr *n_dest_addr = (struct sockaddr *)&com_addr;
  
  // Validate input parameters
  if (aosl_fd_invalid(sockfd)) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_sendto: invalid socket fd=%d", sockfd);
    return AOSL_HAL_RET_FAILURE;
  }
  
  if (NULL == buffer) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_sendto: NULL buffer pointer");
    return AOSL_HAL_RET_FAILURE;
  }
  
  if (NULL == dest_addr) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_sendto: NULL dest_addr pointer");
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Convert address with validation
  int conv_ret = conv_addr_to_os(dest_addr, n_dest_addr);
  if (conv_ret < 0) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_sendto: address conversion failed, ret=%d", conv_ret);
    return AOSL_HAL_RET_FAILURE;
  }
  
  // Translate 127.0.0.1 to actual local IP for embedded systems
  translate_loopback_addr(n_dest_addr, false);
  
  socklen_t addrlen = get_addrlen(n_dest_addr->sa_family);
  if (addrlen < 0) {
    aosl_log(AOSL_LOG_ERROR, "[AOSL_HAL] aosl_hal_sk_sendto: invalid address length for family %d", n_dest_addr->sa_family);
    return AOSL_HAL_RET_FAILURE;
  }
  
  int ret = sendto(sockfd, buffer, length, flags, n_dest_addr, addrlen);
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

int aosl_hal_sk_recvfrom(int sockfd, void *buffer, size_t length,
                          int flags, aosl_sockaddr_t *src_addr)
{
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
  struct sockaddr *n_src_addr = (struct sockaddr *)&com_addr;
  socklen_t addrlen = sizeof(com_addr);
  int ret = recvfrom(sockfd, buffer, length, flags, n_src_addr, &addrlen);
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
  int ret = read(sockfd, buf, count);
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
  int ret = write(sockfd, buf, count);
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
  int oflags = fcntl(sockfd, F_GETFL, 0);
  int err = fcntl(sockfd, F_SETFL, O_NONBLOCK | oflags);
  if (err < 0) {
    return -1;
  }
  return 0;
}

int aosl_hal_sk_get_local_ip(aosl_sockaddr_t *addr)
{
  return -1;
}

int aosl_hal_sk_get_sockname(int sockfd, aosl_sockaddr_t *addr)
{
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
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
  struct addrinfo hints, *result = NULL, *rp = NULL;
  int err;
  int count = 0;
  struct sockaddr addr;

  // 参数检查
  if (NULL == hostname || NULL == addrs || addr_count <= 0) {
    return -1;
  }

  // 设置hints结构体
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;     // 同时支持IPv4和IPv6
  hints.ai_socktype = 0;           // 任何socket类型
  hints.ai_protocol = 0;           // 任何协议
  hints.ai_flags = AI_CANONNAME;   // 获取规范名

  // 调用线程安全的getaddrinfo
  err = getaddrinfo(hostname, NULL, &hints, &result);
  if (err != 0) {
    return 0;  // 没有解析到地址
  }

  // 遍历结果链表，复制到输出数组
  for (rp = result; rp != NULL && count < addr_count; rp = rp->ai_next) {
    // 只处理IPv4和IPv6地址
    if (rp->ai_family == AF_INET || rp->ai_family == AF_INET6) {
      // 复制sockaddr结构体
#if LWIP_IPV6
      size_t addr_len = (rp->ai_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
#else
      if (rp->ai_family == AF_INET6) {
        continue;
      }

      size_t addr_len = sizeof(struct sockaddr_in);
#endif

      if (addr_len <= sizeof(struct sockaddr_storage)) {
        memcpy(&addr, rp->ai_addr, addr_len);
        conv_addr_to_aosl(&addr, &addrs[count]);
        count++;
      }
    }
  }

  // 释放getaddrinfo分配的内存
  if (result) {
    freeaddrinfo(result);
  }

  return count;
}
