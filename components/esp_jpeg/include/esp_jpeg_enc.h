// Copyright 2022 Espressif Systems (Shanghai) CO., LTD
// All rights reserved.

#pragma once

/**
 * @file esp_jpeg_enc.h
 * @brief Create an JPEG encoder.
 *        Currently, support functions as follows:
 *             - Support variety of width and height to encoder
 *             - Support RGB888 RGBA YCbYCr YCbY2YCrY2 GRAY raw data to encode
 *             - Support sub-sample(YUV444 YUV422 Yuv420)
 *             - Support quality(1-100)
 *             - Support 0, 90 180 270 degree clockwise rotation, under src_type = JPEG_RAW_TYPE_YCbYCr,
 *               subsampling = JPEG_SUB_SAMPLE_YUV420, width and height are multiply of 16  and
 *               src_type = JPEG_RAW_TYPE_YCbYCr, subsampling = JPEG_SUB_SAMPLE_Y, width and height are multiply of 8.
 *             - Support dual-task
 *             - Support two mode encoder, respectively block encoder and one image encoder
 *        The encoder does ASM optimization in ESP32S3. The encoder frame rate performs better than the others chips.
 *        Under src_type = JPEG_RAW_TYPE_YCbYCr, subsampling = JPEG_SUB_SAMPLE_YUV420, width % 16 == 0. height % 16 = 0 conditions,
 *        memory consumption is about 9K.Others, with wider the image and larger `subsampling`, the greater memory consumption will be.
 *
 * @version 1.0.0
 *
 */

#include "stdbool.h"
#include "stdint.h"
#include "esp_jpeg_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_JPEG_ENC_CONFIG()              \
    {                                          \
        .width = 320,                          \
        .height = 240,                         \
        .src_type = JPEG_RAW_TYPE_YCbYCr,      \
        .subsampling = JPEG_SUB_SAMPLE_YUV420, \
        .quality = 40,                         \
        .task_enable = false,                  \
        .hfm_task_priority = 13,               \
        .hfm_task_core = 1,                    \
        .rotate = JPEG_ROTATE_0D,              \
    }

/* JPEG configure information*/
typedef struct jpeg_info {
    int width;                      /*!< Image width */
    int height;                     /*!< Image height */
    jpeg_raw_type_t src_type;       /*!< Input image type */
    jpeg_subsampling_t subsampling; /*!< JPEG chroma subsampling factors. If `src_type` is JPEG_RAW_TYPE_YCbY2YCrY2, this must be `JPEG_SUB_SAMPLE_Y` or `JPEG_SUB_SAMPLE_YUV420`. Others is un-support */
    uint8_t quality;                /*!< Quality: 1-100, higher is better. Typical values are around 40 - 100. */
    bool task_enable;               /*!< True: `jpeg_enc_open`  would create task to finish part of encoding work. false: no task help the encoder encode */
    uint8_t hfm_task_priority;      /*!< Task priority.If task_enable is true, this must be set */
    uint8_t hfm_task_core;          /*!< Task core.If task_enable is true, this must be set */
    jpeg_rotate_t rotate;           /*!< Supports 0, 90 180 270 degree clockwise rotation.
                                         Under src_type = JPEG_RAW_TYPE_YCbYCr, subsampling = JPEG_SUB_SAMPLE_YUV420, width % 16 == 0. height % 16 = 0 conditions, rotation enabled.
                                         Otherwise unsupported */
} jpeg_enc_info_t;

/**
 * @brief      Create an JPEG handle to encode
 *
 * @param      info  The configuration information
 *
 * @return     >0: The JPEG encoder handle
 *             NULL: refer to `jpeg_error_t`
 */
void *jpeg_enc_open(jpeg_enc_info_t *info);

/**
 * @brief      Encode one image
 *
 * @param      handle      The JPEG encoder handle. It gained from `jpeg_enc_open`
 * @param      in_buf      The input buffer, It needs a completed image. The buffer must be aligned 16 byte.
 * @param      inbuf_size  The size of `in_buf`. The value must be size of a completed image.
 * @param      out_buf     The output buffer, it saves a completed JPEG image. The size must be gather than 700 bytes.
 * @param      outbuf_size The size of output buffer
 * @param      out_size    The size of JPEG image
 *
 * @return     `JPEG_ERR_OK`: It has finished to encode one image.
 *             other values refer to `jpeg_error_t`
 */
jpeg_error_t jpeg_enc_process(const void *handle, const uint8_t *in_buf, int inbuf_size, uint8_t *out_buf, int outbuf_size, int *out_size);

/**
 * @brief      Get block size. Block size is minimum process unit.
 *
 * @param      handle      The JPEG encoder handle. It gained from `jpeg_enc_open`
 *
 * @return     positive value     block size
 */
int jpeg_enc_get_block_size(const void *handle);

/**
 * @brief      Encode block size image. Get block size from  `jpeg_enc_get_block_size`
 *
 * @param      handle      The JPEG encoder handle. It gained from `jpeg_enc_open`
 * @param      in_buf      The input buffer, It needs a completed image.
 * @param      inbuf_size  The size of `in_buf`. Get block size from  `jpeg_enc_get_block_size`
 * @param      out_buf     The output buffer, it saves a completed JPEG image. The size must be gather than 700 bytes.
 * @param      outbuf_size The size of output buffer
 * @param      out_size    The size of JPEG image
 *
 * @return     block size: It has finished to encode current block size image.
 *             `JPEG_ERR_OK`: It has finished to encode one image.
 *             `JPEG_ERR_FAIL`:   Encoder failed
 */
int jpeg_enc_process_with_block(const void *handle, const uint8_t *in_buf, int inbuf_size, uint8_t *out_buf, int outbuf_size, int *out_size);

/**
 * @brief      Deinit JPEG handle
 *
 * @param      handle      The JPEG encoder handle. It gained from `jpeg_enc_open`
 *
 * @return     `JPEG_ERR_OK`     It has finished to deinit.
 */
jpeg_error_t jpeg_enc_close(void *handle);

/**
 * @brief      Reset quality.
 *
 * @param      handle      The JPEG encoder handle. It gained from `jpeg_enc_open`
 * @param      q            Quality: 1-100, higher is better. Typical values are around 40 - 100.
 *
 * @return     JPEG_ERR_Ok    succeed
 *             others         failed
 */
jpeg_error_t jpeg_enc_set_quality(const void *handle, uint8_t q);

/**
 * Example usage:
 * demo1: It is to encode one image using `jpeg_enc_process`
 * @code{c}
 *
 * int esp_jpeg_enc_demo1 () {
 *  /// configure encoder
 *  jpeg_enc_info_t info = DEFAULT_JPEG_ENC_CONFIG();
 *
 *  /// allocate input buffer to fill original image  stream.
 *  int in_len = info.width *info.height * 2;
 *  char *inbuf = jpeg_malloc_align(in_len);
 *  if(inbuf == NULL) {
 *      goto exit;
 *  }
 *  /// allocate output buffer to fill encoded image stream.
 *  int out_len = info.width * info.height * 2;
 *  char *outbuf = malloc(out_len);
 *  if (inbuf == NULL) {
 *      goto exit;
 *  }
 *  int out_size = 0;
 *  void* el = jpeg_enc_open(&info);
 *  if (el == NULL) {
 *      goto exit;
 *  }
 *  char in_name[100] = "test.yuv";
 *  char out_name[100] = "test.jpg";
 *  FILE *in = fopen(in_name, "rb");
 *  if (el == NULL){
 *      goto exit;
 *  }
 *  FILE *out = fopen(out_name, "wb");
 *  if (el == NULL) {
 *      fclose(in);
 *      goto exit;
 *  }
 *  int ret = fread(inbuf, 1, in_len, in);
 *  if(ret <= 0) {
 *      fclose(in);
 *      fclose(out);
 *      goto exit;
 *  }
 *  jpeg_enc_process(el, inbuf, in_len, outbuf, out_len, &out_size);
 *  ret = fwrite(outbuf, 1, out_size, out);
 *  if (ret <= 0){
 *      fclose(in);
 *      fclose(out);
 *      goto exit;
 *  }
 *  fclose(in);
 *  fclose(out);
 *  exit:
 *  jpeg_enc_close(el);
 *  free(inbuf);
 *  free(outbuf);
 * }

 *  demo2: It is to encode one image using `jpeg_enc_process_with_block`
 * int esp_jpeg_enc_demo2 () {
 *  jpeg_enc_info_t info = DEFAULT_JPEG_ENC_CONFIG();
 *  int in_len = jpeg_enc_get_block_size(el);
 *  char *inbuf = jpeg_malloc_align(in_len);
 *  if(inbuf == NULL) {
 *      goto exit;
 *  }
 *  int num_times = info.width * info.height * 3 / in_len;
 *  int out_len = info.width * info.height * 2;
 *  char *outbuf = malloc(out_len);
 *  if (inbuf == NULL){
 *      goto exit;
 *  }
 *  int out_size = 0;
 *  void* el = jpeg_enc_open(&info);
 *  if (el == NULL){
 *      goto exit;
 *  }
 *
 *  char in_name[100] = "test.yuv";
 *  char out_name[100] = "test.jpg";
 *  FILE *in = fopen(in_name, "rb") if (el == NULL)
 *  {
 *      goto exit;
 *  }
 *  FILE *out = fopen(out_name, "wb") if (el == NULL)
 *  {
 *      fclose(in);
 *      goto exit;
 *  }
 *  for (size_t j = 0; j < num_times; j++) {
 *     int ret = fread(inbuf, 1, in_len, in);
 *     if(ret <= 0) {
 *         ret = fwrite(outbuf, 1, out_size, out);
 *         fclose(in);
 *         fclose(out);
 *         goto exit;
 *     }
 *     jpeg_enc_process_with_block(el, inbuf, in_len, outbuf, out_len, &out_size);
 *  }
 *  fclose(in);
 *  fclose(out);
 *  exit:
 *  jpeg_enc_close(el);
 *  free(inbuf);
 *  free(outbuf);
 * }
 * @endcode
 */

#ifdef __cplusplus
}
#endif
