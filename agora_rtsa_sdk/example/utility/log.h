#ifndef __AGORA_UTILITY_LOG_H__
#define __AGORA_UTILITY_LOG_H__
#if 0
#define LOGS(fmt, ...) fprintf(stdout, "" fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...) fprintf(stdout, "[DBG] " fmt "\n", ##__VA_ARGS__)
#define LOGI(fmt, ...) fprintf(stdout, "[INF] " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) fprintf(stdout, "[WRN] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stdout, "[ERR] " fmt "\n", ##__VA_ARGS__)

#else
#include <time.h>
#include <string.h>
#define AG_LOG_COMM(level, fmt, ...)                                           \
  do {                                                                         \
    char __time_str[20] = { 0 };                                               \
    struct timespec __tsp = { 0 };                                             \
    time_t __now_sec;                                                          \
    time_t __now_ms;                                                           \
    clock_gettime(CLOCK_REALTIME, &__tsp);                                     \
    __now_ms = (time_t)(__tsp.tv_nsec / 1000000);                              \
    __now_sec = __tsp.tv_sec;                                                  \
    strftime(__time_str, sizeof(__time_str), "%F %T", localtime(&__now_sec));  \
    fprintf(stdout, "[%s.%03d]%s " fmt "\n", __time_str, (int)__now_ms, level, ##__VA_ARGS__); \
  } while (0)
#define LOGS(fmt, ...) fprintf(stdout, "" fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...) AG_LOG_COMM("[DBG]", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) AG_LOG_COMM("[INF]", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) AG_LOG_COMM("[WRN]", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) AG_LOG_COMM("[ERR]", fmt, ##__VA_ARGS__)

#endif
#endif //__AGORA_UTILITY_LOG_H__