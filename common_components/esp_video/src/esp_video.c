/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_heap_caps.h"
#include "esp_video.h"
#include "esp_video_vfs.h"
#include "esp_cam_sensor.h"

#include "freertos/portmacro.h"

#define ALLOC_RAM_ATTR (MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)

#if CONFIG_ESP_VIDEO_CHECK_PARAMETERS
#define CHECK_VIDEO_OBJ(v)                                  \
{                                                           \
    esp_err_t __r = esp_video_check_video_obj(v);           \
    if (__r != ESP_OK) {                                    \
        return __r;                                         \
    }                                                       \
}
#define CHECK_PARAM                         ESP_RETURN_ON_FALSE
#else
#define CHECK_VIDEO_OBJ(v)
#define CHECK_PARAM(...)
#endif

struct esp_video_format_desc_map {
    uint32_t pixel_format;
    char desc_string[30];
};

static _lock_t s_video_lock;
static SLIST_HEAD(esp_video_list, esp_video) s_video_list = SLIST_HEAD_INITIALIZER(s_video_list);
static const char *TAG = "esp_video";

static const struct esp_video_format_desc_map esp_video_format_desc_maps[] = {
    {
        V4L2_PIX_FMT_SBGGR8, "RAW8 BGGR",
    },
    {
        V4L2_PIX_FMT_RGB565, "RGB 5-6-5",
    },
    {
        V4L2_PIX_FMT_RGB24,  "RGB 8-8-8",
    },
    {
        V4L2_PIX_FMT_YUV420, "YUV 4:2:0",
    },
    {
        V4L2_PIX_FMT_YUV422P, "YVU 4:2:2 planar"
    },
    {
        V4L2_PIX_FMT_JPEG,   "JPEG"
    },
    {
        V4L2_PIX_FMT_GREY,   "Grey 8"
    },
};

/**
 * @brief Get pixel format description string
 *
 * @param pixel_format Pixel format
 * @param buffer       pixel format description string
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t esp_video_get_format_desc(uint32_t pixel_format, char *buffer)
{
    for (int i = 0; i < ARRAY_SIZE(esp_video_format_desc_maps); i++) {
        if (esp_video_format_desc_maps[i].pixel_format == pixel_format) {
            strcpy(buffer, esp_video_format_desc_maps[i].desc_string);
            return ESP_OK;
        }
    }

    return ESP_ERR_INVALID_ARG;
}

/**
 * @brief Get video buffer type.
 *
 * @param video  Video object
 *
 * @return the type left shift bits
 */
uint32_t esp_video_get_buffer_type_bits(struct esp_video *video)
{
    uint32_t buffer_type_bits = 0;

    if (video->caps & V4L2_CAP_VIDEO_CAPTURE) {
        buffer_type_bits = 0x1 << V4L2_BUF_TYPE_VIDEO_CAPTURE;
    } else if (video->caps & V4L2_CAP_VIDEO_OUTPUT) {
        buffer_type_bits = 0x1 << V4L2_BUF_TYPE_VIDEO_OUTPUT;
    } else if (video->caps & V4L2_CAP_VIDEO_M2M) {
        buffer_type_bits = (0x1 << V4L2_BUF_TYPE_VIDEO_CAPTURE) | (0x1 << V4L2_BUF_TYPE_VIDEO_OUTPUT);
    }

    return buffer_type_bits;
}

/**
 * @brief Set video stream buffer
 *
 * @param video  Video object
 * @param type   Video stream type
 * @param buffer video buffer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_stream_buffer(struct esp_video *video, enum v4l2_buf_type type, struct esp_video_buffer *buffer)
{
    if (video->caps & V4L2_CAP_VIDEO_CAPTURE) {
        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            if (video->stream) {
                video->stream->buffer = buffer;
            }
        }
    } else if (video->caps & V4L2_CAP_VIDEO_OUTPUT) {
        if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
            if (video->stream) {
                video->stream->buffer = buffer;
            }
        }
    } else if (video->caps & V4L2_CAP_VIDEO_M2M) {
        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            if (video->stream) {
                video->stream[0].buffer = buffer;
            }
        } else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
            if (video->stream) {
                video->stream[1].buffer = buffer;
            }
        }
    } else if (video->caps & V4L2_CAP_META_CAPTURE) {
        if (type == V4L2_BUF_TYPE_META_CAPTURE) {
            if (video->stream) {
                video->stream[0].buffer = buffer;
            }
        }
    }

    return ESP_OK;
}

/**
 * @brief Set video priv data
 *
 * @param video  Video object
 * @param priv   priv data to be set
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_priv_data(struct esp_video *video, void *priv)
{
    if (!video) {
        return ESP_ERR_INVALID_ARG;
    }

    video->priv = priv;
    return ESP_OK;
}

/**
 * @brief Get video stream object pointer by stream type.
 *
 * @param video  Video object
 * @param type   Video stream type
 *
 * @return Video stream object pointer
 */
struct esp_video_stream *IRAM_ATTR esp_video_get_stream(struct esp_video *video, enum v4l2_buf_type type)
{
    struct esp_video_stream *stream = NULL;

    if (video->caps & V4L2_CAP_VIDEO_CAPTURE) {
        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            stream = video->stream;
        }
    } else if (video->caps & V4L2_CAP_VIDEO_OUTPUT) {
        if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
            stream = video->stream;
        }
    } else if (video->caps & V4L2_CAP_VIDEO_M2M) {
        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            stream = &video->stream[0];
        } else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
            stream = &video->stream[1];
        }
    } else if (video->caps & V4L2_CAP_META_CAPTURE) {
        if (type == V4L2_BUF_TYPE_META_CAPTURE) {
            stream = &video->stream[0];
        }
    }

    return stream;
}

/**
 * @brief Get video object by name
 *
 * @param name The video object name
 *
 * @return Video object pointer if found by name
 */
struct esp_video *esp_video_device_get_object(const char *name)
{
    struct esp_video *video;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(video, &s_video_list, node) {
        ESP_LOGD(TAG, "dev_name=%s", video->dev_name);
        if (!strcmp(video->dev_name, name)) {
            _lock_release(&s_video_lock);
            return video;
        }
    }

    _lock_release(&s_video_lock);
    return NULL;
}

#if CONFIG_ESP_VIDEO_CHECK_PARAMETERS
/**
 * @brief Check if video is valid
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t esp_video_check_video_obj(struct esp_video *video)
{
    bool found = false;
    struct esp_video *it;

    if (!video) {
        ESP_LOGE(TAG, "video=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
#endif

/**
 * @brief Create video object.
 *
 * @param name         video driver name
 * @param id           video device ID, if id=0, VFS name is /dev/video0
 * @param ops          video operations
 * @param priv         video private data
 * @param caps         video physical device capabilities
 * @param device_caps  video software device capabilities
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_create(const char *name, uint8_t id, const struct esp_video_ops *ops,
                                   void *priv, uint32_t caps, uint32_t device_caps)
{
    esp_err_t ret;
    struct esp_video *video;
    uint32_t size;
    int stream_count;
    char vfs_name[8];

    CHECK_PARAM(name && ops, NULL, TAG, "name or ops is null");
    CHECK_PARAM(ops->set_format, NULL, TAG, "set_format is null");

    _lock_acquire(&s_video_lock);

#if CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;

    SLIST_FOREACH(video, &s_video_list, node) {
        if (!strcmp(video->dev_name, name)) {
            found = true;
            break;
        }
        if (id == video->id) {
            found = true;
            break;
        }
    }

    if (found) {
        ESP_LOGE(TAG, "video name=%s id=%d has been registered", name, id);
        goto exit_0;
    }
#endif

    size = sizeof(struct esp_video) + strlen(name) + 1;
    video = heap_caps_calloc(1, size, ALLOC_RAM_ATTR);
    if (!video) {
        ESP_LOGE(TAG, "Failed to malloc for video");
        goto exit_0;
    }

    stream_count = caps & V4L2_CAP_VIDEO_M2M ? 2 : 1;
    video->stream = heap_caps_calloc(stream_count, sizeof(struct esp_video_stream), ALLOC_RAM_ATTR);
    if (!video->stream) {
        ESP_LOGE(TAG, "Failed to malloc for stream");
        goto exit_1;
    }

    video->mutex = xSemaphoreCreateMutex();
    if (!video->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        goto exit_2;
    }

    video->dev_name = (char *)&video[1];
    strcpy(video->dev_name, name);
    video->ops = ops;
    video->priv = priv;
    video->id = id;
    video->caps = caps;
    video->device_caps = device_caps;
    SLIST_INSERT_HEAD(&s_video_list, video, node);

    ret = snprintf(vfs_name, sizeof(vfs_name), "video%d", id);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to register video VFS dev");
        goto exit_3;
    }

    ret = esp_video_vfs_dev_register(vfs_name, video);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register video VFS dev name=%s", vfs_name);
        goto exit_3;
    }

    _lock_release(&s_video_lock);
    return video;

exit_3:
    SLIST_REMOVE(&s_video_list, video, esp_video, node);
    vSemaphoreDelete(video->mutex);
exit_2:
    heap_caps_free(video->stream);
exit_1:
    heap_caps_free(video);
exit_0:
    _lock_release(&s_video_lock);
    return NULL;
}

/**
 * @brief Destroy video object.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy(struct esp_video *video)
{
    esp_err_t ret;
    char vfs_name[8];

    CHECK_VIDEO_OBJ(video);

    ret = snprintf(vfs_name, sizeof(vfs_name), "video%d", video->id);
    if (ret <= 0) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_video_vfs_dev_unregister(vfs_name);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to unregister video VFS dev name=%s", vfs_name);
        return ESP_ERR_NO_MEM;
    }

    _lock_acquire(&s_video_lock);
    SLIST_REMOVE(&s_video_list, video, esp_video, node);
    _lock_release(&s_video_lock);

    vSemaphoreDelete(video->mutex);
    heap_caps_free(video->stream);
    heap_caps_free(video);

    return ESP_OK;
}

/**
 * @brief Open a video device, this function will initialize hardware.
 *
 * @param name video device name
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_open(const char *name)
{
    esp_err_t ret;
    bool found = false;
    struct esp_video *video;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(video, &s_video_list, node) {
        if (!strcmp(video->dev_name, name)) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%s", name);
        return NULL;
    }

    xSemaphoreTake(video->mutex, portMAX_DELAY);

    assert(video->reference <= UINT8_MAX);
    video->reference++;
    if (video->reference > 1) {
        goto exit_0;
    }

    if (video->ops->init) {
        /* video device operation "init" sets buffer information and video format */

        ret = video->ops->init(video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->init=%x", ret);
            video = NULL;
        } else {
            int stream_count = video->caps & V4L2_CAP_VIDEO_M2M ? 2 : 1;

            portMUX_INITIALIZE(&video->stream_lock);
            for (int i = 0; i < stream_count; i++) {
                struct esp_video_stream *stream = &video->stream[i];

                stream->buffer = NULL;
                SLIST_INIT(&stream->queued_list);
                SLIST_INIT(&stream->done_list);
            }
        }
    } else {
        ESP_LOGD(TAG, "video->ops->init=NULL");
    }

exit_0:
    xSemaphoreGive(video->mutex);
    return video;
}

/**
 * @brief Close a video device, this function will de-initialize hardware.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_close(struct esp_video *video)
{
    esp_err_t ret = ESP_OK;

    CHECK_VIDEO_OBJ(video);

    xSemaphoreTake(video->mutex, portMAX_DELAY);

    assert(video->reference > 0);
    video->reference--;
    if (video->reference > 0) {
        goto exit_0;
    }

    if (video->ops->deinit) {
        ret = video->ops->deinit(video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->deinit=%x", ret);
        } else {
            int stream_count = video->caps & V4L2_CAP_VIDEO_M2M ? 2 : 1;

            for (int i = 0; i < stream_count; i++) {
                struct esp_video_stream *stream = &video->stream[i];

                if (stream->ready_sem) {
                    vSemaphoreDelete(stream->ready_sem);
                    stream->ready_sem = NULL;
                }

                if (stream->buffer) {
                    esp_video_buffer_destroy(stream->buffer);
                    stream->buffer = NULL;
                }
            }
        }
    } else {
        ESP_LOGD(TAG, "video->ops->deinit=NULL");
    }

exit_0:
    xSemaphoreGive(video->mutex);
    return ret;
}

/**
 * @brief Start capturing video data stream.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_start_capture(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    struct esp_video_stream *stream;

    CHECK_VIDEO_OBJ(video);

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    if (stream->started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (video->ops->start) {
        ret = video->ops->start(video, type);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->start=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGD(TAG, "video->ops->start=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    stream->started = true;

    return ESP_OK;
}

/**
 * @brief Stop capturing video data stream.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_stop_capture(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    struct esp_video_stream *stream;

    CHECK_VIDEO_OBJ(video);

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!stream->started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (video->ops->stop) {
        ret = video->ops->stop(video, type);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->stop=%x", ret);
            return ret;
        } else {
            int stream_count = video->caps & V4L2_CAP_VIDEO_M2M ? 2 : 1;

            for (int i = 0; i < stream_count; i++) {
                struct esp_video_stream *stream = &video->stream[i];

                do {
                    ret = xSemaphoreTake(stream->ready_sem, 0);
                } while (ret == pdTRUE);

                SLIST_INIT(&stream->queued_list);
                SLIST_INIT(&stream->done_list);

                esp_video_buffer_reset(stream->buffer);
            }
        }
    } else {
        ESP_LOGD(TAG, "video->ops->stop=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    stream->started = false;

    return ESP_OK;
}

/**
 * @brief Enumerate video format description.
 *
 * @param video  Video object
 * @param type   Video stream type
 * @param index  Pixel format index
 * @param desc   Pixel format enum buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, struct esp_video_format_desc *desc)
{
    esp_err_t ret;
    struct esp_video_stream *stream;

    CHECK_VIDEO_OBJ(video);
    CHECK_PARAM(desc, ESP_ERR_INVALID_ARG, TAG, "desc=NULL");

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    if (video->ops->enum_format) {
        ret = video->ops->enum_format(video, type, index, &desc->pixel_format);
        if (ret != ESP_OK) {
            return ret;
        } else {
            ret = esp_video_get_format_desc(desc->pixel_format, desc->description);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to get format description");
                return ret;
            }
        }
    } else {
        ESP_LOGD(TAG, "video->ops->enum_format=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * @brief Get video format information.
 *
 * @param video  Video object
 * @param format V4L2 format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_format(struct esp_video *video, struct v4l2_format *format)
{
    struct esp_video_stream *stream;

    CHECK_VIDEO_OBJ(video);
    CHECK_PARAM(format, ESP_ERR_INVALID_ARG, TAG, "format=NULL");

    stream = esp_video_get_stream(video, format->type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(format, &stream->format, sizeof(struct v4l2_format));

    return ESP_OK;
}

/**
 * @brief Set video format information.
 *
 * @param video  Video object
 * @param format V4L2 format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    esp_err_t ret;
    struct esp_video_stream *stream;

    CHECK_VIDEO_OBJ(video);
    CHECK_PARAM(format, ESP_ERR_INVALID_ARG, TAG, "format=NULL");

    stream = esp_video_get_stream(video, format->type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = video->ops->set_format(video, format);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "video->ops->set_format=%x", ret);
        return ret;
    } else {
        memcpy(&stream->format, format, sizeof(struct v4l2_format));
    }

    return ESP_OK;
}

/**
 * @brief Setup video buffer.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param memory_type Video buffer memory type, refer to v4l2_memory in videodev2.h
 * @param count Video buffer count
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_setup_buffer(struct esp_video *video, uint32_t type, uint32_t memory_type, uint32_t count)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_info *info;

    CHECK_VIDEO_OBJ(video);

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    /* buffer_size is configured when setting format */

    info = &stream->buf_info;
    if (!info->size || !info->align_size || !info->caps) {
        ESP_LOGE(TAG, "Failed to check buffer information: size=%" PRIu32 " align=%" PRIu32 " cap=%" PRIx32,
                 info->size, info->align_size, info->caps);
        return ESP_ERR_INVALID_STATE;
    }

    info->count = count;
    info->memory_type = memory_type;

    if (stream->ready_sem) {
        vSemaphoreDelete(stream->ready_sem);
        stream->ready_sem = NULL;
    }

    if (stream->buffer) {
        esp_video_buffer_destroy(stream->buffer);
        stream->buffer = NULL;
    }

    stream->ready_sem = xSemaphoreCreateCounting(info->count, 0);
    if (!stream->ready_sem) {
        ESP_LOGE(TAG, "Failed to create done_sem for video stream");
        return ESP_ERR_NO_MEM;
    }

    stream->buffer = esp_video_buffer_create(info);
    if (!stream->buffer) {
        vSemaphoreDelete(stream->ready_sem);
        stream->ready_sem = NULL;
        ESP_LOGE(TAG, "Failed to create buffer");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/**
 * @brief Get video buffer count.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param attr  Video stream buffer information pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_buffer_info(struct esp_video *video, uint32_t type, struct esp_video_buffer_info *info)
{
    struct esp_video_stream *stream;

    CHECK_VIDEO_OBJ(video);

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(info, &stream->buf_info, sizeof(struct esp_video_buffer_info));

    return ESP_OK;
}

/**
 * @brief Get buffer element from buffer queued list.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *IRAM_ATTR esp_video_get_queued_element(struct esp_video *video, uint32_t type)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element = NULL;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    portENTER_CRITICAL_SAFE(&video->stream_lock);
    if (!SLIST_EMPTY(&stream->queued_list)) {
        element = SLIST_FIRST(&stream->queued_list);
        SLIST_REMOVE(&stream->queued_list, element, esp_video_buffer_element, node);
        ELEMENT_SET_FREE(element);
    }
    portEXIT_CRITICAL_SAFE(&video->stream_lock);

    return element;
}

/**
 * @brief Get buffer element's payload from buffer queued list.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
uint8_t *IRAM_ATTR esp_video_get_queued_buffer(struct esp_video *video, uint32_t type)
{
    struct esp_video_buffer_element *element;

    element = esp_video_get_queued_element(video, type);
    if (!element) {
        return NULL;
    }

    return element->buffer;
}

/**
 * @brief Get buffer element from buffer done list.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_get_done_element(struct esp_video *video, uint32_t type)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element = NULL;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    portENTER_CRITICAL_SAFE(&video->stream_lock);
    if (!SLIST_EMPTY(&stream->done_list)) {
        element = SLIST_FIRST(&stream->done_list);
        SLIST_REMOVE(&stream->done_list, element, esp_video_buffer_element, node);
        ELEMENT_SET_FREE(element);
    }
    portEXIT_CRITICAL_SAFE(&video->stream_lock);

    return element;
}

/**
 * @brief Put element into done lost and give semaphore.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video buffer element object get by "esp_video_get_queued_element"
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t IRAM_ATTR esp_video_done_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element)
{
    struct esp_video_stream *stream;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL_SAFE(&video->stream_lock);
    if (!ELEMENT_IS_FREE(element)) {
        portEXIT_CRITICAL_SAFE(&video->stream_lock);
        return ESP_ERR_INVALID_ARG;
    }

    ELEMENT_SET_ALLOCATED(element);
    SLIST_INSERT_HEAD(&stream->done_list, element, node);
    portEXIT_CRITICAL_SAFE(&video->stream_lock);

    if (xPortInIsrContext()) {
        BaseType_t wakeup = pdFALSE;

        xSemaphoreGiveFromISR(stream->ready_sem, &wakeup);
        if (wakeup == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    } else {
        xSemaphoreGive(stream->ready_sem);
    }

    return ESP_OK;
}

/**
 * @brief Process a video buffer element's payload which receives data done.
 *
 * @param video  Video object
 * @param type   Video stream type
 * @param buffer Video buffer element's payload
 * @param n      Video buffer element's payload valid data size
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t IRAM_ATTR esp_video_done_buffer(struct esp_video *video, uint32_t type, uint8_t *buffer, uint32_t n)
{
    esp_err_t ret;
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    element = esp_video_buffer_get_element_by_buffer(stream->buffer, buffer);
    if (element) {
        element->valid_size = n;
        ret = esp_video_done_element(video, type, element);
        if (ret != ESP_OK) {
            return ret;
        }
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief Put buffer element into queued list.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video buffer element
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_queue_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element)
{
    uint32_t val = type;
    struct esp_video_stream *stream;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL_SAFE(&video->stream_lock);
    if (!ELEMENT_IS_FREE(element)) {
        portEXIT_CRITICAL_SAFE(&video->stream_lock);
        return ESP_ERR_INVALID_ARG;
    }

    ELEMENT_SET_ALLOCATED(element);
    SLIST_INSERT_HEAD(&stream->queued_list, element, node);
    portEXIT_CRITICAL_SAFE(&video->stream_lock);

    if (video->ops->notify) {
        video->ops->notify(video, ESP_VIDEO_BUFFER_VALID, &val);
    }

    return ESP_OK;
}

/**
 * @brief Put buffer element index into queued list.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param index   Video buffer element index
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_queue_element_index(struct esp_video *video, uint32_t type, int index)
{
    esp_err_t ret;
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    element = ESP_VIDEO_BUFFER_ELEMENT(stream->buffer, index);

    ret = esp_video_queue_element(video, type, element);

    return ret;
}

/**
 * @brief Put buffer element index into queued list.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param index   Video buffer element index
 * @param buffer  Receive buffer pointer from user space
 * @param size    Receive buffer size
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_queue_element_index_buffer(struct esp_video *video, uint32_t type, int index, uint8_t *buffer, uint32_t size)
{
    esp_err_t ret;
    struct esp_video_stream *stream;
    struct esp_video_buffer_info *info;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    element = ESP_VIDEO_BUFFER_ELEMENT(stream->buffer, index);
    info = &stream->buffer->info;

    if ((info->memory_type != V4L2_MEMORY_USERPTR) ||
            (((uintptr_t)buffer) % info->align_size) ||
            (size < info->size)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (info->caps & MALLOC_CAP_SPIRAM) {
        if (!esp_ptr_external_ram(buffer)) {
            return ESP_ERR_INVALID_ARG;
        }
    } else if (info->caps & MALLOC_CAP_INTERNAL) {
        if (!esp_ptr_internal(buffer)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    element->buffer = buffer;
    element->valid_size = size;

    ret = esp_video_queue_element(video, type, element);

    return ret;
}

/**
 * @brief Get buffer element payload.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param index Video buffer element index
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
uint8_t *esp_video_get_element_index_payload(struct esp_video *video, uint32_t type, int index)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    element = ESP_VIDEO_BUFFER_ELEMENT(stream->buffer, index);

    return element->buffer;
}

/**
 * @brief Receive buffer element from video device.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param ticks Wait OS tick
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_recv_element(struct esp_video *video, uint32_t type, uint32_t ticks)
{
    BaseType_t ret;
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    if (video->device_caps & V4L2_CAP_VIDEO_M2M) {
        uint32_t val = type;

        /**
         * Software M2M device: this callback call can do real codec process.
         * Hardware M2M device: this callback call can start hardware if necessary.
         */

        ret = video->ops->notify(video, ESP_VIDEO_M2M_TRIGGER, &val);
        if (ret != ESP_OK) {
            return NULL;
        }
    }

    ret = xSemaphoreTake(stream->ready_sem, (TickType_t)ticks);
    if (ret != pdTRUE) {
        return NULL;
    }

    element = esp_video_get_done_element(video, type);

    return element;
}

/**
 * @brief Put buffer elements into M2M buffer queue list.
 *
 * @param video       Video object
 * @param src_type    Video resource stream type
 * @param src_element Video resource stream buffer element
 * @param dst_type    Video destination stream type
 * @param dst_element Video destination stream buffer element
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_queue_m2m_elements(struct esp_video *video,
                                       uint32_t src_type,
                                       struct esp_video_buffer_element *src_element,
                                       uint32_t dst_type,
                                       struct esp_video_buffer_element *dst_element)
{
    esp_err_t ret;
    struct esp_video_stream *stream[2];

    stream[0] = esp_video_get_stream(video, src_type);
    if (!stream[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    stream[1] = esp_video_get_stream(video, dst_type);
    if (!stream[1]) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL_SAFE(&video->stream_lock);
    if (ELEMENT_IS_FREE(src_element) && ELEMENT_IS_FREE(dst_element)) {
        ELEMENT_SET_ALLOCATED(src_element);
        SLIST_INSERT_HEAD(&stream[0]->queued_list, src_element, node);

        ELEMENT_SET_ALLOCATED(dst_element);
        SLIST_INSERT_HEAD(&stream[1]->queued_list, dst_element, node);

        ret = ESP_OK;
    } else {
        ret = ESP_ERR_INVALID_STATE;
    }
    portEXIT_CRITICAL_SAFE(&video->stream_lock);

    return ret;
}

/**
 * @brief Put buffer elements into M2M buffer done list.
 *
 * @param video       Video object
 * @param src_type    Video resource stream type
 * @param src_element Video resource stream buffer element
 * @param dst_type    Video destination stream type
 * @param dst_element Video destination stream buffer element
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_done_m2m_elements(struct esp_video *video,
                                      uint32_t src_type,
                                      struct esp_video_buffer_element *src_element,
                                      uint32_t dst_type,
                                      struct esp_video_buffer_element *dst_element)
{
    esp_err_t ret;
    bool user_node = true;
    struct esp_video_stream *stream[2];

    stream[0] = esp_video_get_stream(video, src_type);
    if (!stream[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    stream[1] = esp_video_get_stream(video, dst_type);
    if (!stream[1]) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL_SAFE(&video->stream_lock);
    if (ELEMENT_IS_FREE(src_element) && ELEMENT_IS_FREE(dst_element)) {
        ELEMENT_SET_ALLOCATED(src_element);
        SLIST_INSERT_HEAD(&stream[0]->done_list, src_element, node);

        ELEMENT_SET_ALLOCATED(dst_element);
        SLIST_INSERT_HEAD(&stream[1]->done_list, dst_element, node);

        ret = ESP_OK;
    } else {
        ret = ESP_ERR_INVALID_STATE;
    }
    portEXIT_CRITICAL_SAFE(&video->stream_lock);

    if (ret == ESP_OK && user_node) {
        if (xPortInIsrContext()) {
            BaseType_t wakeup = pdFALSE;

            xSemaphoreGiveFromISR(stream[0]->ready_sem, &wakeup);
            if (wakeup == pdTRUE) {
                portYIELD_FROM_ISR();
            }

            xSemaphoreGiveFromISR(stream[1]->ready_sem, &wakeup);
            if (wakeup == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        } else {
            xSemaphoreGive(stream[0]->ready_sem);
            xSemaphoreGive(stream[1]->ready_sem);
        }
    }

    return ret;
}

/**
 * @brief Get buffer elements from M2M buffer queue list.
 *
 * @param video       Video object
 * @param src_type    Video resource stream type
 * @param src_element Video resource stream buffer element buffer
 * @param dst_type    Video destination stream type
 * @param dst_element Video destination stream buffer element buffer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_m2m_queued_elements(struct esp_video *video,
        uint32_t src_type,
        struct esp_video_buffer_element **src_element,
        uint32_t dst_type,
        struct esp_video_buffer_element **dst_element)
{
    esp_err_t ret;
    struct esp_video_stream *stream[2];

    stream[0] = esp_video_get_stream(video, src_type);
    if (!stream[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    stream[1] = esp_video_get_stream(video, dst_type);
    if (!stream[1]) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL_SAFE(&video->stream_lock);
    if (!SLIST_EMPTY(&stream[0]->queued_list) && !SLIST_EMPTY(&stream[1]->queued_list)) {
        *src_element = SLIST_FIRST(&stream[0]->queued_list);
        SLIST_REMOVE(&stream[0]->queued_list, *src_element, esp_video_buffer_element, node);
        ELEMENT_SET_FREE(*src_element);

        *dst_element = SLIST_FIRST(&stream[1]->queued_list);
        SLIST_REMOVE(&stream[1]->queued_list, *dst_element, esp_video_buffer_element, node);
        ELEMENT_SET_FREE(*dst_element);

        ret = ESP_OK;
    } else {
        ret = ESP_ERR_NOT_FOUND;
    }
    portEXIT_CRITICAL_SAFE(&video->stream_lock);

    return ret;
}

/**
 * @brief Clone video buffer
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video resource element
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_clone_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element)
{
    struct esp_video_buffer_element *new_element;

    new_element = esp_video_get_done_element(video, type);
    if (new_element) {
        new_element->valid_size = element->valid_size;
        memcpy(new_element->buffer, element->buffer, element->valid_size);
    }

    return new_element;
}

/**
 * @brief Get buffer type from video
 *
 * @param video    Video object
 * @param type     Video buffer type pointer
 * @param is_input true: buffer is input into the device; false: buffer is output from the device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_buf_type(struct esp_video *video, uint32_t *type, bool is_input)
{
    if (video->caps & V4L2_CAP_VIDEO_CAPTURE) {
        if (is_input) {
            return ESP_ERR_INVALID_ARG;
        }

        *type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    } else if (video->caps & V4L2_CAP_VIDEO_OUTPUT) {
        if (!is_input) {
            return ESP_ERR_INVALID_ARG;
        }

        *type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    }  else if (video->caps & V4L2_CAP_VIDEO_M2M) {
        if (is_input) {
            *type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        } else {
            *type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief Set the value of several external controls
 *
 * @param video Video object
 * @param ctrls Controls array pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_ext_controls(struct esp_video *video, const struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret;

    CHECK_VIDEO_OBJ(video);

    if (video->ops->set_ext_ctrl) {
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        ret = video->ops->set_ext_ctrl(video, ctrls);
        xSemaphoreGive(video->mutex);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->set_ext_ctrl=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGD(TAG, "video->ops->set_ext_ctrl=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * @brief Get the value of several external controls
 *
 * @param video Video object
 * @param ctrls Controls array pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_ext_controls(struct esp_video *video, struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret;

    CHECK_VIDEO_OBJ(video);

    if (video->ops->get_ext_ctrl) {
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        ret = video->ops->get_ext_ctrl(video, ctrls);
        xSemaphoreGive(video->mutex);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->get_ext_ctrl=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGD(TAG, "video->ops->get_ext_ctrl=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * @brief Query the description of the control
 *
 * @param video Video object
 * @param qctrl Control description buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_query_ext_control(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_err_t ret;

    CHECK_VIDEO_OBJ(video);

    if (video->ops->query_ext_ctrl) {
        ret = video->ops->query_ext_ctrl(video, qctrl);
        if (ret == ESP_ERR_NOT_SUPPORTED) {
            return ret;
        } else if (ret != ESP_OK) {
            ESP_LOGD(TAG, "video->ops->query_ext_ctrl=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGD(TAG, "video->ops->query_ext_ctrl=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * @brief M2M video device process data
 *
 * @param video       Video object
 * @param src_type    Video resource stream type
 * @param dst_type    Video destination stream type
 * @param proc        Video device process callback function
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_m2m_process(struct esp_video *video, uint32_t src_type, uint32_t dst_type, esp_video_m2m_process_t proc)
{
    esp_err_t ret;
    uint32_t dst_out_size;
    struct esp_video_buffer_element *dst_element;
    struct esp_video_buffer_element *src_element;

    ret = esp_video_get_m2m_queued_elements(video,
                                            src_type,
                                            &src_element,
                                            dst_type,
                                            &dst_element);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "no valid buffer");
        return ret;
    }

    ret = proc(video, ELEMENT_BUFFER(src_element), ELEMENT_SIZE(src_element),
               ELEMENT_BUFFER(dst_element), ELEMENT_SIZE(dst_element), &dst_out_size);
    if (ret != ESP_OK) {
        dst_element->valid_size = 0;
    } else {
        dst_element->valid_size = dst_out_size;
    }
    ret = esp_video_done_m2m_elements(video, src_type, src_element, dst_type, dst_element);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to put elements back into done list");
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Set format to sensor
 *
 * @param video  Video object
 * @param format Sensor format pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_sensor_format(struct esp_video *video, const esp_cam_sensor_format_t *format)
{
    esp_err_t ret;

    CHECK_VIDEO_OBJ(video);

    if (video->ops->set_sensor_format) {
        ret = video->ops->set_sensor_format(video, format);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->set_sensor_format=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGD(TAG, "video->ops->set_sensor_format=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * @brief Get format from sensor
 *
 * @param video  Video object
 * @param format Sensor format pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_sensor_format(struct esp_video *video, esp_cam_sensor_format_t *format)
{
    esp_err_t ret;

    CHECK_VIDEO_OBJ(video);

    if (video->ops->get_sensor_format) {
        ret = video->ops->get_sensor_format(video, format);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->get_sensor_format=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGD(TAG, "video->ops->get_sensor_format=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * @brief Query menu value
 *
 * @param video  Video object
 * @param qmenu  Menu value buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_query_menu(struct esp_video *video, struct v4l2_querymenu *qmenu)
{
    esp_err_t ret;

    CHECK_VIDEO_OBJ(video);

    if (video->ops->query_menu) {
        ret = video->ops->query_menu(video, qmenu);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "video->ops->query_menu=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGD(TAG, "video->ops->query_menu=NULL");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}
