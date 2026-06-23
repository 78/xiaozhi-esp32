#include <string.h>

#include <sockets.h>
#include <netdb.h>

#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_socket.h>
#include <api/aosl_log.h>

static inline int conv_domain_to_os(enum aosl_socket_domain domain)
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

static inline int conv_type_to_os(enum aosl_socket_type type)
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

static inline int conv_proto_to_os(enum aosl_socket_proto proto)
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

static inline void conv_addr_to_os(const aosl_sockaddr_t *ah_addr, struct sockaddr *os_addr)
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
#if LWIP_IPV6
      struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)os_addr;
      v6->sin6_family = AF_INET6;
      v6->sin6_port = ah_addr->sa_port;
      v6->sin6_flowinfo = ah_addr->sin6_flowinfo;
      memcpy(&v6->sin6_addr, &ah_addr->sin6_addr, 16);
#endif
      break;
    }
    default:
      return;
  }
}

static inline void conv_addr_to_aosl(const struct sockaddr *os_addr, aosl_sockaddr_t *ah_addr)
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
      memcpy(&ah_addr->sin6_addr, &v6->sin6_addr, 16);
#endif
      break;
    }
    default:
      return;
  }
}

static inline int get_addrlen(int af)
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

int aosl_hal_sk_socket(enum aosl_socket_domain domain,
                       enum aosl_socket_type type,
                       enum aosl_socket_proto protocol)
{
  int n_domain = conv_domain_to_os(domain);
  int n_type = conv_type_to_os(type);
  int n_proto = conv_proto_to_os(protocol);

  return socket(n_domain, n_type, n_proto);
}

int aosl_hal_sk_bind(int sockfd, const aosl_sockaddr_t *addr)
{
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
  struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
  conv_addr_to_os(addr, n_addr);
  socklen_t addrlen = get_addrlen(n_addr->sa_family);
  int ret = bind(sockfd, n_addr, addrlen);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
  }
  return 0;
}

int aosl_hal_sk_bind_device(int sockfd, const char *if_name)
{
  /* Quectel lwIP supports SO_BINDTODEVICE */
  int ret = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,
                       if_name, strlen(if_name));
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
  }
  return 0;
}

int aosl_hal_sk_listen(int sockfd, int backlog)
{
  int ret = listen(sockfd, backlog);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
  }
  return 0;
}

int aosl_hal_sk_accept(int sockfd, aosl_sockaddr_t *addr)
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
    ret = aosl_hal_errno_convert(errno);
  } else {
    conv_addr_to_aosl(n_addr, addr);
  }
  return ret;
}

int aosl_hal_sk_connect(int sockfd, const aosl_sockaddr_t *addr)
{
#if LWIP_IPV6
  struct sockaddr_in6 com_addr = {0};
#else
  struct sockaddr_in com_addr = {0};
#endif
  struct sockaddr *n_addr = (struct sockaddr *)&com_addr;
  conv_addr_to_os(addr, n_addr);
  socklen_t addrlen = get_addrlen(n_addr->sa_family);
  int ret = connect(sockfd, n_addr, addrlen);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
  }
  return 0;
}

int aosl_hal_sk_close(int sockfd)
{
  return close(sockfd);
}

isize_t aosl_hal_sk_send(int sockfd, const void *buf, size_t len, int flags)
{
  int ret = send(sockfd, buf, len, flags);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
  }
  return ret;
}

isize_t aosl_hal_sk_recv(int sockfd, void *buf, size_t len, int flags)
{
  int ret = recv(sockfd, buf, len, flags);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
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
  conv_addr_to_os(dest_addr, n_dest_addr);
  socklen_t addrlen = get_addrlen(n_dest_addr->sa_family);
  int ret = sendto(sockfd, buffer, length, flags, n_dest_addr, addrlen);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
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
    ret = aosl_hal_errno_convert(errno);
  } else {
    conv_addr_to_aosl(n_src_addr, src_addr);
  }
  return ret;
}

int aosl_hal_sk_read(int sockfd, void *buf, size_t count)
{
  int ret = read(sockfd, buf, count);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
  }
  return ret;
}

int aosl_hal_sk_write(int sockfd, const void *buf, size_t count)
{
  int ret = write(sockfd, buf, count);
  if (ret < 0) {
    return aosl_hal_errno_convert(errno);
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
  /*
   * Get local IP from data call info.
   * Caller should ensure data call is active before using sockets.
   */
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
  struct hostent *host;
  int count = 0;

  if (NULL == hostname || NULL == addrs || addr_count <= 0) {
    return -1;
  }

  host = gethostbyname(hostname);
  if (NULL == host) {
    return 0;
  }

  /* Iterate address list */
  for (int i = 0; host->h_addr_list[i] != NULL && count < addr_count; i++) {
    if (host->h_addrtype == AF_INET) {
      addrs[count].sa_family = AOSL_AF_INET;
      addrs[count].sa_port = 0;
      memcpy(&addrs[count].sin_addr, host->h_addr_list[i], 4);
      count++;
    }
#if LWIP_IPV6
    else if (host->h_addrtype == AF_INET6) {
      addrs[count].sa_family = AOSL_AF_INET6;
      addrs[count].sa_port = 0;
      memcpy(&addrs[count].sin6_addr, host->h_addr_list[i], 16);
      count++;
    }
#endif
  }

  return count;
}
