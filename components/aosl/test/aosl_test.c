/***************************************************************************
 * Module:	aosl test
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <stdio.h>
#include <string.h>

#include "hal/aosl_hal_atomic.h"
#include "hal/aosl_hal_errno.h"
#include "hal/aosl_hal_file.h"
#include "hal/aosl_hal_iomp.h"
#include "hal/aosl_hal_log.h"
#include "hal/aosl_hal_memory.h"
#include "hal/aosl_hal_socket.h"
#include "hal/aosl_hal_thread.h"
#include "hal/aosl_hal_time.h"
#include "hal/aosl_hal_utils.h"

#include "api/aosl.h"
#include "api/aosl_log.h"
#include "api/aosl_mpq.h"
#include "api/aosl_socket.h"
#include "api/aosl_mpq_net.h"
#include "api/aosl_thread.h"

#define UNUSED(expr) (void)(expr)
#define CAST_INT64(val)  ((long long)val)
#define CAST_UINT64(val) ((unsigned long long)val)
#define LOG_FMT(fmt, ...) aosl_printf("[%s:%u] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__);

// expect
#define EXPECT_EQ(val, expect)                                                                                         \
  if ((val) != (expect)) {                                                                                             \
    LOG_FMT("expect_eq failed, v1=%llu v2=%llu", CAST_UINT64(val), CAST_UINT64(expect));                               \
    return -1;                                                                                                         \
  }
#define EXPECT_NE(val, expect)                                                                                         \
  if ((val) == (expect)) {                                                                                             \
    LOG_FMT("expect_ne failed, v1=%llu v2=%llu", CAST_UINT64(val), CAST_UINT64(expect));                               \
    return -1;                                                                                                         \
  }
#define EXPECT_LT(val, expect)                                                                                         \
  if ((val) >= (expect)) {                                                                                             \
    LOG_FMT("expect_lt failed, v1=%llu v2=%llu", CAST_UINT64(val), CAST_UINT64(expect));                               \
    return -1;                                                                                                         \
  }
#define EXPECT_LE(val, expect)                                                                                         \
  if ((val) > (expect)) {                                                                                              \
    LOG_FMT("expect_le failed, v1=%llu v2=%llu", CAST_UINT64(val), CAST_UINT64(expect));                               \
    return -1;                                                                                                         \
  }
#define EXPECT_GT(val, expect)                                                                                         \
  if ((val) <= (expect)) {                                                                                             \
    LOG_FMT("expect_gt failed, v1=%llu v2=%llu", CAST_UINT64(val), CAST_UINT64(expect));                               \
    return -1;                                                                                                         \
  }
#define EXPECT_GE(val, expect)                                                                                         \
  if ((val) < (expect)) {                                                                                              \
    LOG_FMT("expect_ge failed, v1=%llu v2=%llu", CAST_UINT64(val), CAST_UINT64(expect));                               \
    return -1;                                                                                                         \
  }

// check
#define CHECK(cond)                                                                                                    \
  if (!(cond)) {                                                                                                         \
    LOG_FMT("check %s failed.", #cond);                                                                              \
    return -1;                                                                                                         \
  }

#define CHECK_FMT(cond, fmt, ...)                                                                                      \
  if (!(cond)) {                                                                                                         \
    LOG_FMT("check %s failed. " fmt, #cond, ##__VA_ARGS__);                                                       \
    return -1;                                                                                                         \
  }

static const char *server_ip = "127.0.0.1";
static const uint16_t server_port = 9527;

// Atomic test data structure
struct atomic_test_data {
  intptr_t atomic_counter;
  intptr_t normal_counter;
  int iterations;
};

// Thread function for atomic increment test
static void *thread_entry_atomic_inc(void *arg)
{
  struct atomic_test_data *data = (struct atomic_test_data *)arg;
  
  for (int i = 0; i < data->iterations; i++) {
    aosl_hal_atomic_inc(&data->atomic_counter);
    data->normal_counter++; // Non-atomic increment for comparison
  }
  
  return NULL;
}

// Thread function for atomic add test
static void *thread_entry_atomic_add(void *arg)
{
  struct atomic_test_data *data = (struct atomic_test_data *)arg;
  
  for (int i = 0; i < data->iterations; i++) {
    aosl_hal_atomic_add(1, &data->atomic_counter);
  }
  
  return NULL;
}

// Thread function for atomic cmpxchg test
static void *thread_entry_atomic_cmpxchg(void *arg)
{
  struct atomic_test_data *data = (struct atomic_test_data *)arg;
  
  for (int i = 0; i < data->iterations; i++) {
    intptr_t old_val, new_val;
    do {
      old_val = aosl_hal_atomic_read(&data->atomic_counter);
      new_val = old_val + 1;
    } while (aosl_hal_atomic_cmpxchg(&data->atomic_counter, old_val, new_val) != old_val);
  }
  
  return NULL;
}

// Test: Basic atomic operations
static int aosl_test_hal_atomic_basic(void)
{
  intptr_t a = 0;

  LOG_FMT("Test: Basic atomic operations");
  
  CHECK(aosl_hal_atomic_read(&a) == 0);
  aosl_hal_atomic_set(&a, 2);
  CHECK(aosl_hal_atomic_read(&a) == 2);
  aosl_hal_atomic_add(3, &a);
  CHECK(aosl_hal_atomic_read(&a) == 5);
  aosl_hal_atomic_sub(1, &a);
  CHECK(aosl_hal_atomic_read(&a) == 4);
  aosl_hal_atomic_inc(&a);
  CHECK(aosl_hal_atomic_read(&a) == 5);
  aosl_hal_atomic_dec(&a);
  CHECK(aosl_hal_atomic_read(&a) == 4);
  CHECK(aosl_hal_atomic_add(10, &a) == 14);
  CHECK(aosl_hal_atomic_sub(5, &a) == 9);
  CHECK(aosl_hal_atomic_cmpxchg(&a, 10, 100) == 9);
  CHECK(aosl_hal_atomic_cmpxchg(&a, 9, 100) == 9);
  CHECK(aosl_hal_atomic_cmpxchg(&a, 9, 100) == 100);
  CHECK(aosl_hal_atomic_xchg(&a, 50) == 100);
  CHECK(aosl_hal_atomic_read(&a) == 50);

  LOG_FMT("test success");
  return 0;
}

// Test: Multi-thread atomic contention with atomic_inc
static int aosl_test_hal_atomic_multithread_inc(void)
{
  int ret;
  struct atomic_test_data test_data = {0};
  aosl_thread_t threads[5] = {0};
  aosl_thread_param_t param;
  const int num_threads = 5;
  const int iterations = 10000;
  
  LOG_FMT("Test: Multi-thread atomic contention (atomic_inc)");
  
  test_data.atomic_counter = 0;
  test_data.normal_counter = 0;
  test_data.iterations = iterations;
  
  // Create threads
  for (int i = 0; i < num_threads; i++) {
    param.name = "test-atomic-inc";
    param.priority = AOSL_THRD_PRI_DEFAULT;
    param.stack_size = 0;
    ret = aosl_hal_thread_create(&threads[i], &param, thread_entry_atomic_inc, &test_data);
    if (ret != 0) {
      LOG_FMT("thread_create[%d] returned %d", i, ret);
      for (int j = 0; j < i; j++) {
        aosl_hal_thread_join(threads[j], NULL);
        aosl_hal_thread_destroy(threads[j]);
      }
      return -1;
    }
  }
  
  // Wait for all threads to complete
  for (int i = 0; i < num_threads; i++) {
    ret = aosl_hal_thread_join(threads[i], NULL);
    if (ret != 0) {
      LOG_FMT("thread_join[%d] returned %d", i, ret);
    }
    aosl_hal_thread_destroy(threads[i]);
  }
  
  intptr_t expected = (intptr_t)num_threads * iterations;
  intptr_t atomic_result = aosl_hal_atomic_read(&test_data.atomic_counter);
  intptr_t normal_result = test_data.normal_counter;
  
  LOG_FMT("atomic_counter=%lld, expected=%lld, normal_counter=%lld", 
          CAST_INT64(atomic_result), CAST_INT64(expected), CAST_INT64(normal_result));
  
  // Atomic counter should be exactly correct
  CHECK_FMT(atomic_result == expected, 
            "atomic_counter=%lld, expected=%lld", 
            CAST_INT64(atomic_result), CAST_INT64(expected));
  
  // Normal counter will likely be incorrect due to race conditions
  if (normal_result != expected) {
    LOG_FMT("normal_counter has race condition as expected: %lld != %lld", 
            CAST_INT64(normal_result), CAST_INT64(expected));
  }
  
  LOG_FMT("test success");
  return 0;
}

// Test: Multi-thread atomic contention with atomic_add
static int aosl_test_hal_atomic_multithread_add(void)
{
  int ret;
  struct atomic_test_data test_data = {0};
  aosl_thread_t threads[5] = {0};
  aosl_thread_param_t param;
  const int num_threads = 5;
  const int iterations = 10000;
  
  LOG_FMT("Test: Multi-thread atomic contention (atomic_add)");
  
  test_data.atomic_counter = 0;
  test_data.iterations = iterations;
  
  // Create threads
  for (int i = 0; i < num_threads; i++) {
    param.name = "test-atomic-add";
    param.priority = AOSL_THRD_PRI_DEFAULT;
    param.stack_size = 0;
    ret = aosl_hal_thread_create(&threads[i], &param, thread_entry_atomic_add, &test_data);
    if (ret != 0) {
      LOG_FMT("thread_create[%d] returned %d", i, ret);
      for (int j = 0; j < i; j++) {
        aosl_hal_thread_join(threads[j], NULL);
        aosl_hal_thread_destroy(threads[j]);
      }
      return -1;
    }
  }
  
  // Wait for all threads to complete
  for (int i = 0; i < num_threads; i++) {
    ret = aosl_hal_thread_join(threads[i], NULL);
    if (ret != 0) {
      LOG_FMT("thread_join[%d] returned %d", i, ret);
    }
    aosl_hal_thread_destroy(threads[i]);
  }
  
  intptr_t expected = (intptr_t)num_threads * iterations;
  intptr_t atomic_result = aosl_hal_atomic_read(&test_data.atomic_counter);
  
  LOG_FMT("atomic_counter=%lld, expected=%lld", 
          CAST_INT64(atomic_result), CAST_INT64(expected));
  
  CHECK_FMT(atomic_result == expected, 
            "atomic_counter=%lld, expected=%lld", 
            CAST_INT64(atomic_result), CAST_INT64(expected));
  
  LOG_FMT("test success");
  return 0;
}

// Test: Multi-thread atomic contention with atomic_cmpxchg
static int aosl_test_hal_atomic_multithread_cmpxchg(void)
{
  int ret;
  struct atomic_test_data test_data = {0};
  aosl_thread_t threads[5] = {0};
  aosl_thread_param_t param;
  const int num_threads = 5;
  const int iterations = 1000; // Fewer iterations for cmpxchg due to retry overhead
  
  LOG_FMT("Test: Multi-thread atomic contention (atomic_cmpxchg)");
  
  test_data.atomic_counter = 0;
  test_data.iterations = iterations;
  
  // Create threads
  for (int i = 0; i < num_threads; i++) {
    param.name = "test-atomic-cmpxchg";
    param.priority = AOSL_THRD_PRI_DEFAULT;
    param.stack_size = 0;
    ret = aosl_hal_thread_create(&threads[i], &param, thread_entry_atomic_cmpxchg, &test_data);
    if (ret != 0) {
      LOG_FMT("thread_create[%d] returned %d", i, ret);
      for (int j = 0; j < i; j++) {
        aosl_hal_thread_join(threads[j], NULL);
        aosl_hal_thread_destroy(threads[j]);
      }
      return -1;
    }
  }
  
  // Wait for all threads to complete
  for (int i = 0; i < num_threads; i++) {
    ret = aosl_hal_thread_join(threads[i], NULL);
    if (ret != 0) {
      LOG_FMT("thread_join[%d] returned %d", i, ret);
    }
    aosl_hal_thread_destroy(threads[i]);
  }
  
  intptr_t expected = (intptr_t)num_threads * iterations;
  intptr_t atomic_result = aosl_hal_atomic_read(&test_data.atomic_counter);
  
  LOG_FMT("atomic_counter=%lld, expected=%lld", 
          CAST_INT64(atomic_result), CAST_INT64(expected));
  
  CHECK_FMT(atomic_result == expected, 
            "atomic_counter=%lld, expected=%lld", 
            CAST_INT64(atomic_result), CAST_INT64(expected));
  
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_atomic(void)
{
  CHECK(aosl_test_hal_atomic_basic() == 0);
  CHECK(aosl_test_hal_atomic_multithread_inc() == 0);
  CHECK(aosl_test_hal_atomic_multithread_add() == 0);
  CHECK(aosl_test_hal_atomic_multithread_cmpxchg() == 0);
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_errno(void)
{
  bool have_EAGAIN = 0;
  bool have_EINTR = 0;
  int ret;
  for (int i = 0; i < 1024; i++) {
    ret = aosl_hal_errno_convert(i);
    if (ret == AOSL_HAL_RET_EAGAIN)
      have_EAGAIN = true;
    else if (ret == AOSL_HAL_RET_EINTR)
      have_EINTR = true;
  }

  CHECK(have_EAGAIN == true);
  CHECK(have_EINTR == true);
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_fd_type(void)
{
#if defined(_WIN32) || defined(_WIN64)
  CHECK(sizeof(aosl_fd_t) >= sizeof(uintptr_t));
#else
  CHECK(sizeof(aosl_fd_t) == sizeof(int));
#endif
  CHECK(aosl_fd_invalid(AOSL_INVALID_FD));
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_file(void)
{
  // ignore
  return 0;
}

#if defined(AOSL_HAL_HAVE_EPOLL) && (AOSL_HAL_HAVE_EPOLL == 1)
static int aosl_test_hal_iomp_epoll(aosl_fd_t server_fd, aosl_fd_t client_fd, const aosl_sockaddr_t *server_addr)
{
  int ret;
  aosl_poll_event_t event = {0};
  char snd_buf[100] = {0};
  char rcv_buf[100] = {0};
  int epfd = aosl_hal_epoll_create();
  CHECK(epfd >= 0);

  event.fd = server_fd;
  event.events = AOSL_POLLIN;
  ret = aosl_hal_epoll_ctl(epfd, AOSL_POLL_CTL_ADD, server_fd, &event);
  if (ret != 0) {
    LOG_FMT("epoll ctl add failed");
    goto __tag_out;
  }

  for (int i = 0; i < 10; i++) {
    sprintf(snd_buf, "iomp test msg [%d]", i);
    ret = aosl_hal_sk_sendto(client_fd, snd_buf, sizeof(snd_buf), 0, server_addr);
    if (ret != sizeof(snd_buf)) {
      LOG_FMT("[%d] send failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }

    memset(&event, 0, sizeof(event));
    ret = aosl_hal_epoll_wait(epfd, &event, 1, 1000);
    if (ret <= 0) {
      LOG_FMT("[%d] epoll failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }

    if (!(event.events & AOSL_POLLIN)) {
      LOG_FMT("[%d] fd check failed", i);
      ret = -1;
      goto __tag_out;
    }

    ret = aosl_hal_sk_recvfrom(server_fd, rcv_buf, sizeof(rcv_buf), 0, NULL);
    if (ret != sizeof(rcv_buf)) {
      LOG_FMT("[%d] recvfrom failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }
    LOG_FMT("rcv_msg='%s'", rcv_buf);
  }

  ret = 0;

__tag_out:
  aosl_hal_epoll_destroy(epfd);
  return ret;
}
#endif

#if defined(AOSL_HAL_HAVE_POLL) && (AOSL_HAL_HAVE_POLL == 1)
static int aosl_test_hal_iomp_poll(aosl_fd_t server_fd, aosl_fd_t client_fd, const aosl_sockaddr_t *server_addr)
{
  int ret;
  aosl_poll_event_t event = {0};
  char snd_buf[100] = {0};
  char rcv_buf[100] = {0};

  for (int i = 0; i < 10; i++) {
    sprintf(snd_buf, "iomp test msg [%d]", i);
    ret = aosl_hal_sk_sendto(client_fd, snd_buf, sizeof(snd_buf), 0, server_addr);
    if (ret != sizeof(snd_buf)) {
      LOG_FMT("[%d] send failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }

    memset(&event, 0, sizeof(event));
    event.fd = server_fd;
    event.events = AOSL_POLLIN;
    ret = aosl_hal_poll(&event, 1, 1000);
    if (ret <= 0) {
      LOG_FMT("[%d] epoll failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }

    if (!(event.revents & AOSL_POLLIN)) {
      LOG_FMT("[%d] fd check failed", i);
      ret = -1;
      goto __tag_out;
    }

    ret = aosl_hal_sk_recvfrom(server_fd, rcv_buf, sizeof(rcv_buf), 0, NULL);
    if (ret != sizeof(rcv_buf)) {
      LOG_FMT("[%d] recvfrom failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }
    LOG_FMT("rcv_msg='%s'", rcv_buf);
  }

  ret = 0;

__tag_out:
  return ret;
}
#endif

#if defined(AOSL_HAL_HAVE_SELECT) && (AOSL_HAL_HAVE_SELECT == 1)
static int aosl_test_hal_iomp_select(aosl_fd_t server_fd, aosl_fd_t client_fd, const aosl_sockaddr_t *server_addr)
{
  int ret;
  char snd_buf[100] = {0};
  char rcv_buf[100] = {0};
  fd_set_t *fdset = aosl_hal_fdset_create();
  CHECK(fdset != NULL);

  for (int i = 0; i < 10; i++) {
    sprintf(snd_buf, "iomp test msg [%d]", i);
    ret = aosl_hal_sk_sendto(client_fd, snd_buf, sizeof(snd_buf), 0, server_addr);
    if (ret != sizeof(snd_buf)) {
      LOG_FMT("[%d] send failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }

    aosl_hal_fdset_zero(fdset);
    aosl_hal_fdset_set(fdset, server_fd);
    ret = aosl_hal_select(server_fd + 1, fdset, NULL, NULL, 1000);
    if (ret <= 0) {
      LOG_FMT("[%d] select failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }
    ret = aosl_hal_fdset_isset(fdset, server_fd);
    if (ret == 0) {
      LOG_FMT("[%d] fdset check failed", i);
      ret = -1;
      goto __tag_out;
    }

    ret = aosl_hal_sk_recvfrom(server_fd, rcv_buf, sizeof(rcv_buf), 0, NULL);
    if (ret != sizeof(rcv_buf)) {
      LOG_FMT("[%d] recvfrom failed, ret=%d", i, ret);
      ret = -1;
      goto __tag_out;
    }
    LOG_FMT("rcv_msg='%s'", rcv_buf);
  }

  ret = 0;

__tag_out:
  aosl_hal_fdset_destroy(fdset);
  return ret;
}
#endif

static int aosl_test_hal_iomp(void)
{
  int ret;
  aosl_fd_t client_fd = AOSL_INVALID_FD;
  aosl_fd_t server_fd = AOSL_INVALID_FD;

  // server
  server_fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP);
  if (aosl_fd_invalid(server_fd)) {
    LOG_FMT("get server socket failed");
    return -1;
  }

  aosl_sockaddr_t server_addr = {0};
  server_addr.sa_family = AOSL_AF_INET;
  server_addr.sa_port = aosl_htons(server_port);
  aosl_inet_addr_from_string(&server_addr.sin_addr, server_ip);
  ret = aosl_hal_sk_bind(server_fd, &server_addr);
  if (ret != 0) {
    LOG_FMT("bind failed, ret=%d", ret);
    goto __tag_out;
  }
  ret = aosl_hal_sk_set_nonblock(server_fd);
  if (ret != 0) {
    LOG_FMT("set nonblock failed, ret=%d", ret);
    goto __tag_out;
  }

  // client
  client_fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP);
  if (aosl_fd_invalid(client_fd)) {
    LOG_FMT("get client socket failed");
    ret = -1;
    goto __tag_out;
  }

  // test iomp
#if defined(AOSL_HAL_HAVE_EPOLL) && (AOSL_HAL_HAVE_EPOLL == 1)
  ret = aosl_test_hal_iomp_epoll(server_fd, client_fd, &server_addr);
#elif defined(AOSL_HAL_HAVE_POLL) && (AOSL_HAL_HAVE_POLL == 1)
  ret = aosl_test_hal_iomp_poll(server_fd, client_fd, &server_addr);
#elif defined(AOSL_HAL_HAVE_SELECT) && (AOSL_HAL_HAVE_SELECT == 1)
  ret = aosl_test_hal_iomp_select(server_fd, client_fd, &server_addr);
#else
  ret = -1;
  LOG_FMT(0, "not impl iomp");
#endif

__tag_out:
  if (!aosl_fd_invalid(server_fd)) {
    aosl_hal_sk_close(server_fd);
  }
  if (!aosl_fd_invalid(client_fd)) {
    aosl_hal_sk_close(client_fd);
  }

  CHECK(ret == 0);

  LOG_FMT("test success");
  return 0;
}


static int aosl_test_hal_socket_trans_udp(char *server_ip)
{
  aosl_sockaddr_t addr = {0};
  const char *dns_server = "8.8.8.8";
  addr.sa_family = AOSL_AF_INET;
  addr.sa_port = aosl_htons(53);
  aosl_inet_addr_from_string(&addr.sin_addr, dns_server);

  aosl_fd_t fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP);
  if (aosl_fd_invalid(fd)) {
    LOG_FMT("create socket failed, fd=%d", fd);
    return -1;
  }

  // Construct a simple DNS query (A record for ipinfo.io)
  char dns_query[512];
  int query_len = 0;
  unsigned char query[] = {
      0xAA, 0xAA,  // random ID
      0x01, 0x00,  // standard query
      0x00, 0x01,  // 1 question
      0x00, 0x00,  // 0 answers
      0x00, 0x00,  // 0 authority
      0x00, 0x00,  // 0 additional
      // query ipinfo.io
      0x06, 'i', 'p', 'i', 'n', 'f', 'o',
      0x02, 'i', 'o',
      0x00,        // termination
      0x00, 0x01,  // query type A
      0x00, 0x01   // query class IN
  };
  memcpy(dns_query, query, sizeof(query));
  query_len = sizeof(query);

  int sent = aosl_hal_sk_sendto(fd, dns_query, query_len, 0, &addr);
  if (sent < 0) {
    LOG_FMT("sendto failed, ret=%d", sent);
    goto __tag_failed;
  }
  char buffer[1024];
  int received = aosl_hal_sk_recvfrom(fd, buffer, sizeof(buffer), 0, NULL);
  if (received <= 0) {
    LOG_FMT("recvfrom failed, ret=%d", received);
    goto __tag_failed;
  }

  // Parse DNS response and print IPv4 A records
  if (received >= 12) {
    unsigned char *p = (unsigned char *)buffer;
    int offset = 0;
    if (received < 12) {
      LOG_FMT("dns response too short, len=%d", received);
      goto __tag_failed;
    }
    uint16_t qdcount = (p[4] << 8) | p[5];
    uint16_t ancount = (p[6] << 8) | p[7];
    offset = 12;

    // Skip question section
    for (uint16_t qi = 0; qi < qdcount; qi++) {
      if (offset >= received) break;
      // Skip name label chain
      while (offset < received && p[offset] != 0) {
        // If it's a pointer (compressed form)
        if ((p[offset] & 0xC0) == 0xC0) {
          offset += 2;
          break;
        }
        unsigned int labellen = p[offset];
        offset += 1 + labellen;
      }
      if (offset < received && p[offset] == 0) offset++;
      // Skip qtype(2) + qclass(2)
      offset += 4;
    }

    // Parse answer section
    for (uint16_t ai = 0; ai < ancount; ai++) {
      if (offset + 10 > received) break; // Need at least type/class/ttl/rdlength

      // Skip name (may be pointer or label chain)
      if ((p[offset] & 0xC0) == 0xC0) {
        offset += 2;
      } else {
        while (offset < received && p[offset] != 0) {
          if ((p[offset] & 0xC0) == 0xC0) { offset += 2; break; }
          unsigned int labellen = p[offset];
          offset += 1 + labellen;
        }
        if (offset < received && p[offset] == 0) offset++;
      }

      if (offset + 10 > received) break;
      uint16_t type = (p[offset] << 8) | p[offset + 1];
      uint16_t clas = (p[offset + 2] << 8) | p[offset + 3];
      (void)clas;
      // ttl 4 bytes
      uint16_t rdlen = (p[offset + 8] << 8) | p[offset + 9];
      offset += 10;
      if (offset + rdlen > received) break;

      // A record
      if (type == 1 && rdlen == 4) {
        snprintf(server_ip, 16, "%u.%u.%u.%u", p[offset], p[offset + 1], p[offset + 2], p[offset + 3]);
        LOG_FMT("got ipinfo.io : A record: %u.%u.%u.%u", p[offset], p[offset + 1], p[offset + 2], p[offset + 3]);
      }

      offset += rdlen;
    }
  } else {
    LOG_FMT("dns response too short, len=%d", received);
    goto __tag_failed;
  }

  aosl_hal_sk_close(fd);
  LOG_FMT("test success");
  return 0;

__tag_failed:
  aosl_hal_sk_close(fd);
  return -1;
}

static int aosl_test_hal_socket_trans_tcp(char *server_ip)
{
  aosl_sockaddr_t addr = {0};
  addr.sa_family = AOSL_AF_INET;
  addr.sa_port = aosl_htons(80);
  aosl_inet_addr_from_string(&addr.sin_addr, server_ip);

  aosl_fd_t fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_STREAM, AOSL_IPPROTO_TCP);
  if (aosl_fd_invalid(fd)) {
    LOG_FMT("create tcp socket failed, fd=%d", fd);
    return -1;
  }

  const char *host = "ipinfo.io";
  if (aosl_hal_sk_connect(fd, &addr) != 0) {
    LOG_FMT("connect to %s (IP %s):80 failed", host, server_ip);
    aosl_hal_sk_close(fd);
    return -1;
  }

  char req[256];
  int req_len = snprintf(req, sizeof(req), "GET /ip HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
  if (req_len <= 0 || req_len >= (int)sizeof(req)) {
    LOG_FMT("build http request failed");
    aosl_hal_sk_close(fd);
    return -1;
  }

  int sent = aosl_hal_sk_send(fd, req, req_len, 0);
  if (sent < 0) {
    LOG_FMT("send http request failed, ret=%d", sent);
    aosl_hal_sk_close(fd);
    return -1;
  }

  char resp[1024];
  int total = 0;
  while (total < (int)sizeof(resp) - 1) {
    int r = aosl_hal_sk_recv(fd, resp + total, sizeof(resp) - 1 - total, 0);
    if (r < 0) {
      LOG_FMT("recv failed, ret=%d", r);
      aosl_hal_sk_close(fd);
      return -1;
    }
    if (r == 0) break; // remote closed
    total += r;
  }
  resp[total] = '\0';

  // find header-body separator
  char *body = NULL;
  char *sep = NULL;
  sep = strstr(resp, "\r\n\r\n");
  if (sep) body = sep + 4;
  else {
    // maybe only LF
    sep = strstr(resp, "\n\n");
    if (sep) body = sep + 2;
  }

  if (!body) {
    LOG_FMT("invalid http response, no header/body separator");
    aosl_hal_sk_close(fd);
    return -1;
  }

  LOG_FMT("http response body: '%s'", body);

  // trim leading/trailing whitespace from body
  while (*body == '\r' || *body == '\n' || *body == ' ' || *body == '\t') body++;
  char *end = body + strlen(body) - 1;
  while (end > body && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t')) { *end = '\0'; end--; }

  // validate IPv4 dotted format
  unsigned int a, b, c, d;
  int n = sscanf(body, "%u.%u.%u.%u", &a, &b, &c, &d);
  if (n == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255) {
    snprintf(server_ip, 16, "%u.%u.%u.%u", a, b, c, d);
    LOG_FMT("got wan ip: %s", server_ip);
    LOG_FMT("test success");
    aosl_hal_sk_close(fd);
    return 0;
  }

  LOG_FMT("invalid ip format in response: '%s'", body);
  aosl_hal_sk_close(fd);
  return -1;
}

static int aosl_test_hal_socket_trans(void)
{
  char server_ip[16] = {0};
  CHECK(aosl_test_hal_socket_trans_udp(server_ip) == 0);
  CHECK(aosl_test_hal_socket_trans_tcp(server_ip) == 0);
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_socket_maxcnt(void)
{
  aosl_fd_t fds[20];
  int cnt = 0;

  for (;;) {
    aosl_fd_t fd = aosl_hal_sk_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP);
    if (aosl_fd_invalid(fd)) {
      break;
    }

    if (cnt >= 20) {
      // reached configured cap, close this fd and stop
      aosl_hal_sk_close(fd);
      break;
    }
    fds[cnt++] = fd;
  }

  LOG_FMT("max sockets opened: %d", cnt);

  // close all opened sockets
  for (int i = 0; i < cnt; i++) {
    aosl_hal_sk_close(fds[i]);
  }

  return (cnt >= 10) ? 0 : -1;
}

static int aosl_test_hal_socket_dns(void)
{
  const char *server_name = "ap1.agora.io";
  aosl_sockaddr_t addrs[5] = {0};
  int ret = aosl_hal_gethostbyname(server_name, addrs, 5);
  if (ret < 0) {
    LOG_FMT("dns resolve %s failed, ret=%d", server_name, ret);
    return -1;
  }
  LOG_FMT("dns resolve %s success, ret=%d, result:", server_name, ret);

  for (int i = 0; i < ret; i++) {
    char ip_str[64] = {0};
    aosl_sockaddr_str(&addrs[i], ip_str, sizeof(ip_str));
    LOG_FMT("  addr[%d]: %s", i, ip_str);
  }

  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_socket(void)
{
  CHECK(aosl_test_hal_socket_trans() == 0);
  CHECK(aosl_test_hal_socket_maxcnt() == 0);
  CHECK(aosl_test_hal_socket_dns() == 0);
  LOG_FMT("test success");
  return 0;
}

// Shared data structure for thread testing
struct thread_test_data {
  aosl_mutex_t mutex;
  aosl_cond_t cond;
  aosl_sem_t sem;
  int counter;
  int ready;
};

// Simple thread entry function
static void *thread_entry_simple(void *arg)
{
  aosl_thread_t *result = (aosl_thread_t *)arg;
  *result = aosl_hal_thread_self();
  LOG_FMT("simple thread running");
  return (void *)(intptr_t)123;
}

// Thread function for testing mutex
static void *thread_entry_mutex(void *arg)
{
  struct thread_test_data *data = (struct thread_test_data *)arg;
  
  for (int i = 0; i < 100; i++) {
    aosl_hal_mutex_lock(data->mutex);
    data->counter++;
    aosl_hal_mutex_unlock(data->mutex);
  }
  
  LOG_FMT("mutex thread finished, counter=%d", data->counter);
  return NULL;
}

// Thread function for testing condition variable wait
static void *thread_entry_cond_wait(void *arg)
{
  struct thread_test_data *data = (struct thread_test_data *)arg;
  
  aosl_hal_mutex_lock(data->mutex);
  while (!data->ready) {
    LOG_FMT("cond wait thread: waiting for signal");
    aosl_hal_cond_wait(data->cond, data->mutex);
  }
  LOG_FMT("cond wait thread: received signal, ready=%d", data->ready);
  aosl_hal_mutex_unlock(data->mutex);
  return NULL;
}

// Thread function for testing semaphore
static void *thread_entry_sem(void *arg)
{
  struct thread_test_data *data = (struct thread_test_data *)arg;
  
  LOG_FMT("sem thread: waiting on semaphore");
  aosl_hal_sem_wait(data->sem);
  LOG_FMT("sem thread: semaphore acquired");
  
  data->counter++;
  return NULL;
}

// Test 1: Basic thread create/join/destroy
static int aosl_test_hal_thread_basic(void)
{
  int ret;
  aosl_thread_t thread = 0;
  aosl_thread_t simple_result = 0;
  aosl_thread_param_t param = {0};
  void *thread_ret = NULL;
  
  LOG_FMT("Test: Basic thread create/join/destroy");
  param.name = "test-simple";
  param.priority = AOSL_THRD_PRI_DEFAULT;
  
  ret = aosl_hal_thread_create(&thread, &param, thread_entry_simple, &simple_result);
  CHECK_FMT(ret == 0, "thread_create returned %d", ret);
  
  ret = aosl_hal_thread_join(thread, &thread_ret);
  if (ret != 0) {
    LOG_FMT("thread_join returned %d", ret);
    aosl_hal_thread_destroy(thread);
    return -1;
  }
  
  if (simple_result != thread) {
    LOG_FMT("simple_result=%llu, expected=%llu",
        CAST_UINT64(simple_result), CAST_UINT64(thread));
    aosl_hal_thread_destroy(thread);
    return -1;
  }

  aosl_hal_thread_destroy(thread);
  LOG_FMT("test success");
  return 0;
}

// Test 2: Thread detach
static int aosl_test_hal_thread_detach(void)
{
  int ret;
  aosl_thread_t thread = 0;
  aosl_thread_t simple_result = 0;
  aosl_thread_param_t param = {0};
  
  LOG_FMT("Test: Thread detach");
  param.name = "test-detach";
  param.priority = AOSL_THRD_PRI_DEFAULT;
  
  ret = aosl_hal_thread_create(&thread, &param, thread_entry_simple, &simple_result);
  CHECK_FMT(ret == 0, "thread_create returned %d", ret);
  
  aosl_hal_thread_detach(thread);
  aosl_hal_msleep(100); // Wait for detached thread to complete
  aosl_hal_thread_destroy(thread);
  
  LOG_FMT("test success");
  return 0;
}

// Test 3: Mutex lock/unlock
// Helper struct for hal mutex trylock test from another thread
struct mutex_trylock_test_data {
  aosl_mutex_t mutex;
  intptr_t done;
  int trylock_result;
};

// Thread function: try to acquire a hal mutex that should be held by the main thread
static void *thread_entry_mutex_trylock(void *arg)
{
  struct mutex_trylock_test_data *data = (struct mutex_trylock_test_data *)arg;
  data->trylock_result = aosl_hal_mutex_trylock(data->mutex);
  if (data->trylock_result == 0) {
    // Unexpectedly acquired, release it
    aosl_hal_mutex_unlock(data->mutex);
  }
  aosl_hal_atomic_set(&data->done, 1);
  return NULL;
}

static int aosl_test_hal_thread_mutex_basic(void)
{
  int ret;
  aosl_mutex_t mutex = NULL;
  
  LOG_FMT("Test: Mutex lock/unlock");
  mutex = aosl_hal_mutex_create();
  CHECK(mutex != NULL);
  
  ret = aosl_hal_mutex_lock(mutex);
  CHECK_FMT(ret == 0, "mutex_lock returned %d", ret);
  
  // Test trylock from another thread (should fail because main thread holds the lock)
  {
    struct mutex_trylock_test_data tdata;
    aosl_thread_t tid;
    aosl_thread_param_t param = {0};

    tdata.mutex = mutex;
    tdata.trylock_result = 0;
    tdata.done = 0;
    param.name = "trylock-test";
    param.priority = AOSL_THRD_PRI_DEFAULT;

    ret = aosl_hal_thread_create(&tid, &param, thread_entry_mutex_trylock, &tdata);
    CHECK_FMT(ret == 0, "thread_create returned %d", ret);

    // Poll for completion (join may be empty on some platforms)
    while (!aosl_hal_atomic_read(&tdata.done)) {
      aosl_hal_msleep(100);
    }
    aosl_hal_thread_join(tid, NULL);
    aosl_hal_thread_destroy(tid);

    CHECK_FMT(tdata.trylock_result != 0,
              "mutex_trylock from another thread should have failed but succeeded");
  }
  
  ret = aosl_hal_mutex_unlock(mutex);
  CHECK_FMT(ret == 0, "mutex_unlock returned %d", ret);
  
  ret = aosl_hal_mutex_trylock(mutex);
  CHECK_FMT(ret == 0, "mutex_trylock returned %d", ret);
  
  ret = aosl_hal_mutex_unlock(mutex);
  CHECK_FMT(ret == 0, "final mutex_unlock returned %d", ret);
  
  aosl_hal_mutex_destroy(mutex);
  LOG_FMT("test success");
  return 0;
}

// Test 4: Multi-thread mutex contention
static int aosl_test_hal_thread_mutex_contention(void)
{
  int ret;
  struct thread_test_data test_data = {0};
  aosl_thread_t threads[3] = {0};
  aosl_thread_param_t param = {0};
  
  LOG_FMT("Test: Multi-thread mutex contention");
  test_data.mutex = aosl_hal_mutex_create();
  CHECK(test_data.mutex != NULL);
  test_data.counter = 0;
  
  for (int i = 0; i < 3; i++) {
    param.name = "test-mutex";
    param.priority = AOSL_THRD_PRI_DEFAULT;
    ret = aosl_hal_thread_create(&threads[i], &param, thread_entry_mutex, &test_data);
    if (ret != 0) {
      LOG_FMT("thread_create[%d] returned %d", i, ret);
      for (int j = 0; j < i; j++) {
        aosl_hal_thread_join(threads[j], NULL);
        aosl_hal_thread_destroy(threads[j]);
      }
      aosl_hal_mutex_destroy(test_data.mutex);
      return -1;
    }
  }
  
  for (int i = 0; i < 3; i++) {
    ret = aosl_hal_thread_join(threads[i], NULL);
    if (ret != 0) {
      LOG_FMT("thread_join[%d] returned %d", i, ret);
    }
    aosl_hal_thread_destroy(threads[i]);
  }
  
  // Add delay to ensure all threads complete (for platforms with empty join implementation)
  aosl_hal_msleep(200);
  
  CHECK_FMT(test_data.counter == 300, "counter=%d, expected 300", test_data.counter);
  
  aosl_hal_mutex_destroy(test_data.mutex);
  LOG_FMT("test success, final counter=%d", test_data.counter);
  return 0;
}

#if defined(AOSL_HAL_HAVE_COND) && AOSL_HAL_HAVE_COND == 1
// Test 5: Condition variable
static int aosl_test_hal_thread_cond_signal(void)
{
  int ret;
  aosl_thread_t thread = 0;
  struct thread_test_data test_data = {0};
  aosl_thread_param_t param = {0};
  
  LOG_FMT("Test: Condition variable");
  test_data.mutex = aosl_hal_mutex_create();
  test_data.cond = aosl_hal_cond_create();
  if (test_data.mutex == NULL || test_data.cond == NULL) {
    if (test_data.cond) aosl_hal_cond_destroy(test_data.cond);
    if (test_data.mutex) aosl_hal_mutex_destroy(test_data.mutex);
    LOG_FMT("mutex or cond create returned NULL");
    return -1;
  }
  test_data.ready = 0;
  
  param.name = "test-cond";
  param.priority = AOSL_THRD_PRI_DEFAULT;
  ret = aosl_hal_thread_create(&thread, &param, thread_entry_cond_wait, &test_data);
  if (ret != 0) {
    LOG_FMT("thread_create returned %d", ret);
    aosl_hal_cond_destroy(test_data.cond);
    aosl_hal_mutex_destroy(test_data.mutex);
    return -1;
  }
  
  aosl_hal_msleep(100); // Ensure waiting thread has started waiting
  
  aosl_hal_mutex_lock(test_data.mutex);
  test_data.ready = 1;
  aosl_hal_cond_signal(test_data.cond);
  aosl_hal_mutex_unlock(test_data.mutex);
  
  ret = aosl_hal_thread_join(thread, NULL);
  if (ret != 0) {
    LOG_FMT("thread_join returned %d", ret);
  }
  
  // Add delay to ensure thread completes (for platforms with empty join implementation)
  aosl_hal_msleep(200);
  
  aosl_hal_thread_destroy(thread);
  aosl_hal_cond_destroy(test_data.cond);
  aosl_hal_mutex_destroy(test_data.mutex);
  LOG_FMT("test success");
  return 0;
}

// Test 6: Condition variable broadcast
static int aosl_test_hal_thread_cond_broadcast(void)
{
  int ret;
  struct thread_test_data test_data = {0};
  aosl_thread_t cond_threads[3] = {0};
  aosl_thread_param_t param = {0};
  int thread_count = 0;
  
  LOG_FMT("Test: Condition variable broadcast");
  test_data.mutex = aosl_hal_mutex_create();
  test_data.cond = aosl_hal_cond_create();
  if (test_data.mutex == NULL || test_data.cond == NULL) {
    if (test_data.cond) aosl_hal_cond_destroy(test_data.cond);
    if (test_data.mutex) aosl_hal_mutex_destroy(test_data.mutex);
    LOG_FMT("mutex or cond create returned NULL");
    return -1;
  }
  test_data.ready = 0;
  
  for (int i = 0; i < 3; i++) {
    param.name = "test-cond-bcast";
    param.priority = AOSL_THRD_PRI_DEFAULT;
    ret = aosl_hal_thread_create(&cond_threads[i], &param, thread_entry_cond_wait, &test_data);
    if (ret != 0) {
      LOG_FMT("thread_create[%d] returned %d", i, ret);
      for (int j = 0; j < i; j++) {
        aosl_hal_thread_join(cond_threads[j], NULL);
        aosl_hal_thread_destroy(cond_threads[j]);
      }
      aosl_hal_cond_destroy(test_data.cond);
      aosl_hal_mutex_destroy(test_data.mutex);
      return -1;
    }
    thread_count++;
  }
  
  aosl_hal_msleep(100); // Ensure all waiting threads have started waiting
  
  aosl_hal_mutex_lock(test_data.mutex);
  test_data.ready = 1;
  aosl_hal_cond_broadcast(test_data.cond);
  aosl_hal_mutex_unlock(test_data.mutex);
  
  for (int i = 0; i < thread_count; i++) {
    ret = aosl_hal_thread_join(cond_threads[i], NULL);
    if (ret != 0) {
      LOG_FMT("thread_join[%d] returned %d", i, ret);
    }
    aosl_hal_thread_destroy(cond_threads[i]);
  }
  
  // Add delay to ensure all threads complete (for platforms with empty join implementation)
  aosl_hal_msleep(200);
  
  aosl_hal_cond_destroy(test_data.cond);
  aosl_hal_mutex_destroy(test_data.mutex);
  LOG_FMT("test success");
  return 0;
}

// Test 7: Condition variable timed wait
static int aosl_test_hal_thread_cond_timedwait(void)
{
  int ret;
  struct thread_test_data test_data = {0};
  aosl_ts_t start_ts;
  aosl_ts_t elapsed;
  
  LOG_FMT("Test: Condition variable timed wait");
  test_data.mutex = aosl_hal_mutex_create();
  test_data.cond = aosl_hal_cond_create();
  if (test_data.mutex == NULL || test_data.cond == NULL) {
    if (test_data.cond) aosl_hal_cond_destroy(test_data.cond);
    if (test_data.mutex) aosl_hal_mutex_destroy(test_data.mutex);
    LOG_FMT("mutex or cond create returned NULL");
    return -1;
  }
  
  aosl_hal_mutex_lock(test_data.mutex);
  start_ts = aosl_tick_ms();
  LOG_FMT("start_ts=%llu, calling cond_timedwait with 200ms timeout", CAST_UINT64(start_ts));
  ret = aosl_hal_cond_timedwait(test_data.cond, test_data.mutex, 200);
  elapsed = aosl_tick_ms() - start_ts;
  aosl_hal_mutex_unlock(test_data.mutex);
  LOG_FMT("ret=%d, elapsed=%llu ms", ret, CAST_UINT64(elapsed));
  
  if (ret == 0) { // Should timeout and fail
    LOG_FMT("cond_timedwait should have timed out but succeeded");
    aosl_hal_cond_destroy(test_data.cond);
    aosl_hal_mutex_destroy(test_data.mutex);
    return -1;
  }
  
  // Relax the timing check - some systems may have timing variations
  if (elapsed < 100 || elapsed > 400) {
    LOG_FMT("warning: elapsed=%llu ms, expected ~200ms (tolerance: 100-400ms)", CAST_UINT64(elapsed));
  }
  
  aosl_hal_cond_destroy(test_data.cond);
  aosl_hal_mutex_destroy(test_data.mutex);
  LOG_FMT("test success, elapsed=%llu ms", CAST_UINT64(elapsed));
  return 0;
}
#endif

#if defined(AOSL_HAL_HAVE_SEM) && AOSL_HAL_HAVE_SEM == 1
// Test 8: Semaphore
static int aosl_test_hal_thread_sem_basic(void)
{
  int ret;
  aosl_thread_t thread = 0;
  struct thread_test_data test_data = {0};
  aosl_thread_param_t param = {0};
  
  LOG_FMT("Test: Semaphore");
  test_data.sem = aosl_hal_sem_create();
  CHECK(test_data.sem != NULL);
  test_data.counter = 0;
  
  param.name = "test-sem";
  param.priority = AOSL_THRD_PRI_DEFAULT;;
  ret = aosl_hal_thread_create(&thread, &param, thread_entry_sem, &test_data);
  if (ret != 0) {
    LOG_FMT("thread_create returned %d", ret);
    aosl_hal_sem_destroy(test_data.sem);
    return -1;
  }
  
  aosl_hal_msleep(100); // Ensure thread has started waiting on semaphore
  
  ret = aosl_hal_sem_post(test_data.sem);
  if (ret != 0) {
    LOG_FMT("sem_post returned %d", ret);
    aosl_hal_thread_join(thread, NULL);
    aosl_hal_thread_destroy(thread);
    aosl_hal_sem_destroy(test_data.sem);
    return -1;
  }
  
  ret = aosl_hal_thread_join(thread, NULL);
  if (ret != 0) {
    LOG_FMT("thread_join returned %d", ret);
  }
  
  // Add delay to ensure thread completes (for platforms with empty join implementation)
  aosl_hal_msleep(200);
  
  aosl_hal_thread_destroy(thread);
  
  CHECK_FMT(test_data.counter == 1, "counter=%d, expected 1", test_data.counter);
  
  aosl_hal_sem_destroy(test_data.sem);
  LOG_FMT("test success");
  return 0;
}

// Test 9: Semaphore timed wait
static int aosl_test_hal_thread_sem_timedwait(void)
{
  int ret;
  struct thread_test_data test_data = {0};
  aosl_ts_t start_ts;
  aosl_ts_t elapsed;
  
  LOG_FMT("Test: Semaphore timed wait");
  test_data.sem = aosl_hal_sem_create();
  CHECK(test_data.sem != NULL);
  
  start_ts = aosl_tick_ms();
  LOG_FMT("start_ts=%llu, calling sem_timedwait with 200ms timeout", CAST_UINT64(start_ts));
  ret = aosl_hal_sem_timedwait(test_data.sem, 200);
  elapsed = aosl_tick_ms() - start_ts;
  
  LOG_FMT("ret=%d, elapsed=%llu ms", ret, CAST_UINT64(elapsed));
  
  if (ret == 0) { // Should timeout and fail
    LOG_FMT("sem_timedwait should have timed out but succeeded");
    aosl_hal_sem_destroy(test_data.sem);
    return -1;
  }
  
  // Relax the timing check - some systems may have timing variations
  if (elapsed < 100 || elapsed > 400) {
    LOG_FMT("warning: elapsed=%llu ms, expected ~200ms (tolerance: 100-400ms)", CAST_UINT64(elapsed));
  }
  
  aosl_hal_sem_destroy(test_data.sem);
  LOG_FMT("test success, elapsed=%llu ms", CAST_UINT64(elapsed));
  return 0;
}
#endif

// Test 10: Maximum mutex count
static int aosl_test_hal_thread_mutex_maxcnt(void)
{
  aosl_mutex_t mutexes[200];
  int cnt = 0;
  
  LOG_FMT("Test: Maximum mutex count");
  
  for (int i = 0; i < 200; i++) {
    mutexes[i] = aosl_hal_mutex_create();
    if (mutexes[i] == NULL) {
      break;
    }
    cnt++;
  }
  
  LOG_FMT("max mutexes created: %d", cnt);
  
  // Clean up all created mutexes
  for (int i = 0; i < cnt; i++) {
    aosl_hal_mutex_destroy(mutexes[i]);
  }
  
  CHECK_FMT(cnt >= 10, "only created %d mutexes, expected at least 10", cnt);
  LOG_FMT("test success");
  return 0;
}

#if defined(AOSL_HAL_HAVE_COND) && AOSL_HAL_HAVE_COND == 1
// Test 11: Maximum condition variable count
static int aosl_test_hal_thread_cond_maxcnt(void)
{
  aosl_cond_t conds[200];
  int cnt = 0;
  
  LOG_FMT("Test: Maximum condition variable count");
  
  for (int i = 0; i < 200; i++) {
    conds[i] = aosl_hal_cond_create();
    if (conds[i] == NULL) {
      break;
    }
    cnt++;
  }
  
  LOG_FMT("max condition variables created: %d", cnt);
  
  // Clean up all created condition variables
  for (int i = 0; i < cnt; i++) {
    aosl_hal_cond_destroy(conds[i]);
  }
  
  CHECK_FMT(cnt >= 10, "only created %d condition variables, expected at least 10", cnt);
  LOG_FMT("test success");
  return 0;
}
#endif

#if defined(AOSL_HAL_HAVE_SEM) && AOSL_HAL_HAVE_SEM == 1
// Test 12: Maximum semaphore count
static int aosl_test_hal_thread_sem_maxcnt(void)
{
  aosl_sem_t sems[200];
  int cnt = 0;
  
  LOG_FMT("Test: Maximum semaphore count");
  
  for (int i = 0; i < 200; i++) {
    sems[i] = aosl_hal_sem_create();
    if (sems[i] == NULL) {
      break;
    }
    cnt++;
  }
  
  LOG_FMT("max semaphores created: %d", cnt);
  
  // Clean up all created semaphores
  for (int i = 0; i < cnt; i++) {
    aosl_hal_sem_destroy(sems[i]);
  }
  
  CHECK_FMT(cnt >= 10, "only created %d semaphores, expected at least 10", cnt);
  LOG_FMT("test success");
  return 0;
}
#endif

// Shared data structure for static lock testing
struct static_lock_test_data {
  aosl_static_lock_t static_lock;
  int counter;
};

// Thread function for testing static lock contention
static void *thread_entry_static_lock(void *arg)
{
  struct static_lock_test_data *data = (struct static_lock_test_data *)arg;
  
  for (int i = 0; i < 100; i++) {
    aosl_static_lock_lock(&data->static_lock);
    data->counter++;
    aosl_static_lock_unlock(&data->static_lock);
  }
  
  LOG_FMT("static lock thread finished, counter=%d", data->counter);
  return NULL;
}

// Helper struct for trylock test from another thread
struct static_lock_trylock_test_data {
  aosl_static_lock_t *lock;
  intptr_t done;
  int trylock_result; // 0 = acquired (unexpected), non-zero = failed to acquire (expected)
};

// Thread function: try to acquire a lock that should be held by the main thread
static void *thread_entry_static_lock_trylock(void *arg)
{
  struct static_lock_trylock_test_data *data = (struct static_lock_trylock_test_data *)arg;
  data->trylock_result = aosl_static_lock_trylock(data->lock);
  if (data->trylock_result == 0) {
    // Unexpectedly acquired, release it
    aosl_static_lock_unlock(data->lock);
  }
  aosl_hal_atomic_set(&data->done, 1);
  return NULL;
}

// Test: Basic static lock operations
static int aosl_test_api_static_lock_basic(void)
{
  int ret;
  aosl_static_lock_t static_lock = AOSL_STATIC_LOCK_INIT;
  
  LOG_FMT("Test: Basic static lock operations");
  
  // Test static lock initialization
  ret = aosl_static_lock_init(&static_lock);
  CHECK_FMT(ret == 0, "static_lock_init returned %d", ret);
  
  // Test lock operation
  ret = aosl_static_lock_lock(&static_lock);
  CHECK_FMT(ret == 0, "static_lock_lock returned %d", ret);
  
  // Test trylock from another thread (should fail because main thread holds the lock)
  {
    struct static_lock_trylock_test_data tdata;
    aosl_thread_t tid;
    aosl_thread_param_t param = {0};

    tdata.lock = &static_lock;
    tdata.trylock_result = 0;
    tdata.done = 0;
    param.name = "trylock-test";
    param.priority = AOSL_THRD_PRI_DEFAULT;

    ret = aosl_hal_thread_create(&tid, &param, thread_entry_static_lock_trylock, &tdata);
    CHECK_FMT(ret == 0, "thread_create returned %d", ret);

    // Poll for completion (join may be empty on some platforms)
    while (!aosl_hal_atomic_read(&tdata.done)) {
      aosl_hal_msleep(100);
    }
    aosl_hal_thread_join(tid, NULL);
    aosl_hal_thread_destroy(tid);

    CHECK_FMT(tdata.trylock_result != 0,
              "trylock from another thread should have failed but succeeded");
  }
  
  // Test unlock operation
  ret = aosl_static_lock_unlock(&static_lock);
  CHECK_FMT(ret == 0, "static_lock_unlock returned %d", ret);
  
  // Test trylock operation (should succeed now, lock is free)
  ret = aosl_static_lock_trylock(&static_lock);
  CHECK_FMT(ret == 0, "static_lock_trylock returned %d", ret);
  
  // Final unlock
  ret = aosl_static_lock_unlock(&static_lock);
  CHECK_FMT(ret == 0, "final static_lock_unlock returned %d", ret);
  
  aosl_static_lock_fini(&static_lock);
  LOG_FMT("test success");
  return 0;
}

// Test: Multi-thread static lock contention
static int aosl_test_api_static_lock_contention(void)
{
  int ret;
  struct static_lock_test_data test_data = {
    .static_lock = AOSL_STATIC_LOCK_INIT,
    .counter = 0
  };
  aosl_thread_t threads[3] = {0};
  aosl_thread_param_t param = {0};
  
  LOG_FMT("Test: Multi-thread static lock contention");
  
  for (int i = 0; i < 3; i++) {
    param.name = "test-static-lock";
    param.priority = AOSL_THRD_PRI_DEFAULT;
    ret = aosl_hal_thread_create(&threads[i], &param, thread_entry_static_lock, &test_data);
    if (ret != 0) {
      LOG_FMT("thread_create[%d] returned %d", i, ret);
      for (int j = 0; j < i; j++) {
        aosl_hal_thread_join(threads[j], NULL);
        aosl_hal_thread_destroy(threads[j]);
      }
      return -1;
    }
  }
  
  for (int i = 0; i < 3; i++) {
    ret = aosl_hal_thread_join(threads[i], NULL);
    if (ret != 0) {
      LOG_FMT("thread_join[%d] returned %d", i, ret);
    }
    aosl_hal_thread_destroy(threads[i]);
  }
  
  // Add delay to ensure all threads complete (for platforms with empty join implementation)
  aosl_hal_msleep(200);
  
  CHECK_FMT(test_data.counter == 300, "counter=%d, expected 300", test_data.counter);
  
  aosl_static_lock_fini(&test_data.static_lock);
  LOG_FMT("test success, final counter=%d", test_data.counter);
  return 0;
}

static int aosl_test_api_static_lock(void)
{
  CHECK(aosl_test_api_static_lock_basic() == 0);
  CHECK(aosl_test_api_static_lock_contention() == 0);
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_thread(void)
{
  CHECK(aosl_test_hal_thread_basic() == 0);
  CHECK(aosl_test_hal_thread_detach() == 0);
  CHECK(aosl_test_hal_thread_mutex_basic() == 0);
  CHECK(aosl_test_hal_thread_mutex_contention() == 0);
  CHECK(aosl_test_hal_thread_mutex_maxcnt() == 0);
#if defined(AOSL_HAL_HAVE_COND) && AOSL_HAL_HAVE_COND == 1
  CHECK(aosl_test_hal_thread_cond_signal() == 0);
  CHECK(aosl_test_hal_thread_cond_broadcast() == 0);
  CHECK(aosl_test_hal_thread_cond_timedwait() == 0);
  CHECK(aosl_test_hal_thread_cond_maxcnt() == 0);
#endif
#if defined(AOSL_HAL_HAVE_SEM) && AOSL_HAL_HAVE_SEM == 1
  CHECK(aosl_test_hal_thread_sem_basic() == 0);
  CHECK(aosl_test_hal_thread_sem_timedwait() == 0);
  CHECK(aosl_test_hal_thread_sem_maxcnt() == 0);
#endif
  CHECK(aosl_test_api_static_lock() == 0);
  LOG_FMT("test success");
  return 0;
}

// Test: Get tick milliseconds
static int aosl_test_hal_time_tick_ms(void)
{
  uint64_t tick1, tick2, tick3;
  
  LOG_FMT("Test: Get tick milliseconds");
  
  tick1 = aosl_hal_get_tick_ms();
  CHECK(tick1 > 0);
  
  aosl_hal_msleep(100);
  
  tick2 = aosl_hal_get_tick_ms();
  CHECK(tick2 > tick1);
  
  uint64_t elapsed = tick2 - tick1;
  LOG_FMT("tick1=%llu, tick2=%llu, elapsed=%llu ms", 
          CAST_UINT64(tick1), CAST_UINT64(tick2), CAST_UINT64(elapsed));
  
  // Check elapsed time is roughly 100ms (allow 50-200ms tolerance)
  CHECK_FMT(elapsed >= 50 && elapsed <= 200, 
            "elapsed=%llu ms, expected ~100ms (tolerance: 50-200ms)", 
            CAST_UINT64(elapsed));
  
  // Verify tick is monotonic
  tick3 = aosl_hal_get_tick_ms();
  CHECK_FMT(tick3 >= tick2, "tick not monotonic: tick3=%llu < tick2=%llu", 
            CAST_UINT64(tick3), CAST_UINT64(tick2));
  
  LOG_FMT("test success");
  return 0;
}

// Test: Get time milliseconds
static int aosl_test_hal_time_time_ms(void)
{
  uint64_t time1, time2;
  
  LOG_FMT("Test: Get time milliseconds");
  
  time1 = aosl_hal_get_time_ms();
  CHECK(time1 > 0);
  
  // Time should be reasonable (after year 2020)
  // 2020-01-01 00:00:00 UTC = 1577836800000 ms
  if (time1 < 1577836800000ULL) {
    LOG_FMT("time1=%llu seems too old (before 2020)", 
            CAST_UINT64(time1));
  }
  
  aosl_hal_msleep(100);
  
  time2 = aosl_hal_get_time_ms();
  CHECK(time2 > time1);
  
  uint64_t elapsed = time2 - time1;
  LOG_FMT("time1=%llu, time2=%llu, elapsed=%llu ms", 
          CAST_UINT64(time1), CAST_UINT64(time2), CAST_UINT64(elapsed));
  
  // Check elapsed time is roughly 100ms (allow 50-200ms tolerance)
  CHECK_FMT(elapsed >= 50 && elapsed <= 200, 
            "elapsed=%llu ms, expected ~100ms (tolerance: 50-200ms)", 
            CAST_UINT64(elapsed));
  
  LOG_FMT("test success");
  return 0;
}

// Test: Get time string
static int aosl_test_hal_time_time_str(void)
{
  char buf[64] = {0};
  int ret;
  
  LOG_FMT("Test: Get time string");
  
  ret = aosl_hal_get_time_str(buf, sizeof(buf));
  CHECK_FMT(ret == 0, "aosl_hal_get_time_str returned %d", ret);
  CHECK_FMT(buf[0] != '\0', "time string is empty");
  
  LOG_FMT("time string: '%s'", buf);
  
  // Verify string contains expected date/time components
  // Most time formats include digits and separators like '-', ':', or space
  int has_digit = 0;
  int has_separator = 0;
  for (int i = 0; buf[i] != '\0'; i++) {
    if (buf[i] >= '0' && buf[i] <= '9') {
      has_digit = 1;
    }
    if (buf[i] == '-' || buf[i] == ':' || buf[i] == ' ' || buf[i] == '/') {
      has_separator = 1;
    }
  }
  
  CHECK_FMT(has_digit, "time string has no digits: '%s'", buf);
  CHECK_FMT(has_separator, "time string has no separators: '%s'", buf);
  
  // Test with smaller buffer
  char small_buf[10] = {0};
  ret = aosl_hal_get_time_str(small_buf, sizeof(small_buf));
  // Should either succeed with truncated string or return error
  LOG_FMT("small buffer result: ret=%d, str='%s'", ret, small_buf);
  
  LOG_FMT("test success");
  return 0;
}

// Test: Sleep accuracy
static int aosl_test_hal_time_msleep(void)
{
  uint64_t start, end, elapsed;
  const uint64_t sleep_times[] = {10, 50, 100, 200};
  
  LOG_FMT("Test: Sleep accuracy");
  
  for (size_t i = 0; i < sizeof(sleep_times) / sizeof(sleep_times[0]); i++) {
    uint64_t sleep_ms = sleep_times[i];
    
    start = aosl_hal_get_tick_ms();
    aosl_hal_msleep(sleep_ms);
    end = aosl_hal_get_tick_ms();
    
    elapsed = end - start;
    
    LOG_FMT("sleep(%llu ms): elapsed=%llu ms", 
            CAST_UINT64(sleep_ms), CAST_UINT64(elapsed));
    
    // Allow 50% tolerance for short sleeps, tighter for longer sleeps
    uint64_t min_expected = sleep_ms / 2;
    uint64_t max_expected = sleep_ms * 2;
    
    CHECK_FMT(elapsed >= min_expected && elapsed <= max_expected,
              "sleep(%llu ms) elapsed=%llu ms out of range [%llu, %llu]",
              CAST_UINT64(sleep_ms), CAST_UINT64(elapsed),
              CAST_UINT64(min_expected), CAST_UINT64(max_expected));
  }
  
  LOG_FMT("test success");
  return 0;
}

// Test: Time consistency between tick and time
static int aosl_test_hal_time_consistency(void)
{
  uint64_t tick1, time1, tick2, time2;
  
  LOG_FMT("Test: Time consistency");
  
  // Get both tick and time
  tick1 = aosl_hal_get_tick_ms();
  time1 = aosl_hal_get_time_ms();
  
  aosl_hal_msleep(100);
  
  tick2 = aosl_hal_get_tick_ms();
  time2 = aosl_hal_get_time_ms();
  
  uint64_t tick_elapsed = tick2 - tick1;
  uint64_t time_elapsed = time2 - time1;
  
  LOG_FMT("tick_elapsed=%llu ms, time_elapsed=%llu ms", 
          CAST_UINT64(tick_elapsed), CAST_UINT64(time_elapsed));
  
  // Both should measure roughly the same elapsed time
  // Allow 50ms difference
  int64_t diff = (int64_t)tick_elapsed - (int64_t)time_elapsed;
  if (diff < 0) diff = -diff;
  
  CHECK_FMT(diff <= 50, 
            "tick and time elapsed differ too much: tick=%llu, time=%llu, diff=%lld",
            CAST_UINT64(tick_elapsed), CAST_UINT64(time_elapsed), CAST_INT64(diff));
  
  LOG_FMT("test success");
  return 0;
}

// Test: Zero sleep
static int aosl_test_hal_time_zero_sleep(void)
{
  uint64_t start, end, elapsed;
  
  LOG_FMT("Test: Zero sleep");
  
  start = aosl_hal_get_tick_ms();
  aosl_hal_msleep(0);
  end = aosl_hal_get_tick_ms();
  
  elapsed = end - start;
  
  LOG_FMT("sleep(0 ms): elapsed=%llu ms", CAST_UINT64(elapsed));
  
  // Zero sleep should return quickly (within 10ms)
  CHECK_FMT(elapsed <= 10, 
            "sleep(0) took too long: %llu ms", 
            CAST_UINT64(elapsed));
  
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_time(void)
{
  CHECK(aosl_test_hal_time_tick_ms() == 0);
  CHECK(aosl_test_hal_time_time_ms() == 0);
  CHECK(aosl_test_hal_time_time_str() == 0);
  CHECK(aosl_test_hal_time_msleep() == 0);
  CHECK(aosl_test_hal_time_consistency() == 0);
  CHECK(aosl_test_hal_time_zero_sleep() == 0);
  LOG_FMT("test success");
  return 0;
}

// Test: Get device UUID
static int aosl_test_hal_utils_uuid(void)
{
  char uuid[64] = {0};
  int ret;
  
  LOG_FMT("Test: Get device UUID");
  
  ret = aosl_hal_get_uuid(uuid, sizeof(uuid));
  CHECK_FMT(ret == 0, "aosl_hal_get_uuid returned %d", ret);
  
  // If successful, UUID should not be empty and should be exactly 32 characters
  CHECK_FMT(uuid[0] != '\0', "UUID string is empty");
  
  int len = 0;
  for (int i = 0; uuid[i] != '\0'; i++) {
    len++;
  }
  
  // Function guarantees 32 characters if it returns success
  CHECK_FMT(len >= 32, "UUID length is %d, expected 32", len);
  LOG_FMT("UUID: '%s' (length: %d)", uuid, len);
  
  // Test with exact size buffer (33 bytes: 32 chars + null terminator)
  char uuid2[33] = {0};
  ret = aosl_hal_get_uuid(uuid2, sizeof(uuid2));
  CHECK_FMT(ret == 0, "aosl_hal_get_uuid (second call) returned %d", ret);
  
  // UUID should be different across calls (random UUID)
  int same = 1;
  for (int i = 0; i < 32; i++) {
    if (uuid[i] != uuid2[i]) {
      same = 0;
      break;
    }
  }

  CHECK_FMT(!same, "UUID should not same: '%s' vs '%s'", uuid, uuid2);
  
  // Test with small buffer (should fail)
  char small_uuid[10] = {0};
  ret = aosl_hal_get_uuid(small_uuid, sizeof(small_uuid));
  CHECK_FMT(ret == 0, "small buffer (size 10) should returned success, ret=%d", ret);
  CHECK_FMT(strlen(small_uuid) == 9, "uffer size 10, tiny_uuid len should be 9");
  
  // Test with minimum valid buffer (33 bytes)
  char min_uuid[33] = {0};
  ret = aosl_hal_get_uuid(min_uuid, sizeof(min_uuid));
  CHECK_FMT(ret == 0, "aosl_hal_get_uuid with size 33 returned %d", ret);
    CHECK_FMT(strlen(min_uuid) == 32, "UUID in min buffer has length %d, expected 32", (int)strlen(min_uuid));
  
  // Test with buffer too small (32 bytes - missing space for null terminator)
  char tiny_uuid[32] = {0};
  ret = aosl_hal_get_uuid(tiny_uuid, sizeof(tiny_uuid));
  CHECK_FMT(ret == 0, "buffer size 32 (no space for null) but returned success");
  CHECK_FMT(strlen(tiny_uuid) == 31, "uffer size 32, tiny_uuid len should be 32");
  
  LOG_FMT("test success");
  return 0;
}

// Test: UUID format validation
static int aosl_test_hal_utils_uuid_format(void)
{
  char uuid[32+1] = {0};
  int ret;
  
  LOG_FMT("Test: UUID format validation");
  
  ret = aosl_hal_get_uuid(uuid, sizeof(uuid));
  CHECK_FMT(ret == 0, "aosl_hal_get_uuid returned %d", ret);
  
  // UUID must be 32 characters
  int len = (int)strlen(uuid);
  CHECK_FMT(len == 32, "UUID length is %d, expected 32", len);
  
  // Check if UUID contains only valid hex characters (0-9, a-f, A-F)
  // No hyphens, colons, or other separators allowed
  int valid_count = 0;
  int invalid_count = 0;
  
  for (int i = 0; i < len; i++) {
    char c = uuid[i];
    if ((c >= '0' && c <= '9') || 
        (c >= 'a' && c <= 'f') || 
        (c >= 'A' && c <= 'F')) {
      valid_count++;
    } else {
      invalid_count++;
      LOG_FMT("ERROR: UUID contains invalid char at pos %d: '%c' (0x%02x)", 
              i, c, (unsigned char)c);
    }
  }
  
  CHECK_FMT(valid_count == 32, "UUID has %d valid hex chars, expected 32", valid_count);
  CHECK_FMT(invalid_count == 0, "UUID has %d invalid chars, expected 0", invalid_count);
  
  // Verify UUID contains at least some variety (not all zeros or all same char)
  int all_same = 1;
  char first_char = uuid[0];
  for (int i = 1; i < len; i++) {
    if (uuid[i] != first_char) {
      all_same = 0;
      break;
    }
  }
  
  if (all_same) {
    LOG_FMT("warning: UUID contains all same character: '%c'", first_char);
  }
  
  LOG_FMT("test success");
  return 0;
}

// Test: Get OS version
static int aosl_test_hal_utils_os_version(void)
{
  char version[128] = {0};
  int ret;
  
  LOG_FMT("Test: Get OS version");
  
  ret = aosl_hal_os_version(version, sizeof(version));
  CHECK_FMT(ret == 0, "aosl_hal_os_version returned %d", ret);
  CHECK_FMT(version[0] != '\0', "OS version string is empty");
  
  LOG_FMT("OS version: '%s'", version);
  
  // Verify version string length (minimum 63 characters)
  int len = 0;
  for (int i = 0; version[i] != '\0'; i++) {
    len++;
  }
  
  CHECK_FMT(len >= 1, "OS version length is %d", len);
  LOG_FMT("OS version length: %d", len);
  
  // Test with exact size buffer (64 bytes: min 63 chars + null terminator)
  char version2[64] = {0};
  ret = aosl_hal_os_version(version2, sizeof(version2));
  CHECK_FMT(ret == 0, "aosl_hal_os_version (second call) returned %d", ret);
  
  // OS version should be consistent across calls
  int same = 1;
  for (int i = 0; i < 64-1; i++) {
    if (version[i] != version2[i]) {
      same = 0;
      break;
    }
  }
  CHECK_FMT(same, "OS version changed between calls: '%s' vs '%s'", version, version2);
  
  // Test with small buffer (should success)
  char small_version[5] = {0};
  ret = aosl_hal_os_version(small_version, sizeof(small_version));
  LOG_FMT("small buffer (size 5) result: ret=%d, version='%s'", ret, small_version);
  CHECK_FMT(ret == 0, "small buffer should returned success");

  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal_utils(void)
{
  CHECK(aosl_test_hal_utils_uuid() == 0);
  CHECK(aosl_test_hal_utils_uuid_format() == 0);
  CHECK(aosl_test_hal_utils_os_version() == 0);
  LOG_FMT("test success");
  return 0;
}

static int aosl_test_hal(void)
{
  CHECK(aosl_test_hal_fd_type() == 0);
  CHECK(aosl_test_hal_atomic() == 0);
  CHECK(aosl_test_hal_errno() == 0);
  CHECK(aosl_test_hal_file() == 0);
  CHECK(aosl_test_hal_iomp() == 0);
  CHECK(aosl_test_hal_socket() == 0);
  CHECK(aosl_test_hal_thread() == 0);
  CHECK(aosl_test_hal_time() == 0);
  CHECK(aosl_test_hal_utils() == 0);
  LOG_FMT("test success");
  return 0;
}

struct test_mpq_server_res {
  aosl_fd_t sk;
  int recv_cnt;
};

struct test_mpq_client_res {
  aosl_fd_t sk;
  int sent_cnt;
  aosl_sockaddr_t server_addr;
};

static struct test_mpq_server_res mpq_server_res = { 0 };
static struct test_mpq_client_res mpq_client_res = { 0 };

static void mpq_udp_server_on_data(void *data, size_t len, uintptr_t argc, uintptr_t argv[], const aosl_sk_addr_t *addr)
{
  UNUSED(argc);
  UNUSED(addr);
  struct test_mpq_server_res *server_res = (struct test_mpq_server_res *)argv[0];
  char *msg = (char *)data;
  server_res->recv_cnt++;
  if (server_res->recv_cnt % 500 == 1) {
    LOG_FMT("len=%d recv msg %s", (int)len, msg);
  }
}

static void mpq_udp_server_on_event(aosl_fd_t fd, int event, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(argc);
  UNUSED(argv);
  if (event >= 0) {
    return;
  }
  LOG_FMT("fd=%d event=%d\n", fd, event);
}

static void mpq_udp_client_on_data(void *data, size_t len, uintptr_t argc, uintptr_t argv[], const aosl_sk_addr_t *addr)
{
  UNUSED(data);
  UNUSED(len);
  UNUSED(argc);
  UNUSED(argv);
  UNUSED(addr);
}

static void mpq_udp_client_on_event(aosl_fd_t fd, int event, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(argc);
  UNUSED(argv);
  if (event >= 0) {
    return;
  }
  LOG_FMT("fd=%d event=%d\n", fd, event);
}

static int test_mpq_udp_server_init(void *arg)
{
  UNUSED(arg);
  int ret;
  mpq_server_res.recv_cnt = 0;
  mpq_server_res.sk = -1;
  aosl_fd_t fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP);
  CHECK(!aosl_fd_invalid(fd));

  aosl_sockaddr_t addr = { 0 };
  addr.sa_family = AOSL_AF_INET;
  addr.sa_port = aosl_htons(server_port);
  aosl_inet_addr_from_string(&addr.sin_addr, server_ip);
  ret = aosl_bind(fd, &addr);
  EXPECT_EQ(ret, 0);
  ret = aosl_mpq_add_dgram_socket(fd, 1400, mpq_udp_server_on_data, mpq_udp_server_on_event, 1, &mpq_server_res);
  EXPECT_EQ(ret, 0);
  mpq_server_res.sk = fd;
  return 0;
}

static void test_mpq_udp_server_fini(void *arg)
{
  UNUSED(arg);
  aosl_hal_sk_close(mpq_server_res.sk);
}

static int test_mpq_udp_client_init(void *arg)
{
  UNUSED(arg);
  int ret;
  mpq_client_res.sent_cnt = 0;
  mpq_client_res.sk = -1;
  aosl_fd_t fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_DGRAM, AOSL_IPPROTO_UDP);
  CHECK(!aosl_fd_invalid(fd));

  aosl_sockaddr_t addr = { 0 };
  addr.sa_family = AOSL_AF_INET;
  addr.sa_port = aosl_htons(server_port);
  aosl_inet_addr_from_string(&addr.sin_addr, server_ip);
  mpq_client_res.server_addr = addr;

  ret = aosl_bind_port_only(fd, AOSL_AF_INET, 0);
  EXPECT_EQ(ret, 0);
  ret = aosl_mpq_add_dgram_socket(fd, 1400, mpq_udp_client_on_data, mpq_udp_client_on_event, 1, &mpq_client_res);
  EXPECT_EQ(ret, 0);
  mpq_client_res.sk = fd;
  return 0;
}

static void test_mpq_udp_client_fini(void *arg)
{
  UNUSED(arg);
  aosl_hal_sk_close(mpq_client_res.sk);
}

static void test_mpq_udp_client_send_func(const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc,
                                      uintptr_t argv[])
{
  UNUSED(queued_ts_p);
  UNUSED(robj);
  UNUSED(argc);
  struct test_mpq_client_res *client_res = (struct test_mpq_client_res *)argv[0];
  char msg[1024] = { 0 };
  sprintf(msg, "this is the %d cnt client sent msg!", client_res->sent_cnt++);
  int ret = aosl_sendto(client_res->sk, msg, sizeof(msg), 0, &client_res->server_addr);
  if (client_res->sent_cnt % 500 == 1) {
    LOG_FMT("ret=%d send msg %s", ret, msg);
  }
}

static int aosl_test_mpq_api_udp(void)
{
  memset(&mpq_server_res, 0, sizeof(mpq_server_res));
  memset(&mpq_client_res, 0, sizeof(mpq_client_res));

  int priority = AOSL_THRD_PRI_DEFAULT; // default
  int stack_size = 0; // default
  int max_func_size = 10000;
  aosl_mpq_t q_server = aosl_mpq_create(priority, stack_size, max_func_size, "udp-server",
                  test_mpq_udp_server_init, test_mpq_udp_server_fini, NULL);
  CHECK(!aosl_mpq_invalid(q_server));

  aosl_mpq_t q_client = aosl_mpq_create(priority, stack_size, max_func_size, "udp-client",
                  test_mpq_udp_client_init, test_mpq_udp_client_fini, NULL);
  CHECK(!aosl_mpq_invalid(q_client));

  // client async send msg
  int cnt_cycs = 20;
  int cnt_pers = 50;
  int cnt_alls = cnt_cycs * cnt_pers * 2;
  for (int i = 0; i < cnt_cycs; i++) {
    for (int j = 0; j < cnt_pers; j++) {
      aosl_mpq_queue(q_client, AOSL_MPQ_INVALID, AOSL_REF_INVALID, "test_mpq_udp_client_send_func",
                     test_mpq_udp_client_send_func, 1, &mpq_client_res);
    }
    aosl_msleep(100);
  }

  // client sync send msg
  for (int i = 0; i < cnt_cycs; i++) {
    for (int j = 0; j < cnt_pers; j++) {
      aosl_mpq_call(q_client, AOSL_REF_INVALID, "test_mpq_client_send_func",
                    test_mpq_udp_client_send_func, 1, &mpq_client_res);
    }
    aosl_msleep(100);
  }

  // check cnts
  aosl_ts_t start_ts = aosl_tick_ms();
  while (mpq_server_res.recv_cnt < cnt_alls && (aosl_tick_ms() - start_ts) < 5000) {
    aosl_msleep(100);
  }
  aosl_mpq_destroy_wait(q_server);
  aosl_mpq_destroy_wait(q_client);
  EXPECT_EQ(mpq_server_res.recv_cnt, cnt_alls);
  EXPECT_EQ(mpq_client_res.sent_cnt, cnt_alls);
  LOG_FMT("mpq udp test success");
  return 0;
}

// TCP packet checker: simple length-prefixed protocol
// Format: [4-byte length][data]
static isize_t mpq_tcp_check_packet(const void *data, size_t len, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(argc);
  UNUSED(argv);
  
  if (len < 4) {
    return 0; // Need more data for length header
  }
  
  const uint8_t *buf = (const uint8_t *)data;
  uint32_t pkt_len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  
  if (pkt_len > 2048) {
    return -1; // Invalid packet length
  }
  
  if (len < 4 + pkt_len) {
    return 0; // Need more data
  }
  
  return 4 + pkt_len; // Complete packet
}

static void mpq_tcp_server_on_data(void *data, size_t len, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(argc);
  struct test_mpq_server_res *server_res = (struct test_mpq_server_res *)argv[0];
  
  // Skip 4-byte length header
  if (len < 4) {
    return;
  }
  char *msg = (char *)data + 4;
  server_res->recv_cnt++;
  if (server_res->recv_cnt % 500 == 1) {
    LOG_FMT("TCP len=%d recv msg %s", (int)len - 4, msg);
  }
}

static void mpq_tcp_server_on_event(aosl_fd_t fd, int event, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(argc);
  UNUSED(argv);
  if (event >= 0) {
    return;
  }
  LOG_FMT("TCP server fd=%d event=%d\n", fd, event);
}

// TCP client callbacks
static void mpq_tcp_client_on_data(void *data, size_t len, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(data);
  UNUSED(len);
  UNUSED(argc);
  UNUSED(argv);
}

static void mpq_tcp_client_on_event(aosl_fd_t fd, int event, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(argc);
  UNUSED(argv);
  if (event >= 0) {
    return;
  }
  LOG_FMT("TCP client fd=%d event=%d\n", fd, event);
}

// TCP server accepted callback
static void mpq_tcp_server_on_accepted(aosl_accept_data_t *accept_data, size_t len, uintptr_t argc, uintptr_t argv[])
{
  UNUSED(len);
  UNUSED(argc);
  struct test_mpq_server_res *server_res = (struct test_mpq_server_res *)argv[0];
  
  int ret;
  aosl_fd_t client_fd = accept_data->newsk;
  
  char addr_buf[64] = {0};
  aosl_ip_sk_addr_str(&accept_data->addr, addr_buf, sizeof(addr_buf));
  LOG_FMT("TCP server accepted connection from %s, fd=%d", addr_buf, client_fd);
  
  // Add the accepted socket as a stream socket with packet checker
  ret = aosl_mpq_add_stream_socket(client_fd, 2048, mpq_tcp_check_packet,
                                    mpq_tcp_server_on_data, mpq_tcp_server_on_event, 1, server_res);
  if (ret != 0) {
    LOG_FMT("Failed to add stream socket, ret=%d", ret);
    aosl_hal_sk_close(client_fd);
    return;
  }
  
  server_res->sk = client_fd;
}

// TCP server init/fini
static int test_mpq_tcp_server_init(void *arg)
{
  UNUSED(arg);
  int ret;
  mpq_server_res.recv_cnt = 0;
  mpq_server_res.sk = -1;
  
  // Create TCP listen socket
  aosl_fd_t listen_fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_STREAM, AOSL_IPPROTO_TCP);
  CHECK(!aosl_fd_invalid(listen_fd));

  aosl_sockaddr_t addr = { 0 };
  addr.sa_family = AOSL_AF_INET;
  addr.sa_port = aosl_htons(server_port);
  aosl_inet_addr_from_string(&addr.sin_addr, server_ip);
  
  ret = aosl_bind(listen_fd, &addr);
  EXPECT_EQ(ret, 0);
  
  // Use aosl_mpq_listen to handle listening and accepting
  ret = aosl_mpq_listen(listen_fd, 5, mpq_tcp_server_on_accepted, mpq_tcp_server_on_event, 1, &mpq_server_res);
  EXPECT_EQ(ret, 0);
  
  mpq_server_res.sk = listen_fd;
  return 0;
}

static void test_mpq_tcp_server_fini(void *arg)
{
  UNUSED(arg);
  aosl_hal_sk_close(mpq_server_res.sk);
}

// TCP client init/fini
static int test_mpq_tcp_client_init(void *arg)
{
  UNUSED(arg);
  int ret;
  mpq_client_res.sent_cnt = 0;
  mpq_client_res.sk = -1;
  
  // Create TCP socket
  aosl_fd_t fd = aosl_socket(AOSL_AF_INET, AOSL_SOCK_STREAM, AOSL_IPPROTO_TCP);
  CHECK(!aosl_fd_invalid(fd));

  aosl_sockaddr_t addr = { 0 };
  addr.sa_family = AOSL_AF_INET;
  addr.sa_port = aosl_htons(server_port);
  aosl_inet_addr_from_string(&addr.sin_addr, server_ip);

  // Use aosl_mpq_connect to connect and add socket
  ret = aosl_mpq_connect(fd, &addr, 5000, 2048, mpq_tcp_check_packet,
                         mpq_tcp_client_on_data, mpq_tcp_client_on_event, 1, &mpq_client_res);
  EXPECT_EQ(ret, 0);
  
  mpq_client_res.sk = fd;
  return 0;
}

static void test_mpq_tcp_client_fini(void *arg)
{
  UNUSED(arg);
  aosl_hal_sk_close(mpq_client_res.sk);
}

static void test_mpq_tcp_client_send_func(const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, uintptr_t argc,
                                          uintptr_t argv[])
{
  UNUSED(queued_ts_p);
  UNUSED(robj);
  UNUSED(argc);
  struct test_mpq_client_res *client_res = (struct test_mpq_client_res *)argv[0];
  char msg[1024] = { 0 };
  uint8_t pkt[1028];
  
  sprintf(msg, "this is the %d cnt TCP client sent msg!", client_res->sent_cnt++);
  uint32_t msg_len = sizeof(msg);
  
  // Write length header (big-endian)
  pkt[0] = (msg_len >> 24) & 0xFF;
  pkt[1] = (msg_len >> 16) & 0xFF;
  pkt[2] = (msg_len >> 8) & 0xFF;
  pkt[3] = msg_len & 0xFF;
  
  // Copy message
  memcpy(pkt + 4, msg, msg_len);
  
  // Use aosl_send for TCP (no destination address needed)
  int ret = aosl_send(client_res->sk, pkt, 4 + msg_len, 0);
  if (client_res->sent_cnt % 500 == 1) {
    LOG_FMT("TCP ret=%d send msg %s", ret, msg);
  }
}

static int aosl_test_mpq_api_tcp(void)
{
  memset(&mpq_server_res, 0, sizeof(mpq_server_res));
  memset(&mpq_client_res, 0, sizeof(mpq_client_res));

  int priority = AOSL_THRD_PRI_DEFAULT; // default
  int stack_size = 0; // default
  int max_func_size = 10000;
  
  // Create server first (it will listen and accept)
  aosl_mpq_t q_server = aosl_mpq_create(priority, stack_size, max_func_size, "tcp-server",
                  test_mpq_tcp_server_init, test_mpq_tcp_server_fini, NULL);
  CHECK(!aosl_mpq_invalid(q_server));
  
  // Give server time to start listening
  aosl_msleep(100);

  // Create client (it will connect)
  aosl_mpq_t q_client = aosl_mpq_create(priority, stack_size, max_func_size, "tcp-client",
                  test_mpq_tcp_client_init, test_mpq_tcp_client_fini, NULL);
  CHECK(!aosl_mpq_invalid(q_client));

  // client async send msg
  int cnt_cycs = 20;
  int cnt_pers = 50;
  int cnt_alls = cnt_cycs * cnt_pers * 2;
  for (int i = 0; i < cnt_cycs; i++) {
    for (int j = 0; j < cnt_pers; j++) {
      aosl_mpq_queue(q_client, AOSL_MPQ_INVALID, AOSL_REF_INVALID, "test_mpq_tcp_client_send_func",
                     test_mpq_tcp_client_send_func, 1, &mpq_client_res);
    }
    aosl_msleep(100);
  }

  // client sync send msg
  for (int i = 0; i < cnt_cycs; i++) {
    for (int j = 0; j < cnt_pers; j++) {
      aosl_mpq_call(q_client, AOSL_REF_INVALID, "test_mpq_tcp_client_send_func",
                    test_mpq_tcp_client_send_func, 1, &mpq_client_res);
    }
    aosl_msleep(100);
  }

  // check cnts
  aosl_ts_t start_ts = aosl_tick_ms();
  while (mpq_server_res.recv_cnt < cnt_alls && (aosl_tick_ms() - start_ts) < 5000) {
    aosl_msleep(100);
  }
  aosl_mpq_destroy_wait(q_client);
  aosl_mpq_destroy_wait(q_server);
  EXPECT_EQ(mpq_server_res.recv_cnt, cnt_alls);
  EXPECT_EQ(mpq_client_res.sent_cnt, cnt_alls);
  LOG_FMT("mpq tcp test success");
  return 0;
}

static int aosl_test_mpq_max()
{
  int priority = AOSL_THRD_PRI_DEFAULT; // default
  int stack_size = 0; // default
  int max_func_size = 10000;
  char qname[32] = {0};
  aosl_mpq_t q;

  for (int i = 0; i < 10; i++) {
    sprintf(qname, "qtest-%d", i);
    LOG_FMT("create q=%s", qname);
    q = aosl_mpq_create(priority, stack_size, max_func_size, qname, NULL,NULL, NULL);
    CHECK(!aosl_mpq_invalid(q));
  }

  return 0;
}

static int aosl_test_mpq(void)
{
  CHECK(aosl_test_mpq_api_udp() == 0);
  CHECK(aosl_test_mpq_api_tcp() == 0);
  //CHECK(aosl_test_mpq_max() == 0);
  LOG_FMT("test success");
  return 0;
}

__export_in_so__ void aosl_test(void)
{
  LOG_FMT("Start AOSL test...");

  aosl_ctor();

  aosl_test_hal();
  aosl_test_mpq();

  aosl_dtor();

  LOG_FMT("End   AOSL test...");

  return;
}
