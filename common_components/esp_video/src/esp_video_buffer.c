/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "linux/videodev2.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_video_buffer.h"

#define ESP_VIDEO_BUFFER_ALIGN(s, a)      (((s) + ((a) - 1)) & (~((a) - 1)))

static const char *TAG = "esp_video_buffer";

/**
 * @brief Create video buffer object.
 *
 * @param info Buffer information pointer.
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_create(const struct esp_video_buffer_info *info)
{
    uint32_t size;
    struct esp_video_buffer *buffer;

    size = sizeof(struct esp_video_buffer) + sizeof(struct esp_video_buffer_element) * info->count;
    buffer = heap_caps_calloc(1, size, info->caps);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to malloc for video buffer");
        return NULL;
    }

    for (int i = 0; i < info->count; i++) {
        struct esp_video_buffer_element *element = &buffer->element[i];

        if (info->memory_type == V4L2_MEMORY_MMAP) {
            element->buffer = heap_caps_aligned_alloc(info->align_size, info->size, info->caps);
            if (element->buffer) {
                element->index = i;
                element->video_buffer = buffer;
                ELEMENT_SET_FREE(element);
            } else {
                goto exit_0;
            }
        } else {
            element->index = i;
            element->video_buffer = buffer;
            element->buffer = NULL;
            ELEMENT_SET_FREE(element);
        }
    }

    memcpy(&buffer->info, info, sizeof(struct esp_video_buffer_info));

    return buffer;

exit_0:
    for (int i = 0; i < info->count; i++) {
        struct esp_video_buffer_element *element = &buffer->element[i];

        if (element->buffer) {
            heap_caps_free(element->buffer);
        }
    }

    heap_caps_free(buffer);
    return NULL;
}

/**
 * @brief Clone a new video buffer
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_clone(const struct esp_video_buffer *buffer)
{
    if (!buffer) {
        return NULL;
    }

    return esp_video_buffer_create(&buffer->info);
}

/**
 * @brief Destroy video buffer object.
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_buffer_destroy(struct esp_video_buffer *buffer)
{
    if (buffer->info.memory_type == V4L2_MEMORY_MMAP) {
        for (int i = 0; i < buffer->info.count; i++) {
            heap_caps_free(buffer->element[i].buffer);
        }
    }

    heap_caps_free(buffer);

    return ESP_OK;
}

/**
 * @brief Get element object pointer by buffer
 *
 * @param buffer Video buffer object
 * @param ptr    Element buffer pointer
 *
 * @return
 *      - Element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *IRAM_ATTR esp_video_buffer_get_element_by_buffer(struct esp_video_buffer *buffer, uint8_t *ptr)
{
    for (int i = 0; i < buffer->info.count; i++) {
        if (buffer->element[i].buffer == ptr) {
            return &buffer->element[i];
        }
    }

    return NULL;
}


/**
 * @brief Reset video buffer
 *
 * @param buffer Video buffer object
 *
 * @return None
 */
void esp_video_buffer_reset(struct esp_video_buffer *buffer)
{
    for (int i = 0; i < buffer->info.count; i++) {
        ELEMENT_SET_FREE(&buffer->element[i]);
        buffer->element[i].valid_size = 0;
    }
}
