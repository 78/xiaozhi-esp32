#include <windows.h>

#include <stdio.h>

#include <hal/aosl_hal_time.h>

#define WINDOWS_TO_UNIX_EPOCH_100NS 116444736000000000ULL

uint64_t aosl_hal_get_tick_ms(void)
{
    return (uint64_t)GetTickCount64();
}

uint64_t aosl_hal_get_time_ms(void)
{
    FILETIME ft;
    ULARGE_INTEGER now;

    GetSystemTimeAsFileTime(&ft);
    now.LowPart = ft.dwLowDateTime;
    now.HighPart = ft.dwHighDateTime;

    return (uint64_t)((now.QuadPart - WINDOWS_TO_UNIX_EPOCH_100NS) / 10000ULL);
}

int aosl_hal_get_time_str(char *buf, int len)
{
    SYSTEMTIME st;

    if (!buf || len <= 0) {
        return -1;
    }

    GetLocalTime(&st);
    _snprintf(buf, (size_t)len, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
              st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    buf[len - 1] = '\0';
    return 0;
}

void aosl_hal_msleep(uint64_t ms)
{
    Sleep((DWORD)ms);
}
