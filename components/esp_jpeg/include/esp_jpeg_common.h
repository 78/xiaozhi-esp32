// Copyright 2022 Espressif Systems (Shanghai) CO., LTD
// All rights reserved.

#pragma once

#include "stdint.h"

/* Error code */
typedef enum {
    JPEG_ERR_OK = 0,            /* Succeeded */
    JPEG_ERR_FAIL = -1,         /* Device error or wrong termination of input stream */
    JPEG_ERR_MEM = -2,          /* Insufficient memory pool for the image */
    JPEG_ERR_NO_MORE_DATA = -3, /* Input data is not enough */
    JPEG_ERR_PAR = -4,          /* Parameter error */
    JPEG_ERR_FMT1 = -5,         /* Data format error (may be damaged data) */
    JPEG_ERR_FMT2 = -6,         /* Right format but not supported */
    JPEG_ERR_FMT3 = -7          /* Not supported JPEG standard */
} jpeg_error_t;

/* Jpeg raw data type.
 * raw data ---> encoder  --->  encoded data ---> transmit  ---> decoder ---> raw data
 */
typedef enum {
    JPEG_RAW_TYPE_GRAY = 0,       /*!< Grayscale. encoder supported. decoder un-supported.*/
    JPEG_RAW_TYPE_RGB888 = 1,     /*!< RGB888. encoder supported.  decoder supported. */
    JPEG_RAW_TYPE_RGBA = 2,       /*!< RGBA. encoder supported.  decoder un-supported.*/
    JPEG_RAW_TYPE_YCbYCr = 3,     /*!< Y422. encoder supported.  decoder un-supported.*/
    JPEG_RAW_TYPE_YCbY2YCrY2 = 4, /*!< Y420. encoder supported.  decoder un-supported.*/
    JPEG_RAW_TYPE_RGB565_BE = 5,  /*!< RGB565. The data is big end. encoder un-supported.  decoder supported. */
    JPEG_RAW_TYPE_RGB565_LE = 6,  /*!< RGB565. The data is little end. encoder un-supported.  decoder supported.*/
} jpeg_raw_type_t;

/* JPEG chroma subsampling factors */
typedef enum {
    JPEG_SUB_SAMPLE_Y = 0,      /*!< Grayscale */
    JPEG_SUB_SAMPLE_YUV444 = 1, /*!< data order:YUV 1x1x1   3 pixel per MCU  Y444*/
    JPEG_SUB_SAMPLE_YUV422 = 2, /*!< data order:YUYV 2x1x1  2 pixel per MCU  Y422*/
    JPEG_SUB_SAMPLE_YUV420 = 3  /*!< data order:YUY2YVY2 4x1x1  1.25 pixel per MCU  Y420)*/
} jpeg_subsampling_t;

typedef enum {
    JPEG_ROTATE_0D = 0,   /*!< Source image rotates clockwise 0 degree before encoding */
    JPEG_ROTATE_90D = 1,  /*!< Source image rotates clockwise 90 degree before encoding */
    JPEG_ROTATE_180D = 2, /*!< Source image rotates clockwise 180 degree before encoding  */
    JPEG_ROTATE_270D = 3, /*!< Source image rotates clockwise 270 degree before encoding  */
} jpeg_rotate_t;

/**
 * @brief      Convert YUV to RGB
 *
 * @param[in]      sub_sample    chroma subsampling factors. Grayscale is un-supported. The others are supported.
 * @param[in]      raw_type      raw data type. RGB888 and RGB565 are supported. The others are un-supported.
 * @param[in]      yuv_image     input YUV image data. The buffer must be aligned 16 byte.
 * @param[in]      width         image width
 * @param[in]      height        image height
 * @param[out]     rgb_image     output RGB image data
 *
 * @return     jpeg_error_t
 *             - JPEG_ERR_OK: on success
 *             - Others: error occurs
 */
jpeg_error_t jpeg_yuv2rgb(jpeg_subsampling_t sub_sample, jpeg_raw_type_t raw_type, uint8_t *yuv_image, int width, int height, uint8_t *rgb_image);

/**
 * @brief      Allocated buffer. And the buffer address will be aligned.
 *
 * @param[in]      size    Allocate buffer size.
 * @param[in]      aligned Aligned byte
 *
 * @return     positive:   Allocate buffer address
 *             NULL: failed
 */
void *jpeg_malloc_align(int size, int aligned);

/**
 * @brief      Free buffer. The buffer address come from `jpeg_malloc_align`
 *
 * @param[in]      data    The buffer address.
 *
 */
void jpeg_free_align(void *data);

/**
 * Example usage:
 * @code{c}
 *
 * void* yu2_rgb_demo()
 * {
 *  // allocate memory
 *  int width = 320;
 *  int height = 240;
 *  int yuv_pixel = 3; //JPEG_SUB_SAMPLE_YUV444:3 JPEG_SUB_SAMPLE_YUV422:2 JPEG_SUB_SAMPLE_YUV420:1.5
 *  int yuv_image_size = width * height * yuv_pixel;
 *  uint8_t* yuv_image = (uint8_t*)calloc(1, yuv_image_size);
 *  if(yuv_image == NULL) {
 *      return;
 *  }
 *  int rgb_pixel = 3; // JPEG_RAW_TYPE_RGB888:3  JPEG_RAW_TYPE_RGB565_LE/JPEG_RAW_TYPE_RGB565_BE:2
 *  int rgb_image_size = width * height * rgb_pixel;
 *  uint8_t* rgb_image = (uint8_t*)jpeg_malloc_align(rgb_image_size, 16);
 *  if(rgb_image == NULL) {
 *      return;
 *  }
 *  // read yuv data
 *  memset(yuv_image, 128, yuv_image_size)
 *  for (int i= 0; i< yuv_image_size; i+=3) {
 *      yuv_image[i] = i / 3;
 *  }
 *  // convert
 *  jpeg_error_t ret = jpeg_yuv2rgb(JPEG_SUB_SAMPLE_YUV444, JPEG_RAW_TYPE_RGB888, yuv_image, width, height, rgb_image);
 *  // print rgb data
 *  for (int i= 0; i< rgb_image_size; i++) {
 *      printf("%x ", rgb_image[i]&0xff);
 *  }
 * jpeg_free_align(rgb_image);
 * free(yuv_image);
 * }
 *
 * @endcode
 */