#include <errno.h>
#include <winsock2.h>

#include <hal/aosl_hal_errno.h>

int aosl_hal_errno_convert(int errnum)
{
    switch (errnum) {
    case 0:
        return AOSL_HAL_RET_SUCCESS;
    case EAGAIN:
    case EWOULDBLOCK:
    case WSAEWOULDBLOCK:
        return AOSL_HAL_RET_EAGAIN;
    case EINTR:
    case WSAEINTR:
        return AOSL_HAL_RET_EINTR;
    case EINPROGRESS:
    case WSAEINPROGRESS:
    case WSAEALREADY:
        return AOSL_HAL_RET_EINPROGRESS;
    default:
        return AOSL_HAL_RET_EHAL;
    }
}
