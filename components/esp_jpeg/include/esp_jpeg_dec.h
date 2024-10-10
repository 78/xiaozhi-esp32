// Copyright 2021 Espressif Systems (Shanghai) CO., LTD
// All rights reserved.

#ifndef ESP_JPEG_DEC_H
#define ESP_JPEG_DEC_H

/**
 * @file esp_jpeg_dec.h
 * @brief Create an JPEG decoder.
 *        Currently, support functions as follows:
 *             - Support variety of width and height to decoder
 *             - Support RGB888 RGB565(big end) RGB565(little end) raw data to output
 *             - Support 0, 90 180 270 degree clockwise rotation, under width and height are multiply of 8.
 *        The encoder do ASM optimization in ESP32S3. The encoder frame rate performs better than the others chips.
 * @version 1.0.0
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_jpeg_common.h"

#define DEFAULT_JPEG_DEC_CONFIG() {                           \
    .output_type               = JPEG_RAW_TYPE_RGB565_LE,     \
    .rotate                    = JPEG_ROTATE_0D,              \
}

#define JPEG_DEC_MAX_MARKER_CHECK_LEN (1024)

typedef void* jpeg_dec_handle_t;

/* Jpeg dec user need to config */
typedef struct {
    jpeg_raw_type_t output_type; /*!< jpeg_dec_out_type 1:rgb888 0:rgb565 */
    jpeg_rotate_t rotate;        /*!< Supports 0, 90 180 270 degree clockwise rotation.
                                      Under width % 8 == 0. height % 8 = 0 conditions, rotation enabled. Otherwise unsupported */
} jpeg_dec_config_t;

/* Jpeg dec out info */
typedef struct {
    int width;                      /* Number of pixels in the horizontal direction */
    int height;                     /* Number of pixels in the vertical direction */
    int component_num;              /* Number of color component*/
    uint8_t component_id[3];        /* ID of color component*/
    uint8_t x_factory[3];           /* Size factory in the x direction*/
    uint8_t y_factory[3];           /* Size factory in the y direction*/
    uint8_t huffbits[2][2][16];     /* Huffman bit distribution tables [id][dcac] */
    uint16_t huffdata[2][2][256];   /* Huffman decoded data tables [id][dcac] */
    uint8_t qtid[3];                /* Quantization table ID of each component */
    int16_t qt_tbl[4][64];          /* De-quantizer tables [id] */
} jpeg_dec_header_info_t;

/* Jpeg dec io control */
typedef struct {
    unsigned char *inbuf;           /* The input buffer pointer */
    int inbuf_len;                  /* The number of the input buffer */
    int inbuf_remain;               /* Not used number of the in buffer */
    unsigned char *outbuf;          /* The decoded data is placed.The buffer must be aligned 16 byte. */
} jpeg_dec_io_t;

/**
 * @brief      Create a Jpeg decode handle, set user config info to decode handle
 * 
 * @param[in]      config        The configuration information
 * 
 * @return     other values: The JPEG decoder handle
 *             NULL: failed  
 */
jpeg_dec_handle_t *jpeg_dec_open(jpeg_dec_config_t *config);

/**
 * @brief      Parse picture data header, and out put info to user
 *
 * @param[in]      jpeg_dec        jpeg decoder handle
 * 
 * @param[in]      io              struct of jpeg_dec_io_t
 * 
 * @param[out]     out_info        output info struct to user 
 * 
 * @return     jpeg_error_t
 *             - JPEG_ERR_OK: on success
 *             - Others: error occurs    
 */
jpeg_error_t jpeg_dec_parse_header(jpeg_dec_handle_t *jpeg_dec, jpeg_dec_io_t *io, jpeg_dec_header_info_t *out_info);

/**
 * @brief      Decode one Jpeg picture 
 *
 * @param[in]      jpeg_dec    jpeg decoder handle
 * 
 * @param[in]      io          struct of jpeg_dec_io_t 
 * 
 * @return     jpeg_error_t
 *             - JPEG_ERR_OK: on success
 *             - Others: error occurs    
 */
jpeg_error_t jpeg_dec_process(jpeg_dec_handle_t *jpeg_dec, jpeg_dec_io_t *io);

/**
 * @brief      Deinitialize Jpeg decode handle 
 * 
 * @param[in]      jpeg_dec    jpeg decoder handle     
 * 
 * @return     jpeg_error_t
 *             - JPEG_ERR_OK: on success
 *             - Others: error occurs    
 */
jpeg_error_t jpeg_dec_close(jpeg_dec_handle_t *jpeg_dec);

/**
 * Example usage:
 * @code{c}
 * 
 * // Function for decode one jpeg picture
 * // input_buf   input picture data
 * // len         input picture data length
 * int esp_jpeg_decoder_one_picture(unsigned char *input_buf, int len, unsigned char **output_buf)
 * {
 *      // Generate default configuration
 *      jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
 *      
 *      // Empty handle to jpeg_decoder
 *      jpeg_dec_handle_t jpeg_dec = NULL;
 *      
 *      // Create jpeg_dec
 *      jpeg_dec = jpeg_dec_open(&config);
 * 
 *      // Create io_callback handle
 *      jpeg_dec_io_t *jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
 *      if (jpeg_io == NULL) {
 *          return ESP_FAIL;
 *      }
 *      
 *      // Create out_info handle
 *      jpeg_dec_header_info_t *out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
 *      if (out_info == NULL) {
 *          return ESP_FAIL;
 *      }
 * 
 *      // Set input buffer and buffer len to io_callback
 *      jpeg_io->inbuf = input_buf;
 *      jpeg_io->inbuf_len = len;
 *      
 *      int ret = 0;
 *      // Parse jpeg picture header and get picture for user and decoder
 *      ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
 *      if (ret < 0) {
 *          return ret;
 *      }
 * 
 *      // Calloc out_put data buffer and update inbuf ptr and inbuf_len
 *      int outbuf_len;
 *      if (config.output_type == JPEG_RAW_TYPE_RGB565_LE
 *          || config.output_type == JPEG_RAW_TYPE_RGB565_BE) {
 *          outbuf_len = out_info->width * out_info->height * 2;
 *      } else if (config.output_type == JPEG_RAW_TYPE_RGB888) {
 *          outbuf_len = out_info->width * out_info->height * 3;
 *      } else {
 *          return ESP_FAIL;
 *      }
 *      unsigned char *out_buf = jpeg_malloc_align(outbuf_len, 16);
 *      if (out_buf == NULL) {
 *         return ESP_FAIL;     
 *      }
 *      jpeg_io->outbuf = out_buf;
 *      *output_buf = out_buf;
 *      int inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
 *      jpeg_io->inbuf = input_buf + inbuf_consumed;
 *      jpeg_io->inbuf_len = jpeg_io->inbuf_remain;
 * 
 *      // Start decode jpeg raw data
 *      ret = jpeg_dec_process(jpeg_dec, jpeg_io);
 *      if (ret < 0) {
 *          return ret;
 *      }
 * 
 *      // Decoder deinitialize 
 *      jpeg_free_align(out_buf);
 *      jpeg_dec_close(jpeg_dec);
 *      free(out_info);
 *      free(jpeg_io);
 *      return ESP_OK;
 * }
 * 
 * @endcode
 *  
 */

#ifdef __cplusplus
}
#endif

#endif
