#include <windows.h>
#include <bcrypt.h>

#include <stdio.h>
#include <string.h>

#include <hal/aosl_hal_utils.h>

typedef LONG (WINAPI *rtl_get_version_t)(PRTL_OSVERSIONINFOW);

static int fill_random_bytes(void *buf, int len)
{
    return (BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) ? 0 : -1;
}

int aosl_hal_get_uuid(char buf[], int buf_sz)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char bytes[16];
    char full_uuid[33];
    int i;
    int out = 0;
    int copy_len;

    if (!buf || buf_sz <= 1) {
        return -1;
    }

    if (fill_random_bytes(bytes, sizeof(bytes)) != 0) {
        return -1;
    }

    bytes[6] = (unsigned char)((bytes[6] & 0x0F) | 0x40);
    bytes[8] = (unsigned char)((bytes[8] & 0x3F) | 0x80);

    for (i = 0; i < (int)sizeof(bytes); ++i) {
        full_uuid[out++] = hex[(bytes[i] >> 4) & 0x0F];
        full_uuid[out++] = hex[bytes[i] & 0x0F];
    }
    full_uuid[out] = '\0';

    copy_len = buf_sz - 1;
    if (copy_len > 32) {
        copy_len = 32;
    }
    memcpy(buf, full_uuid, (size_t)copy_len);
    buf[copy_len] = '\0';

    return 0;
}

int aosl_hal_os_version(char buf[], int buf_sz)
{
    HMODULE ntdll;
    rtl_get_version_t rtl_get_version;
    RTL_OSVERSIONINFOW version_info;

    if (!buf || buf_sz <= 1) {
        return -1;
    }

    memset(&version_info, 0, sizeof(version_info));
    version_info.dwOSVersionInfoSize = sizeof(version_info);

    ntdll = GetModuleHandleW(L"ntdll.dll");
    rtl_get_version = ntdll ? (rtl_get_version_t)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    if (rtl_get_version && rtl_get_version(&version_info) == 0) {
        _snprintf(buf, (size_t)buf_sz, "Windows %lu.%lu.%lu",
                  version_info.dwMajorVersion,
                  version_info.dwMinorVersion,
                  version_info.dwBuildNumber);
        buf[buf_sz - 1] = '\0';
        return 0;
    }

    _snprintf(buf, (size_t)buf_sz, "Windows");
    buf[buf_sz - 1] = '\0';
    return 0;
}

int aosl_hal_rand_bytes(void *buf, int len)
{
    if (!buf || len <= 0) {
        return -1;
    }

    return fill_random_bytes(buf, len);
}
