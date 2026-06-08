# AOSL HAL 适配说明 — ASR1606 (Quectel EC800MCN_LE)

## 平台概述

| 项目 | 信息 |
|------|------|
| SoC | ASR1606 (Marvell CRANE) |
| CPU | ARM Cortex-R4, 单核 |
| 操作系统 | ThreadX RTOS |
| 模组 | Quectel EC800MCN_LE (LTE Cat-1) |
| SDK | Quectel QuecOpen C SDK |
| 应用编译器 | arm-none-eabi-gcc 9.2.1 |
| C 标准 | C99 |
| CPU 编译参数 | `-mcpu=cortex-r4 -mfloat-abi=soft -mlittle-endian -mthumb` |
| 浮点 | 仅软件模拟（无硬件 FPU） |
| 可用堆内存 | 约 2.8 MB |

## HAL 配置

```c
// config/hal/aosl_hal_config.h
#define AOSL_HAL_HAVE_EPOLL   0   // 不支持
#define AOSL_HAL_HAVE_POLL    0   // 不支持
#define AOSL_HAL_HAVE_SELECT  1   // lwIP select()

#define AOSL_HAL_HAVE_COND    0   // ThreadX 无 pthread_cond 等价物
#define AOSL_HAL_HAVE_SEM     1   // 使用信号量替代条件变量
```

### 配置说明

- **仅支持 SELECT**: ASR1606 上的 lwIP 提供 `select()` 接口，但不支持 `epoll` 或 `poll`。
- **无条件变量**: ThreadX RTOS 没有 `pthread_cond` 等价接口，使用信号量 (`ql_rtos_semaphore_*`) 作为同步原语替代，与 bk7258 平台保持一致。

## HAL 实现说明

### 线程 (`aosl_hal_thread.c`)

- 通过 `ql_rtos_task_create()` / `ql_rtos_task_delete()` 封装 ThreadX 任务。
- 入口函数适配器: 将 `void *(*)(void *)` (POSIX 风格) 转换为 `void (*)(void *)` (ThreadX 风格)，使用包装结构体实现。
- `thread_join`: 通过优先级轮询模拟实现，超时 30 秒（ThreadX 无原生 join）。
- `thread_detach`: 标记线程为 detached 状态，线程退出时自动释放资源。
- 默认栈大小: 8 KB。默认优先级: `QL_DEFAULT_ELOOP_PRIORITY` (100)。
- 静态互斥锁 (`AOSL_STATIC_MUTEX_SIZE = 128`): 首次使用时动态分配内部互斥锁。

### 互斥锁 (`aosl_hal_thread.c`)

- `ql_rtos_mutex_create()` / `ql_rtos_mutex_lock()` / `ql_rtos_mutex_unlock()` / `ql_rtos_mutex_delete()`。
- `trylock`: 使用 `ql_rtos_mutex_try_lock()`。

### 信号量 (`aosl_hal_thread.c`)

- `ql_rtos_semaphore_create()`，初始计数 = 0。
- `sem_timedwait`: 将毫秒转换为 ThreadX tick (1 tick = 5 ms)。

### 套接字 (`aosl_hal_socket.c`)

- 基于 lwIP POSIX socket 接口，通过 `<lwipv4v6/sockets.h>` 和 `<lwipv4v6/netdb.h>` 调用。
- 地址族转换: `AOSL_AF_INET` <-> `AF_INET`, `AOSL_AF_INET6` <-> `AF_INET6`。
- `set_nonblock`: 使用 `lwip_fcntl()` 设置 `O_NONBLOCK`。
- `bind_device`: 使用 `SO_BINDTODEVICE` 套接字选项。
- `gethostbyname`: 封装 lwIP `lwip_getaddrinfo()`。
- 注意: 该平台的 `sockaddr_in6` 结构体无 `sin6_scope_id` 字段。

### I/O 多路复用 (`aosl_hal_iomp.c`)

- `fd_set_t` 通过动态分配封装 `fd_set` (`aosl_hal_fdset_create` / `_destroy`)。
- `aosl_hal_select()` 封装 lwIP `select()`。

### 文件 (`aosl_hal_file.c`)

- 通过 Quectel `ql_fs.h` API (`ql_fopen`, `ql_fclose`, `ql_fread`, `ql_fwrite`) 访问 LittleFS 文件系统。
- `fexist`: 使用 `ql_access(path, 0)` (2 个参数，不是 3 个)。
- `fsize`: 使用 `ql_fseek(fd, 0, 2)` + `ql_ftell()` (字面量 `2` 表示 `SEEK_END`，不使用宏)。
- `rmdir`: 使用 `ql_remove()` (无专用 `ql_rmdir` API)。
- `file_rename`: 使用 `ql_rename()`。

### 时间 (`aosl_hal_time.c`)

- `get_tick_ms`: `ql_rtos_get_systicks() * 5` (1 ThreadX tick = 5 ms)。
- `get_time_ms`: 与 `get_tick_ms` 相同 (该平台无 RTC 实时时钟)。
- `msleep`: `ql_rtos_task_sleep_ms()`。
- `get_time_str`: 将基于 tick 的时间戳格式化为 `"XXXX.XXXs"`。

### 原子操作 (`aosl_hal_atomic.c`)

- 使用 GCC 内置原子操作: `__atomic_load_n`, `__atomic_store_n`, `__atomic_fetch_add`, `__atomic_compare_exchange_n` 等。
- 内存屏障: `__atomic_thread_fence(__ATOMIC_SEQ_CST)`。

### 内存管理 (`aosl_hal_memory.c`)

- 直接封装标准 C 库 `malloc` / `free` / `calloc` / `realloc`。

### 日志 (`aosl_hal_log.c`)

- 512 字节静态缓冲区。
- `vsnprintf()` + `printf()` 输出。

### 工具函数 (`aosl_hal_utils.c`)

- `get_uuid`: 使用 `rand()` 生成 32 字符随机十六进制字符串。
- `os_version`: 返回 `"ThreadX (ASR1606)"` 平台标识。

## 工具链与编译说明

### 工具链要求

| 项目 | 信息 |
|------|------|
| 交叉编译器 | `arm-none-eabi-gcc` 9.2.1+ (已随交付包提供于 `gcc-arm-none-eabi/` 目录) |
| CPU 编译参数 | `-mcpu=cortex-r4 -mfloat-abi=soft -mlittle-endian -mthumb -mthumb-interwork` |
| C 预定义宏 | `__OCPU_COMPILER_GCC__`, `_WANT_USE_LONG_TIME_T`, `_POSIX_THREADS` |
| C 标准 | `-std=c99` |
| SDK 头文件 | Quectel QuecOpen SDK `common/include/` (提供 `ql_rtos.h`, `ql_fs.h`, `ql_log.h` 等) |
| 网络头文件 | lwIP `common/include/lwipv4v6/` (提供 `sockets.h`, `netdb.h`) |

### 交叉编译 libaosl.a

编写 `toolchain.cmake`（需根据本地 SDK 路径调整 `<path-to>`）：

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_DIR "<path-to>/gcc-arm-none-eabi")
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

输出: `build_asr1606/libaosl.a`

> 注意: 测试可执行文件的链接会失败（缺少 `-lpthread -ldl -lrt`），这是预期行为，bare-metal 环境仅交付 `libaosl.a` 静态库。

### 集成到 Quectel SDK

1. 将 `libaosl.a` 复制到 `ql-sdk/ql-application/threadx/interface/ptalk/lib/`
2. 将 AOSL 头文件 (`api/` 和 `hal/`) 复制到 `ptalk/lib/include/`
3. 在模块 Makefile 中设置 `U_LIBS := $(PTALK_DIR)lib/libaosl.a`
4. 将模块目录添加到 `ql-application/threadx/Makefile` 的 `COMMPILE_DIRS`
5. 编译: `build.bat app`，然后 `build.bat firmware`

## 已知限制

1. **无硬件 FPU** — 所有浮点运算均为软件模拟。
2. **无条件变量** — ThreadX 限制，使用信号量替代。
3. **时间基于 tick** — 无 RTC 实时时钟，`get_time_ms` 返回开机时长而非纪元时间。
4. **LittleFS 文件系统** — POSIX 兼容性有限，无 `ql_rmdir()`。
5. **5 ms tick 精度** — 睡眠精度受限于 5 ms 的倍数。
6. **线程 join 为模拟实现** — 通过轮询实现，超时 30 秒，非原生阻塞 join。

## 测试结果

全部测试已在设备上通过。详见 `test-report-zh.md` 完整测试报告和 `log/Data.txt` 原始串口日志。
