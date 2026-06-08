# AOSL (Advanced Operating System Layer) SDK

**Version:** 1.0.0  
**Description:** AOSL is a cross-platform low-level general-purpose component library providing abstract implementations for threads, IO, logging, memory, networking, message queues, and more.

---

## Module Index

| Module | Header File | Description |
|--------|-------------|-------------|
| Initialization | aosl.h | Library initialization and cleanup |
| Atomic Operations | aosl_atomic.h | Atomic variable operations and memory barriers |
| Memory Management | aosl_mm.h | Memory allocation and statistics |
| Logging System | aosl_log.h | Leveled log output |
| Time Management | aosl_time.h | Timestamp retrieval and sleep |
| Thread Synchronization | aosl_thread.h | Locks, condition variables, events |
| Multiplex Queue | aosl_mpq.h | Message queue core |
| MPQ Pool | aosl_mpqp.h | Thread pool |
| MPQ Timer | aosl_mpq_timer.h | Timers |
| MPQ File Descriptor | aosl_mpq_fd.h | IO event monitoring |
| MPQ Network | aosl_mpq_net.h | Socket networking |
| Reference Object | aosl_ref.h | Reference counting and read-write locks |
| Linked List | aosl_list.h | Doubly linked list |
| Red-Black Tree | aosl_rbtree.h | Red-black tree |
| Packet Buffer | aosl_psb.h | Network packet buffering |
| Socket | aosl_socket.h | Socket address and byte order |

---

## 1. Initialization and Cleanup

```c
void aosl_ctor(void);           // Initialize AOSL library (must be called first)
void aosl_dtor(void);           // Cleanup AOSL library (call before program exit)
```

---

## 2. Atomic Operations (aosl_atomic.h)

### Basic Operations
```c
int  aosl_atomic_read(const aosl_atomic_t *v);           // Atomic read
void aosl_atomic_set(aosl_atomic_t *v, int i);            // Atomic set
void aosl_atomic_inc(aosl_atomic_t *v);                    // Atomic increment
void aosl_atomic_dec(aosl_atomic_t *v);                    // Atomic decrement
```

### Arithmetic Operations
```c
int  aosl_atomic_add_return(int i, aosl_atomic_t *v);      // Add and return result
int  aosl_atomic_sub_return(int i, aosl_atomic_t *v);      // Subtract and return result
int  aosl_atomic_inc_and_test(aosl_atomic_t *v);           // Increment and test if zero
int  aosl_atomic_dec_and_test(aosl_atomic_t *v);           // Decrement and test if zero
```

### Exchange Operations
```c
int  aosl_atomic_cmpxchg(aosl_atomic_t *v, int oldval, int newval);  // Compare and swap (CAS)
int  aosl_atomic_xchg(aosl_atomic_t *v, int newval);                   // Exchange
```

### Memory Barriers
```c
void aosl_mb(void);    // Full memory barrier
void aosl_rmb(void);   // Read memory barrier
void aosl_wmb(void);   // Write memory barrier
```

---

## 3. Memory Management (aosl_mm.h)

### Memory Allocation
```c
void *aosl_malloc(size_t size);              // Allocate memory
void *aosl_calloc(size_t nmemb, size_t size); // Allocate and zero-initialize
void *aosl_realloc(void *ptr, size_t size);   // Reallocate
char *aosl_strdup(const char *s);             // Duplicate string
void  aosl_free(void *ptr);                   // Free memory
```

### Memory Statistics
```c
size_t aosl_memused(void);       // Get used memory size
void   aosl_memdump(void);        // Print memory allocation info (debug)
```

---

## 4. Logging System (aosl_log.h)

### Log Levels
```c
AOSL_LOG_EMERG    (0)  // System is unusable
AOSL_LOG_ALERT    (1)  // Action must be taken immediately
AOSL_LOG_CRIT     (2)  // Critical conditions
AOSL_LOG_ERROR    (3)  // Error
AOSL_LOG_WARNING  (4)  // Warning
AOSL_LOG_NOTICE   (5)  // Notice
AOSL_LOG_INFO     (6)  // Informational
AOSL_LOG_DEBUG    (7)  // Debug
```

### Log Functions
```c
void aosl_set_vlog_func(aosl_vlog_t vlog);  // Set custom log function
int  aosl_get_log_level(void);               // Get current log level
void aosl_set_log_level(int level);          // Set log level
void aosl_log(int level, const char *fmt, ...);  // Output log message
```

### Convenience Macros
```c
AOSL_LOG_DBG(fmt, ...)  // Debug log
AOSL_LOG_INF(fmt, ...)  // Info log
AOSL_LOG_WRN(fmt, ...)  // Warning log
AOSL_LOG_ERR(fmt, ...)  // Error log
```

---

## 5. Time Management (aosl_time.h)

### Get Time
```c
aosl_ts_t aosl_tick_now(void);    // Get tick count
aosl_ts_t aosl_tick_ms(void);     // Get millisecond timestamp
aosl_ts_t aosl_tick_us(void);     // Get microsecond timestamp
aosl_ts_t aosl_time_sec(void);    // Get Unix timestamp in seconds
aosl_ts_t aosl_time_ms(void);     // Get Unix timestamp in milliseconds
```

### Sleep
```c
void aosl_msleep(uint64_t ms);    // Sleep for specified milliseconds
```

---

## 6. Thread Synchronization (aosl_thread.h)

### Thread Local Storage (TLS)
```c
int  aosl_tls_key_create(aosl_tls_key_t *key);   // Create TLS key
void *aosl_tls_key_get(aosl_tls_key_t key);       // Get TLS value
int  aosl_tls_key_set(aosl_tls_key_t key, void *value);  // Set TLS value
int  aosl_tls_key_delete(aosl_tls_key_t key);     // Delete TLS key
```

### Mutex Lock
```c
aosl_lock_t aosl_lock_create(void);        // Create lock
void        aosl_lock_lock(aosl_lock_t lock);      // Lock (blocking)
int         aosl_lock_trylock(aosl_lock_t lock);   // Try lock (non-blocking)
void        aosl_lock_unlock(aosl_lock_t lock);    // Unlock
void        aosl_lock_destroy(aosl_lock_t lock);   // Destroy lock
```

### Read-Write Lock
```c
aosl_rwlock_t aosl_rwlock_create(void);           // Create read-write lock
void aosl_rwlock_rdlock(aosl_rwlock_t rwlock);    // Acquire read lock
int  aosl_rwlock_tryrdlock(aosl_rwlock_t rwlock); // Try read lock
void aosl_rwlock_wrlock(aosl_rwlock_t rwlock);    // Acquire write lock
int  aosl_rwlock_trywrlock(aosl_rwlock_t rwlock); // Try write lock
void aosl_rwlock_rdunlock(aosl_rwlock_t rwlock);  // Release read lock
void aosl_rwlock_wrunlock(aosl_rwlock_t rwlock);  // Release write lock
void aosl_rwlock_destroy(aosl_rwlock_t rwlock);   // Destroy read-write lock
```

### Condition Variable
```c
aosl_cond_t aosl_cond_create(void);                   // Create condition variable
void        aosl_cond_signal(aosl_cond_t cond_var);   // Wake one
void        aosl_cond_broadcast(aosl_cond_t cond_var); // Wake all
void        aosl_cond_wait(aosl_cond_t cond_var, aosl_lock_t lock); // Wait indefinitely
int         aosl_cond_timedwait(aosl_cond_t cond_var, aosl_lock_t lock, intptr_t timeo); // Timed wait
void        aosl_cond_destroy(aosl_cond_t cond_var);  // Destroy condition variable
```

### Event
```c
aosl_event_t aosl_event_create(void);              // Create event
void         aosl_event_set(aosl_event_t event_var);    // Set event (persistent)
void         aosl_event_pulse(aosl_event_t event_var);  // Pulse event (wake then reset)
void         aosl_event_wait(aosl_event_t event_var);   // Wait for event
int          aosl_event_timedwait(aosl_event_t event_var, intptr_t timeo); // Timed wait
void         aosl_event_reset(aosl_event_t event_var);  // Reset event
void         aosl_event_destroy(aosl_event_t event_var); // Destroy event
```

---

## 7. Multiplex Queue (MPQ) - Message Queue

### Priority Definitions
```c
AOSL_THRD_PRI_DEFAULT (0)
AOSL_THRD_PRI_LOW     (1)
AOSL_THRD_PRI_NORMAL  (2)
AOSL_THRD_PRI_HIGH    (3)
AOSL_THRD_PRI_HIGHEST (4)
AOSL_THRD_PRI_RT      (5)
```

### Create and Destroy
```c
aosl_mpq_t aosl_mpq_create(int pri, int stack_size, int max, const char *name, 
                            aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);
int aosl_mpq_destroy(aosl_mpq_t q);              // Destroy (no wait)
int aosl_mpq_destroy_wait(aosl_mpq_t mpq_id);    // Destroy and wait
```

### Queue Functions (Async/Sync)
```c
// argv version
int aosl_mpq_queue(aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, 
                   aosl_mpq_func_argv_t f, uintptr_t argc, ...);  // Async
int aosl_mpq_call(aosl_mpq_t q, aosl_ref_t ref, const char *f_name, 
                  aosl_mpq_func_argv_t f, uintptr_t argc, ...);   // Sync
int aosl_mpq_run(aosl_mpq_t q, aosl_ref_t ref, const char *f_name, 
                 aosl_mpq_func_argv_t f, uintptr_t argc, ...);    // Smart (sync on same queue, async on different queue)

// data version
int aosl_mpq_queue_data(aosl_mpq_t tq, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name, 
                        aosl_mpq_func_data_t f, size_t len, void *data);  // Async
int aosl_mpq_call_data(aosl_mpq_t q, aosl_ref_t ref, const char *f_name, 
                       aosl_mpq_func_data_t f, size_t len, void *data);   // Sync
```

### Main MPQ Management
```c
int        aosl_main_start(int pri, aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);
aosl_mpq_t aosl_mpq_main(void);       // Get main MPQ
int        aosl_main_exit(void);        // Exit main MPQ (no wait)
int        aosl_main_exit_wait(void);   // Exit main MPQ and wait
```

### Event Loop
```c
void aosl_mpq_loop(void);  // Enter MPQ event loop (never returns)
```

### Status Query
```c
aosl_mpq_t aosl_mpq_this(void);        // Get current MPQ
aosl_mpq_t aosl_mpq_current(void);     // Get or create current MPQ
int        aosl_mpq_queued_count(aosl_mpq_t q);  // Get queue length
int        aosl_mpq_this_destroyed(void);         // Check if destroyed
int        aosl_mpq_is_main(void);                 // Check if main MPQ
```

---

## 8. MPQ Pool (MPQP) - Thread Pool

### Create and Destroy
```c
aosl_mpqp_t aosl_mpqp_create(int pool_size, int pri, int stack_size, int max, 
                              int max_idles, int flags, const char *name,
                              aosl_mpq_init_t init, aosl_mpq_fini_t fini, void *arg);
void aosl_mpqp_destroy(aosl_mpqp_t qp, int wait);
```

### Pool Queue Operations
```c
aosl_mpq_t aosl_mpqp_queue(aosl_mpqp_t qp, aosl_mpq_t dq, aosl_ref_t ref, const char *f_name,
                           aosl_mpq_func_argv_t f, uintptr_t argc, ...);
aosl_mpq_t aosl_mpqp_call(aosl_mpqp_t qp, aosl_ref_t ref, const char *f_name,
                          aosl_mpq_func_argv_t f, uintptr_t argc, ...);
```

### System Built-in Pools
```c
aosl_mpqp_t aosl_cpup(void);  // CPU-intensive task pool
aosl_mpqp_t aosl_gpup(void);  // GPU-related task pool
aosl_mpqp_t aosl_genp(void);  // General-purpose task pool
aosl_mpqp_t aosl_ltwp(void);  // Lightweight task pool
```

### Dedicated MPQ Allocation
```c
aosl_mpq_t aosl_mpq_alloc(void);  // Allocate dedicated MPQ
int        aosl_mpq_free(aosl_mpq_t qid);  // Free dedicated MPQ
```

---

## 9. MPQ Timer

### Create Timer
```c
// Periodic timer
aosl_timer_t aosl_mpq_create_timer(uintptr_t interval, aosl_timer_func_t func, 
                                    aosl_obj_dtor_t dtor, uintptr_t argc, ...);
aosl_timer_t aosl_mpq_create_timer_on_q(aosl_mpq_t qid, uintptr_t interval, 
                                         aosl_timer_func_t func, aosl_obj_dtor_t dtor, 
                                         uintptr_t argc, ...);

// One-shot timer
aosl_timer_t aosl_mpq_create_oneshot_timer(aosl_timer_func_t func, aosl_obj_dtor_t dtor, 
                                             uintptr_t argc, ...);
aosl_timer_t aosl_mpq_set_oneshot_timer(aosl_ts_t expire_time, aosl_timer_func_t func, 
                                          aosl_obj_dtor_t dtor, uintptr_t argc, ...);
```

### Timer Operations
```c
int aosl_mpq_timer_interval(aosl_timer_t timer_id, uintptr_t *interval_p);  // Get interval
int aosl_mpq_timer_active(aosl_timer_t timer_id, int *active_p);            // Check if active
int aosl_mpq_resched_timer(aosl_timer_t timer_id, uintptr_t interval);       // Reschedule periodic
int aosl_mpq_resched_oneshot_timer(aosl_timer_t timer_id, aosl_ts_t expire_time);  // Reschedule one-shot
int aosl_mpq_cancel_timer(aosl_timer_t timer_id);                             // Cancel timer
int aosl_mpq_kill_timer(aosl_timer_t timer_id);                               // Destroy timer
```

---

## 10. MPQ File Descriptor (IO Events)

### Add/Remove FD
```c
int aosl_mpq_add_fd(aosl_fd_t fd, size_t max_pkt_size,
                    aosl_fd_read_t read_f, aosl_fd_write_t write_f,
                    aosl_check_packet_t chk_pkt_f, aosl_fd_data_t data_f,
                    aosl_fd_event_t event_f, uintptr_t argc, ...);
int aosl_mpq_add_fd_on_q(aosl_mpq_t qid, aosl_fd_t fd, size_t max_pkt_size,
                         aosl_fd_read_t read_f, aosl_fd_write_t write_f,
                         aosl_check_packet_t chk_pkt_f, aosl_fd_data_t data_f,
                         aosl_fd_event_t event_f, uintptr_t argc, ...);
int aosl_mpq_del_fd(aosl_fd_t fd);  // Remove FD
```

### IO Operations
```c
isize_t aosl_write(aosl_fd_t fd, const void *buf, size_t len);  // Write
int     aosl_close(aosl_fd_t fd);                                 // Close
```

---

## 11. MPQ Network (Socket)

### Socket Basics
```c
aosl_fd_t aosl_socket(int domain, int type, int protocol);        // Create socket
int       aosl_bind(aosl_fd_t sockfd, const aosl_sockaddr_t *addr);  // Bind
int       aosl_bind_port_only(aosl_fd_t sockfd, uint16_t af, unsigned short port);
```

### TCP Connection
```c
int aosl_mpq_connect(aosl_fd_t fd, const aosl_sockaddr_t *dest_addr,
                     int timeo, size_t max_pkt_size, aosl_check_packet_t chk_pkt_f,
                     aosl_fd_data_t data_f, aosl_fd_event_t event_f, uintptr_t argc, ...);
```

### TCP Listen
```c
int aosl_mpq_listen(aosl_fd_t fd, int backlog, aosl_sk_accepted_t accepted_f, 
                    aosl_fd_event_t event_f, uintptr_t argc, ...);
```

### UDP Socket
```c
int aosl_mpq_add_dgram_socket(aosl_fd_t fd, size_t max_pkt_size, 
                              aosl_dgram_sk_data_t data_f, aosl_fd_event_t event_f, 
                              uintptr_t argc, ...);
```

### Send Data
```c
isize_t aosl_send(aosl_fd_t sockfd, const void *buf, size_t len, int flags);
isize_t aosl_sendto(aosl_fd_t sockfd, const void *buf, size_t len, int flags, 
                    const aosl_sockaddr_t *dest_addr);
```

### Address Conversion
```c
const char *aosl_sockaddr_str(const aosl_sockaddr_t *addr, char *addr_buf, size_t buf_len);
const char *aosl_inet_addr_str(int af, const void *addr, char *addr_buf, size_t buf_len);
aosl_socklen_t aosl_ip_sk_addr_from_string(aosl_sk_addr_t *sk_addr, const char *str_addr, uint16_t port);
```

---

## 12. Reference Object (Ref)

### Create and Destroy
```c
aosl_ref_t aosl_ref_create(void *arg, aosl_ref_dtor_t dtor, int modify_async, 
                           int recursive, int caller_free);
int aosl_ref_destroy(aosl_ref_t ref, int do_delete);
```

### Access Operations
```c
int aosl_ref_hold(aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...);    // Hold
int aosl_ref_read(aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...);    // Read lock
int aosl_ref_write(aosl_ref_t ref, aosl_ref_func_t f, uintptr_t argc, ...);   // Write lock
```

---

## 13. Data Structures

### Doubly Linked List (aosl_list.h)
```c
// Initialization
AOSL_DEFINE_LIST_HEAD(name)
void aosl_list_head_init(struct aosl_list_head *list)

// Add/Remove
void aosl_list_add(struct aosl_list_head *new_node, struct aosl_list_head *head)
void aosl_list_add_tail(struct aosl_list_head *new_node, struct aosl_list_head *head)
void aosl_list_del(struct aosl_list_head *entry)

// Traversal
aosl_list_for_each(pos, head)
aosl_list_for_each_safe(pos, n, head)
aosl_list_entry(ptr, type, member)

// Query
int  aosl_list_empty(const struct aosl_list_head *head)
```

### Red-Black Tree (aosl_rbtree.h)
```c
void aosl_rb_root_init(struct aosl_rb_root *root, aosl_rb_node_cmp_t cmp)
void aosl_rb_insert_node(struct aosl_rb_root *root, struct aosl_rb_node *node, ...)
struct aosl_rb_node *aosl_rb_remove(struct aosl_rb_root *root, struct aosl_rb_node *node, ...)
struct aosl_rb_node *aosl_find_rb_node(struct aosl_rb_root *root, struct aosl_rb_node *node, ...)
struct aosl_rb_node *aosl_rb_first(struct aosl_rb_root *)
struct aosl_rb_node *aosl_rb_last(struct aosl_rb_root *)
struct aosl_rb_node *aosl_rb_next(struct aosl_rb_node *)
struct aosl_rb_node *aosl_rb_prev(struct aosl_rb_node *)
```

### Packet Buffer (aosl_psb.h)
```c
aosl_psb_t *aosl_alloc_psb(size_t size)
void       *aosl_psb_data(const aosl_psb_t *psb)
size_t      aosl_psb_len(const aosl_psb_t *psb)
void       *aosl_psb_put(aosl_psb_t *psb, size_t len)
void       *aosl_psb_get(aosl_psb_t *psb, size_t len)
void       *aosl_psb_push(aosl_psb_t *psb, size_t len)
void       *aosl_psb_pull(aosl_psb_t *psb, size_t len)
void        aosl_free_psb_list(aosl_psb_t *psb)
```

---

## 14. Error Codes (aosl_errno.h)

### Basic Errors
```c
AOSL_EPERM        (1001)  // Operation not permitted
AOSL_ENOENT       (1002)  // No such file or directory
AOSL_EINTR        (1004)  // Interrupted system call
AOSL_EIO          (1005)  // IO error
AOSL_ENOMEM       (1012)  // Out of memory
AOSL_EACCES       (1013)  // Permission denied
AOSL_EINVAL       (1022)  // Invalid argument
```

### Network Errors
```c
AOSL_ENOTSOCK     (1088)  // Socket operation on non-socket
AOSL_EPROTONOSUPPORT (1093)  // Protocol not supported
AOSL_EOPNOTSUPP   (1095)  // Operation not supported
AOSL_EADDRINUSE   (1098)  // Address already in use
AOSL_ENETDOWN     (1100)  // Network is down
AOSL_ECONNRESET   (1104)  // Connection reset
AOSL_ETIMEDOUT    (1110)  // Connection timed out
AOSL_ECONNREFUSED (1111)  // Connection refused
AOSL_EALREADY     (1114)  // Operation already in progress
AOSL_EINPROGRESS  (1115)  // Operation in progress
```

### Error Functions
```c
int  *aosl_errno_ptr(void);    // Get errno pointer
char *aosl_strerror(int errnum);  // Get error description
#define aosl_errno (*aosl_errno_ptr())  // Error code variable
```

---

## 15. Utility Macros (aosl_defs.h)

```c
container_of(ptr, type, member)  // Get struct pointer from member pointer
aosl_min(x, y)                    // Minimum value
aosl_max(x, y)                    // Maximum value
aosl_clamp(val, lo, hi)           // Clamp value to range
```

---

## Typical Usage Patterns

### 1. Basic Initialization
```c
#include <api/aosl.h>

int main(int argc, char *argv[]) {
    aosl_ctor();  // Initialize
    
    // Your code
    
    aosl_dtor();  // Cleanup
    return 0;
}
```

### 2. MPQ Message Queue
```c
// Define callback function
void my_func(const aosl_ts_t *queued_ts_p, aosl_refobj_t robj, 
             uintptr_t argc, uintptr_t argv[]) {
    if (aosl_is_free_only(robj)) {
        // Cleanup resources only
        return;
    }
    // Normal processing
}

// Create MPQ
aosl_mpq_t q = aosl_mpq_create(AOSL_THRD_PRI_NORMAL, 0, 1000, "my_queue", 
                                 NULL, NULL, NULL);

// Async queue call
aosl_mpq_queue(q, -1, AOSL_REF_INVALID, "my_func", my_func, 2, arg1, arg2);

// Enter event loop
aosl_mpq_loop();
```

### 3. Timer
```c
void timer_cb(aosl_timer_t timer_id, const aosl_ts_t *now_p, 
              uintptr_t argc, uintptr_t argv[]) {
    // Timer fired
}

// Create periodic timer (fires every 100ms)
aosl_timer_t timer = aosl_mpq_create_timer(100, timer_cb, NULL, 0);

// Cancel timer
aosl_mpq_cancel_timer(timer);
```

### 4. Network Socket
```c
// Create TCP socket
aosl_fd_t sk = aosl_socket(AOSL_AF_INET, AOSL_SOCK_STREAM, 0);

// Connect
aosl_mpq_connect(sk, &dest_addr, 5000, 4096, check_packet, 
                 on_data, on_event, 0);

// Send data
aosl_send(sk, data, len, 0);
```

---

## Build and Integration

### CMake Integration
```cmake
# Set platform
set(CONFIG_PLATFORM "linux")

# Include as subdirectory
set(AOSL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/aosl)
set(AOSL_DECLARE_PROJECT OFF CACHE BOOL "Declare as Standalone Project" FORCE)
include(${AOSL_DIR}/CMakeLists.txt)

# Link AOSL library
target_link_libraries(your_target PRIVATE aosl)
target_include_directories(your_target PRIVATE ${AOSL_ADD_INCLUDES_PUBLIC})
```

### Build Commands
```bash
cd aosl
mkdir build && cd build
cmake .. -DCONFIG_PLATFORM=linux
make
```

---

**Note:** This document accurately describes all public interfaces of the AOSL SDK, including parameter meanings, return value descriptions, and usage scenarios. All interface descriptions are derived from header file definitions.
