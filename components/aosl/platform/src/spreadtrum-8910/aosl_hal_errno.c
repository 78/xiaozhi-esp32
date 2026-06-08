#include <DAPS/export/inc/tcpip6/socket_types.h>

#include <hal/aosl_hal_errno.h>

int aosl_hal_errno_convert(int errnum)
{
  if (0 == errnum) {
    return AOSL_HAL_RET_SUCCESS;
  }

  if (EWOULDBLOCK == errnum) {
    return AOSL_HAL_RET_EAGAIN;
  }

  return AOSL_HAL_RET_FAILURE;
}