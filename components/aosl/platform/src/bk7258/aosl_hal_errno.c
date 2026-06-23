#include <lwip/errno.h>
#include <hal/aosl_hal_errno.h>

int aosl_hal_errno_convert(int errnum)
{
	switch (errnum) {
		case 0:
			return AOSL_HAL_RET_SUCCESS;
		case EAGAIN:
			return AOSL_HAL_RET_EAGAIN;
		case EINTR:
			return AOSL_HAL_RET_EINTR;
		case EINPROGRESS:
			return AOSL_HAL_RET_EINPROGRESS;
		default:
			return AOSL_HAL_RET_EHAL;
	}
}