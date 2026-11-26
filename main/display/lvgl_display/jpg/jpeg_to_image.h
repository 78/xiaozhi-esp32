#include "sdkconfig.h"
#ifndef CONFIG_IDF_TARGET_ESP32

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decodes a JPEG image from memory to raw RGB565 pixel data
 *
 * This function attempts to decode a JPEG image using hardware acceleration first (if enabled),
 * falling back to a software decoder if hardware decoding fails or is unavailable.
 *
 * @param[in] src Pointer to the JPEG bitstream in memory
 * @param[in] src_len Length of the JPEG bitstream in bytes
 * @param[out] out Pointer to a buffer pointer that will be set to the decoded image data.
 *             This buffer is allocated internally and MUST be freed by the caller using heap_caps_free().
 * @param[out] out_len Pointer to a variable that will receive the size of the decoded image data in bytes
 * @param[out] width Pointer to a variable that will receive the image width in pixels
 * @param[out] height Pointer to a variable that will receive the image height in pixels
 * @param[out] stride Pointer to a variable that will receive the image stride in bytes
 *
 * @return ESP_OK on successful decoding
 * @return ESP_ERR_INVALID_ARG on invalid parameters
 * @return ESP_ERR_NO_MEM on memory allocation failure
 * @return ESP_FAIL on failure
 *
 * @attention Memory Management for `*out`:
 *            - The function allocates memory for the decoded image internally
 *            - On success, the caller takes ownership of this memory and SHOULD free it using heap_caps_free()
 *            - On failure, `*out` is guaranteed to be NULL and no freeing is required
 *            - Example usage:
 *              @code{.c}
 *              uint8_t *image = NULL;
 *              size_t len, width, height;
 *              if (jpeg_to_image(jpeg_data, jpeg_len, &image, &len, &width, &height)) {
 *                  // Use image data...
 *                  heap_caps_free(image);  // Critical: use heap_caps_free
 *              }
 *              @endcode
 *
 * @note Configuration dependency:
 *       - When CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_DECODER is enabled, hardware acceleration is attempted first
 *       - Both hardware and software paths allocate memory that requires heap_caps_free() for deallocation
 *       - The decoded image format is always RGB565 (2 bytes per pixel)
 *
 * @note When using hardware decoder, the decoded image dimensions might be aligned up to 16-byte boundaries.
 *       For YUV420 or YUV422 compressed images, both width and height will be rounded up to the nearest multiple of 16.
 *       See details at
 *       <https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/jpeg.html#jpeg-decoder-engine>
 *
 */
esp_err_t jpeg_to_image(const uint8_t* src, size_t src_len, uint8_t** out, size_t* out_len, size_t* width,
                        size_t* height, size_t* stride);

/**
 * @brief Same as jpeg_to_image(), but the software decoder scales the image down to fit within
 *        max_width x max_height (0 = no limit on that side), keeping the aspect ratio.
 *
 * Decoding a large camera frame at full size needs a large contiguous buffer (1280x720 RGB565 is
 * about 1.8 MB); scaling it to the display size while decoding avoids that.
 *
 * @note The output size is rounded to multiples of 8, and the scale is at most 1/8; images that
 *       need more are decoded at 1/8, so the output can exceed max_width x max_height. The
 *       hardware decoder does not scale, so its output is always full size.
 */
esp_err_t jpeg_to_image_fit(const uint8_t* src, size_t src_len, size_t max_width, size_t max_height,
                            uint8_t** out, size_t* out_len, size_t* width, size_t* height,
                            size_t* stride);

#ifdef __cplusplus
}
#endif

#endif  // CONFIG_IDF_TARGET_ESP32