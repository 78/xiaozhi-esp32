# AOSL HAL Adaptation — ASR1606 (Quectel EC800MCN_LE)

## Platform Overview

| Item | Detail |
|------|--------|
| SoC | ASR1606 (Marvell CRANE) |
| CPU | ARM Cortex-R4, single core |
| RTOS | ThreadX |
| Module | Quectel EC800MCN_LE (LTE Cat-1) |
| SDK | Quectel QuecOpen C SDK (QuecProductName: EC800MCN_LE) |
| App compiler | arm-none-eabi-gcc 9.2.1 |
| C standard | C99 |
| CPU flags | `-mcpu=cortex-r4 -mfloat-abi=soft -mlittle-endian -mthumb` |
| Float | Software emulation only (no hardware FPU) |
| RAM | ~2.8 MB heap available for user application |

## HAL Configuration

```c
// config/hal/aosl_hal_config.h
#define AOSL_HAL_HAVE_EPOLL   0   // Not available
#define AOSL_HAL_HAVE_POLL    0   // Not available
#define AOSL_HAL_HAVE_SELECT  1   // lwIP select()

#define AOSL_HAL_HAVE_COND    0   // ThreadX has no pthread_cond equivalent
#define AOSL_HAL_HAVE_SEM     1   // Use semaphores instead of cond vars
```

### Rationale

- **SELECT only**: lwIP on ASR1606 provides `select()` but not `epoll` or `poll`.
- **No condition variables**: ThreadX RTOS has no `pthread_cond` equivalent. Semaphores (`ql_rtos_semaphore_*`) are used as the synchronization primitive instead, consistent with the bk7258 platform.

## HAL Implementation Notes

### Thread (`aosl_hal_thread.c`)

- Wraps ThreadX tasks via `ql_rtos_task_create()` / `ql_rtos_task_delete()`.
- Entry function adapter: converts `void *(*)(void *)` (POSIX style) to `void (*)(void *)` (ThreadX style) using a wrapper struct.
- `thread_join`: implemented via priority-based polling with 30s timeout (ThreadX has no native join).
- `thread_detach`: marks thread as detached; resources freed on thread exit.
- Default stack size: 8 KB. Default priority: `QL_DEFAULT_ELOOP_PRIORITY` (100).
- Static mutex (`AOSL_STATIC_MUTEX_SIZE = 128`): dynamically allocates internal mutex on first use.

### Mutex (`aosl_hal_thread.c`)

- `ql_rtos_mutex_create()` / `ql_rtos_mutex_lock()` / `ql_rtos_mutex_unlock()` / `ql_rtos_mutex_delete()`.
- `trylock`: uses `ql_rtos_mutex_try_lock()`.

### Semaphore (`aosl_hal_thread.c`)

- `ql_rtos_semaphore_create()` with initial count = 0.
- `sem_timedwait`: converts milliseconds to ThreadX ticks (1 tick = 5 ms).

### Socket (`aosl_hal_socket.c`)

- lwIP POSIX sockets via `<lwipv4v6/sockets.h>` and `<lwipv4v6/netdb.h>`.
- Address family conversion: `AOSL_AF_INET` <-> `AF_INET`, `AOSL_AF_INET6` <-> `AF_INET6`.
- `set_nonblock`: uses `lwip_fcntl()` with `O_NONBLOCK`.
- `bind_device`: uses `SO_BINDTODEVICE` socket option.
- `gethostbyname`: wraps lwIP `lwip_getaddrinfo()`.
- Note: `sockaddr_in6` on this platform has no `sin6_scope_id` field.

### I/O Multiplexing (`aosl_hal_iomp.c`)

- `fd_set_t` wraps `fd_set` with dynamic allocation (`aosl_hal_fdset_create` / `_destroy`).
- `aosl_hal_select()` wraps lwIP `select()`.

### File (`aosl_hal_file.c`)

- LittleFS via Quectel `ql_fs.h` API (`ql_fopen`, `ql_fclose`, `ql_fread`, `ql_fwrite`).
- `fexist`: uses `ql_access(path, 0)` (2 arguments, not 3).
- `fsize`: uses `ql_fseek(fd, 0, 2)` + `ql_ftell()` (literal `2` for `SEEK_END`, not the macro).
- `rmdir`: uses `ql_remove()` (no dedicated `ql_rmdir` API).
- `file_rename`: uses `ql_rename()`.

### Time (`aosl_hal_time.c`)

- `get_tick_ms`: `ql_rtos_get_systicks() * 5` (1 ThreadX tick = 5 ms).
- `get_time_ms`: same as `get_tick_ms` (no RTC wall-clock on this platform).
- `msleep`: `ql_rtos_task_sleep_ms()`.
- `get_time_str`: formats tick-based timestamp as `"XXXX.XXXs"`.

### Atomic (`aosl_hal_atomic.c`)

- Uses GCC built-in atomics: `__atomic_load_n`, `__atomic_store_n`, `__atomic_fetch_add`, `__atomic_compare_exchange_n`, etc.
- Memory barriers: `__atomic_thread_fence(__ATOMIC_SEQ_CST)`.

### Memory (`aosl_hal_memory.c`)

- Direct wrappers around standard C library `malloc` / `free` / `calloc` / `realloc`.

### Log (`aosl_hal_log.c`)

- 512-byte static buffer.
- `vsnprintf()` + `printf()` for output.

### Utils (`aosl_hal_utils.c`)

- `get_uuid`: generates 32-character random hex string using `rand()`.
- `os_version`: returns `"ThreadX (ASR1606)"` or similar platform identifier.

## Toolchain & Build Instructions

### Toolchain Requirements

| Item | Detail |
|------|--------|
| Cross compiler | `arm-none-eabi-gcc` 9.2.1+ |
| CPU flags | `-mcpu=cortex-r4 -mfloat-abi=soft -mlittle-endian -mthumb -mthumb-interwork` |
| C defines | `__OCPU_COMPILER_GCC__`, `_WANT_USE_LONG_TIME_T`, `_POSIX_THREADS` |
| C standard | `-std=c99` |
| SDK headers | Quectel QuecOpen SDK `common/include/` (provides `ql_rtos.h`, `ql_fs.h`, `ql_log.h` etc.) |
| Network headers | lwIP `common/include/lwipv4v6/` (provides `sockets.h`, `netdb.h`) |

> Note: The toolchain is provided by Quectel SDK at `ql-sdk/ql-cross-tool/win32/owtoolchain/gcc-arm-none-eabi/`. No separate installation required.

### Cross-compile libaosl.a

编写 `toolchain.cmake`（需根据本地 SDK 路径调整）：

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_DIR "<path-to>/ql-sdk/ql-cross-tool/win32/owtoolchain/gcc-arm-none-eabi")
set(CONFIG_TOOLCHAIN_PREFIX "arm-none-eabi-")
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/${CONFIG_TOOLCHAIN_PREFIX}gcc.exe")
set(CMAKE_AR         "${TOOLCHAIN_DIR}/bin/${CONFIG_TOOLCHAIN_PREFIX}ar.exe")

set(QUECTEL_SDK_DIR "<path-to>/ql-sdk/ql-application/threadx")
set(CPU_FLAGS "-mcpu=cortex-r4 -mfloat-abi=soft -mlittle-endian -mthumb -mthumb-interwork")
set(CMAKE_C_FLAGS "${CPU_FLAGS} -D__OCPU_COMPILER_GCC__ -D_WANT_USE_LONG_TIME_T -D_POSIX_THREADS -I${QUECTEL_SDK_DIR}/common/include -I${QUECTEL_SDK_DIR}/common/include/lwipv4v6" CACHE STRING "" FORCE)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

然后编译：

```bash
cd aosl
mkdir build_asr1606 && cd build_asr1606
cmake -G Ninja -DCONFIG_PLATFORM=asr1606 -DCMAKE_TOOLCHAIN_FILE=<your-toolchain>.cmake ..
ninja
```

Output: `build_asr1606/libaosl.a`

> Note: 测试可执行文件的链接会失败（缺少 `-lpthread -ldl -lrt`），这是预期行为，bare-metal 环境仅交付 `libaosl.a` 静态库。

### Integrate into Quectel SDK

1. Copy `libaosl.a` to `ql-sdk/ql-application/threadx/interface/ptalk/lib/`
2. Copy AOSL headers (`api/` and `hal/`) to `ptalk/lib/include/`
3. In module Makefile, set `U_LIBS := $(PTALK_DIR)lib/libaosl.a`
4. Add module to `COMMPILE_DIRS` in `ql-application/threadx/Makefile`
5. Build: `build.bat app` then `build.bat firmware`

## Known Limitations

1. **No hardware FPU** — all float operations are software-emulated.
2. **No condition variables** — ThreadX limitation; semaphores used as replacement.
3. **Time is tick-based** — no RTC wall-clock; `get_time_ms` returns uptime, not epoch time.
4. **File system is LittleFS** — limited POSIX compatibility; no `ql_rmdir()`.
5. **1 tick = 5 ms granularity** — sleep precision limited to 5 ms multiples.
6. **Thread join is simulated** — uses polling with 30s timeout, not a native blocking join.

## Test Results

All tests passed on device. See `test-report.md` for the full test report and `log/Data.txt` for raw serial output.
