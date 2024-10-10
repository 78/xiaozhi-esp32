#ifndef __MJPEG_H
#define __MJPEG_H 

#include "stdio.h"
#include "stdint.h"
#include <cdjpeg.h> 
// #include <sys.h> 
#include <setjmp.h>

#ifdef __cplusplus 
extern "C" {
#endif


void mjpegdraw(uint8_t *mjpegbuffer, uint32_t size, uint8_t *outbuffer);

#ifdef __cplusplus 
}
#endif

#endif

