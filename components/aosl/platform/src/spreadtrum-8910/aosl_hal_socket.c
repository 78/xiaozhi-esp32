#include <DAPS/export/inc/tcpip6/socket_types.h>
#include <DAPS/export/inc/tcpip6/socket_api.h>
#include <RTOS/export/inc/os_api.h>

#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_socket.h>
#include <api/aosl_route.h>
#include <api/aosl_log.h>

/**
 * Helper functions to convert between AOSL and Spreadtrum SCI socket structures
 **/
static int get_spreadtrun_net_id(void)
{
  extern uint32_t agora_spreadtrun_net_id;
  return agora_spreadtrun_net_id;
}

static inline int conv_domain_to_sci(enum aosl_socket_domain domain)
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

static inline int conv_type_to_sci(enum aosl_socket_type type)
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

static inline int conv_proto_to_sci(enum aosl_socket_proto proto)
{
  switch (proto) {
    case AOSL_IPPROTO_TCP:
      return 0;  // IPPROTO_TCP, default 0 for TCP in SCI API
    case AOSL_IPPROTO_UDP:
      return 0;  // IPPROTO_UDP, default 0 for UDP in SCI API
    default:
      return 0;
  }
}

static inline void conv_addr_to_sci(const aosl_sockaddr_t *ah_addr, struct sci_sockaddr *sci_addr)
{
  switch (ah_addr->sa_family) {
    case AOSL_AF_INET: {
      sci_addr->family = AF_INET;
      sci_addr->port = ah_addr->sa_port;
      sci_addr->ip_addr = ah_addr->sin_addr;
      break;
    }
    case AOSL_AF_INET6: {
      // For IPv6, use sci_sockaddrext structure
      struct sci_sockaddrext *sci_addr_ext = (struct sci_sockaddrext *)sci_addr;
      struct sci_sockaddr6 *v6 = (struct sci_sockaddr6 *)sci_addr_ext->sa_data;
      sci_addr_ext->sa_family = AF_INET6;
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

static void conv_addr_to_aosl(const struct sci_sockaddr *sci_addr, aosl_sockaddr_t *ah_addr)
{
  switch (sci_addr->family) {
    case AF_INET: {
      ah_addr->sa_family = AOSL_AF_INET;
      ah_addr->sa_port = sci_addr->port;
      ah_addr->sin_addr = sci_addr->ip_addr;
      break;
    }
    case AF_INET6: {
      const struct sci_sockaddrext *sci_addr_ext = (const struct sci_sockaddrext *)sci_addr;
      const struct sci_sockaddr6 *v6 = (const struct sci_sockaddr6 *)sci_addr_ext->sa_data;
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

aosl_fd_t aosl_hal_sk_socket(enum aosl_socket_domain domain,
											 enum aosl_socket_type type,
											 enum aosl_socket_proto protocol)
{
  int sci_domain = conv_domain_to_sci(domain);
  int sci_type = conv_type_to_sci(type);
  int fd = sci_sock_socket(sci_domain, sci_type, 0, get_spreadtrun_net_id());
  if (fd < 0) {
    return AOSL_INVALID_FD;
  }
  return (aosl_fd_t)fd;
}

int aosl_hal_sk_bind(int sockfd, const aosl_sockaddr_t* addr)
{
  struct sci_sockaddr sci_addr = {0};
  sci_addr.family = conv_domain_to_sci(addr->sa_family);
  sci_addr.port = addr->sa_port;
  sci_addr.ip_addr = 0;

  int ret = sci_sock_bind(sockfd, &sci_addr, sizeof(struct sci_sockaddr));
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }

  return 0;
}

int aosl_hal_sk_listen(int sockfd, int backlog)
{
  int ret = sci_sock_listen(sockfd, backlog);
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }
  return 0;
}

aosl_fd_t aosl_hal_sk_accept(aosl_fd_t sockfd, aosl_sockaddr_t *addr)
{
  struct sci_sockaddr sci_addr = {0};
  int ret = sci_sock_accept(sockfd, &sci_addr, 0);
  if (ret < 0) {
    (void)aosl_hal_errno_convert(sci_sock_errno(sockfd));
    return AOSL_INVALID_FD;
  } else {
    conv_addr_to_aosl(&sci_addr, addr);
  }
  return (aosl_fd_t)ret;
}

int aosl_hal_sk_connect(int sockfd, const aosl_sockaddr_t *addr)
{
  struct sci_sockaddrext sci_addr_ext = {0};
  conv_addr_to_sci(addr, (struct sci_sockaddr *)&sci_addr_ext);
  
  int ret = sci_sock_connect(sockfd, &sci_addr_ext, sizeof(struct sci_sockaddr));
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }
  return 0;
}

int aosl_hal_sk_close(int sockfd)
{
  return sci_sock_socketclose(sockfd);
}

int aosl_hal_sk_send(int sockfd, const void* buf, size_t len, int flags)
{
  int ret = sci_sock_send(sockfd, (char *)buf, len, flags);
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }
  return ret;
}

int aosl_hal_sk_recv(int sockfd, void* buf, size_t len, int flags)
{
  int ret = sci_sock_recv(sockfd, (char *)buf, len, flags);
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }
  return ret;
}

int aosl_hal_sk_sendto(int sockfd, const void *buffer, size_t length,
                       int flags, const aosl_sockaddr_t *dest_addr)
{
  struct sci_sockaddr sci_addr = {0};
  conv_addr_to_sci(dest_addr, &sci_addr);
  
  int ret = sci_sock_sendto(sockfd, (char *)buffer, length, flags, &sci_addr);
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }
  return ret;
}

int aosl_hal_sk_recvfrom(int sockfd, void *buffer, size_t length,
                         int flags, aosl_sockaddr_t *src_addr)
{
  struct sci_sockaddr sci_addr = {0};
  int ret = sci_sock_recvfrom(sockfd, (char *)buffer, length, flags, &sci_addr);
  if (ret < 0) {
    ret = aosl_hal_errno_convert(sci_sock_errno(sockfd));
  } else {
    conv_addr_to_aosl(&sci_addr, src_addr);
  }
  return ret;
}

int aosl_hal_sk_read(int sockfd, void *buf, size_t count)
{
  int ret = sci_sock_recv(sockfd, (char *)buf, count, 0);
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }
  return ret;
}

int aosl_hal_sk_write(int sockfd, const void *buf, size_t count)
{
  int ret = sci_sock_send(sockfd, (char *)buf, count, 0);
  if (ret < 0) {
    return aosl_hal_errno_convert(sci_sock_errno(sockfd));
  }
  return ret;
}

int aosl_hal_sk_set_nonblock(int sockfd)
{
  int ret = sci_sock_setsockopt(sockfd, SO_NBIO, NULL);
  if (ret < 0) {
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
  struct sci_sockaddr sci_addr = {0};
  int err = sci_sock_getsockname(sockfd, &sci_addr);
  if (err == 0) {
    conv_addr_to_aosl(&sci_addr, addr);
  }
  return err;
}

int aosl_hal_sk_bind_device(int sockfd, const char *if_name)
{
  return 0;
}

int aosl_hal_gethostbyname(const char *hostname, aosl_sockaddr_t *addrs, int addr_count)
{
  struct sci_hostent *hostent;
  int count = 0;

  if (NULL == hostname || NULL == addrs || addr_count <= 0) {
    return -1;
  }

  hostent = sci_gethostbyname_ext((char *)hostname, get_spreadtrun_net_id());
  if (NULL == hostent) {
    return 0;
  }

  if (hostent->h_addr_list != NULL) {
    for (int i = 0; hostent->h_addr_list[i] != NULL && count < addr_count; i++) {
      aosl_sockaddr_t *addr = &addrs[count];
      if (hostent->h_addrtype == AF_INET) {
        addr->sa_family = AOSL_AF_INET;
        addr->sa_port = 0;
        addr->sin_addr = *(uint32_t *)hostent->h_addr_list[i];
        count++;
      } else if (hostent->h_addrtype == AF_INET6) {
        addr->sa_family = AOSL_AF_INET6;
        addr->sa_port = 0;
        addr->sin6_flowinfo = 0;
        addr->sin6_scope_id = 0;
        memcpy(&addr->sin6_addr, hostent->h_addr_list[i], 16);
        count++;
      }
    }
  }

  return count;
}
