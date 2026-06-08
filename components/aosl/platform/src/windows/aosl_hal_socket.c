#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <stdio.h>
#include <string.h>

#include <api/aosl_log.h>
#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_socket.h>

static INIT_ONCE g_wsa_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK init_wsa_once(PINIT_ONCE once, PVOID param, PVOID *ctx) {
  WSADATA wsa_data;

  (void)once;
  (void)param;
  (void)ctx;

  return (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0) ? TRUE : FALSE;
}

static int ensure_wsa_started(void) {
  return InitOnceExecuteOnce(&g_wsa_once, init_wsa_once, NULL, NULL) ? 0 : -1;
}

static int conv_domain_to_os(enum aosl_socket_domain domain) {
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

static int conv_type_to_os(enum aosl_socket_type type) {
  switch (type) {
  case AOSL_SOCK_STREAM:
    return SOCK_STREAM;
  case AOSL_SOCK_DGRAM:
    return SOCK_DGRAM;
  default:
    return -1;
  }
}

static int conv_proto_to_os(enum aosl_socket_proto proto) {
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

static int get_addrlen(int af) {
  switch (af) {
  case AF_INET:
    return (int)sizeof(struct sockaddr_in);
  case AF_INET6:
    return (int)sizeof(struct sockaddr_in6);
  default:
    return -1;
  }
}

static void conv_addr_to_os(const aosl_sockaddr_t *ah_addr,
                            struct sockaddr *os_addr) {
  switch (ah_addr->sa_family) {
  case AOSL_AF_INET: {
    struct sockaddr_in *v4 = (struct sockaddr_in *)os_addr;
    memset(v4, 0, sizeof(*v4));
    v4->sin_family = AF_INET;
    v4->sin_port = ah_addr->sa_port;
    v4->sin_addr.s_addr = ah_addr->sin_addr;
    break;
  }
  case AOSL_AF_INET6: {
    struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)os_addr;
    memset(v6, 0, sizeof(*v6));
    v6->sin6_family = AF_INET6;
    v6->sin6_port = ah_addr->sa_port;
    v6->sin6_flowinfo = ah_addr->sin6_flowinfo;
    v6->sin6_scope_id = ah_addr->sin6_scope_id;
    memcpy(&v6->sin6_addr, ah_addr->sin6_addr, 16);
    break;
  }
  default:
    break;
  }
}

static void conv_addr_to_aosl(const struct sockaddr *os_addr,
                              aosl_sockaddr_t *ah_addr) {
  if (!os_addr || !ah_addr) {
    return;
  }

  switch (os_addr->sa_family) {
  case AF_INET: {
    const struct sockaddr_in *v4 = (const struct sockaddr_in *)os_addr;
    memset(ah_addr, 0, sizeof(*ah_addr));
    ah_addr->sa_family = AOSL_AF_INET;
    ah_addr->sa_port = v4->sin_port;
    ah_addr->sin_addr = v4->sin_addr.s_addr;
    break;
  }
  case AF_INET6: {
    const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *)os_addr;
    memset(ah_addr, 0, sizeof(*ah_addr));
    ah_addr->sa_family = AOSL_AF_INET6;
    ah_addr->sa_port = v6->sin6_port;
    ah_addr->sin6_flowinfo = v6->sin6_flowinfo;
    ah_addr->sin6_scope_id = v6->sin6_scope_id;
    memcpy(ah_addr->sin6_addr, &v6->sin6_addr, 16);
    break;
  }
  default:
    break;
  }
}

static int convert_socket_error(int err) { return aosl_hal_errno_convert(err); }

aosl_fd_t aosl_hal_sk_socket(enum aosl_socket_domain domain,
                             enum aosl_socket_type type,
                             enum aosl_socket_proto protocol) {
  SOCKET sk;

  if (ensure_wsa_started() != 0) {
    return AOSL_INVALID_FD;
  }

  sk = socket(conv_domain_to_os(domain), conv_type_to_os(type),
              conv_proto_to_os(protocol));
  if (sk == INVALID_SOCKET) {
    return AOSL_INVALID_FD;
  }

  return (aosl_fd_t)sk;
}

int aosl_hal_sk_bind(aosl_fd_t sockfd, const aosl_sockaddr_t *addr) {
  struct sockaddr_storage storage;
  struct sockaddr *os_addr = (struct sockaddr *)&storage;
  int af;
  int addrlen;
  SOCKET sock;

  memset(&storage, 0, sizeof(storage));
  af = conv_domain_to_os(addr->sa_family);
  addrlen = get_addrlen(af);
  conv_addr_to_os(addr, os_addr);

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  if (bind(sock, os_addr, addrlen) == SOCKET_ERROR) {
    return convert_socket_error(WSAGetLastError());
  }

  return 0;
}

int aosl_hal_sk_bind_device(aosl_fd_t sockfd, const char *if_name) {
  (void)sockfd;
  (void)if_name;
  return 0;
}

int aosl_hal_sk_listen(aosl_fd_t sockfd, int backlog) {
  SOCKET sock;

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  if (listen(sock, backlog) == SOCKET_ERROR) {
    return convert_socket_error(WSAGetLastError());
  }

  return 0;
}

aosl_fd_t aosl_hal_sk_accept(aosl_fd_t sockfd, aosl_sockaddr_t *addr) {
  struct sockaddr_storage storage;
  int addrlen = (int)sizeof(storage);
  SOCKET sk;
  SOCKET listen_sock;

  memset(&storage, 0, sizeof(storage));
  if (aosl_fd_invalid(sockfd)) {
    return AOSL_INVALID_FD;
  }
  listen_sock = (SOCKET)sockfd;

  sk = accept(listen_sock, (struct sockaddr *)&storage, &addrlen);
  if (sk == INVALID_SOCKET) {
    return AOSL_INVALID_FD;
  }

  conv_addr_to_aosl((const struct sockaddr *)&storage, addr);
  return (aosl_fd_t)sk;
}

int aosl_hal_sk_connect(aosl_fd_t sockfd, const aosl_sockaddr_t *addr) {
  struct sockaddr_storage storage;
  struct sockaddr *os_addr = (struct sockaddr *)&storage;
  int af;
  int addrlen;
  SOCKET sock;

  memset(&storage, 0, sizeof(storage));
  af = conv_domain_to_os(addr->sa_family);
  addrlen = get_addrlen(af);
  conv_addr_to_os(addr, os_addr);

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  if (connect(sock, os_addr, addrlen) == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    if (wsa_err == WSAEWOULDBLOCK || wsa_err == WSAEINPROGRESS ||
        wsa_err == WSAEALREADY) {
      return AOSL_HAL_RET_EINPROGRESS;
    }
    return convert_socket_error(wsa_err);
  }

  return 0;
}

int aosl_hal_sk_close(aosl_fd_t sockfd) {
  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  return (closesocket((SOCKET)sockfd) == SOCKET_ERROR) ? AOSL_HAL_RET_EHAL : 0;
}

int aosl_hal_sk_send(aosl_fd_t sockfd, const void *buf, size_t len, int flags) {
  SOCKET sock;
  int ret;

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  ret = send(sock, (const char *)buf, (int)len, flags);
  if (ret == SOCKET_ERROR) {
    return convert_socket_error(WSAGetLastError());
  }

  return ret;
}

int aosl_hal_sk_recv(aosl_fd_t sockfd, void *buf, size_t len, int flags) {
  SOCKET sock;
  int ret;

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  ret = recv(sock, (char *)buf, (int)len, flags);
  if (ret == SOCKET_ERROR) {
    return convert_socket_error(WSAGetLastError());
  }

  return ret;
}

int aosl_hal_sk_sendto(aosl_fd_t sockfd, const void *buffer, size_t length,
                       int flags, const aosl_sockaddr_t *dest_addr) {
  struct sockaddr_storage storage;
  struct sockaddr *os_addr = (struct sockaddr *)&storage;
  int af;
  int addrlen;
  int ret;
  SOCKET sock;

  memset(&storage, 0, sizeof(storage));
  af = conv_domain_to_os(dest_addr->sa_family);
  addrlen = get_addrlen(af);
  conv_addr_to_os(dest_addr, os_addr);

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  ret =
    sendto(sock, (const char *)buffer, (int)length, flags, os_addr, addrlen);
  if (ret == SOCKET_ERROR) {
    return convert_socket_error(WSAGetLastError());
  }

  return ret;
}

int aosl_hal_sk_recvfrom(aosl_fd_t sockfd, void *buffer, size_t length,
                         int flags, aosl_sockaddr_t *src_addr) {
  struct sockaddr_storage storage;
  int addrlen = (int)sizeof(storage);
  int ret;
  SOCKET sock;

  memset(&storage, 0, sizeof(storage));
  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  ret = recvfrom(sock, (char *)buffer, (int)length, flags,
                 (struct sockaddr *)&storage, &addrlen);
  if (ret == SOCKET_ERROR) {
    return convert_socket_error(WSAGetLastError());
  }

  conv_addr_to_aosl((const struct sockaddr *)&storage, src_addr);
  return ret;
}

int aosl_hal_sk_read(aosl_fd_t sockfd, void *buf, size_t count) {
  return aosl_hal_sk_recv(sockfd, buf, count, 0);
}

int aosl_hal_sk_write(aosl_fd_t sockfd, const void *buf, size_t count) {
  return aosl_hal_sk_send(sockfd, buf, count, 0);
}

int aosl_hal_sk_set_nonblock(aosl_fd_t sockfd) {
  u_long mode = 1;
  SOCKET sock;

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_HAL_RET_EHAL;
  }
  sock = (SOCKET)sockfd;

  if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
    return convert_socket_error(WSAGetLastError());
  }

  return 0;
}

int aosl_hal_sk_get_local_ip(aosl_sockaddr_t *addr) {
  ULONG family = AF_UNSPEC;
  ULONG flags =
    GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG size = 16 * 1024;
  IP_ADAPTER_ADDRESSES *addrs = NULL;
  IP_ADAPTER_ADDRESSES *adapter;
  ULONG ret;

  if (!addr) {
    return -1;
  }

  addrs = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(), 0, size);
  if (!addrs) {
    return -1;
  }

  ret = GetAdaptersAddresses(family, flags, NULL, addrs, &size);
  if (ret == ERROR_BUFFER_OVERFLOW) {
    HeapFree(GetProcessHeap(), 0, addrs);
    addrs = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(), 0, size);
    if (!addrs) {
      return -1;
    }
    ret = GetAdaptersAddresses(family, flags, NULL, addrs, &size);
  }

  if (ret != NO_ERROR) {
    HeapFree(GetProcessHeap(), 0, addrs);
    return -1;
  }

  for (adapter = addrs; adapter != NULL; adapter = adapter->Next) {
    IP_ADAPTER_UNICAST_ADDRESS *ua;

    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
        adapter->OperStatus != IfOperStatusUp) {
      continue;
    }

    for (ua = adapter->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
      if (ua->Address.lpSockaddr &&
          ua->Address.lpSockaddr->sa_family == AF_INET) {
        conv_addr_to_aosl(ua->Address.lpSockaddr, addr);
        HeapFree(GetProcessHeap(), 0, addrs);
        return 0;
      }
    }
  }

  HeapFree(GetProcessHeap(), 0, addrs);
  return -1;
}

int aosl_hal_sk_get_sockname(aosl_fd_t sockfd, aosl_sockaddr_t *addr) {
  struct sockaddr_storage storage;
  int addrlen = (int)sizeof(storage);
  SOCKET sock;

  memset(&storage, 0, sizeof(storage));
  if (aosl_fd_invalid(sockfd)) {
    return -1;
  }
  sock = (SOCKET)sockfd;

  if (getsockname(sock, (struct sockaddr *)&storage, &addrlen) ==
      SOCKET_ERROR) {
    return -1;
  }

  conv_addr_to_aosl((const struct sockaddr *)&storage, addr);
  return 0;
}

int aosl_hal_gethostbyname(const char *hostname, aosl_sockaddr_t *addrs,
                           int addr_count) {
  struct addrinfo hints;
  struct addrinfo *res = NULL;
  struct addrinfo *ai;
  int count = 0;

  if (ensure_wsa_started() != 0) {
    return 0;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
    return 0;
  }

  for (ai = res; ai != NULL && count < addr_count; ai = ai->ai_next) {
    if (ai->ai_addr == NULL || ai->ai_addrlen == 0) {
      continue;
    }

    switch (ai->ai_family) {
    case AF_INET:
    case AF_INET6:
      conv_addr_to_aosl(ai->ai_addr, &addrs[count]);
      ++count;
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
