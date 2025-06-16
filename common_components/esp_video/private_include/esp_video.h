/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include <sys/queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "linux/videodev2.h"
#include "esp_video_buffer.h"
#include "esp_video_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Video format description object.
 */
struct esp_video_format_desc {
    uint32_t pixel_format;                  /*!< Video frame pixel format */
    char description[30];                   /*!< Video frame pixel format description string */
};

/**
 * @brief Video stream object.
 */
struct esp_video_stream {
    bool started;                           /*!< Video stream is started */

    struct v4l2_format format;              /*!< Video stream format */
    struct esp_video_buffer_info buf_info;  /*!< Video stream buffer information */

    esp_video_buffer_list_t queued_list;    /*!< Workqueue buffer elements list */
    esp_video_buffer_list_t done_list;      /*!< Done buffer elements list */

    struct esp_video_buffer *buffer;        /*!< Video stream buffer */
    SemaphoreHandle_t ready_sem;            /*!< Video stream buffer element ready semaphore */
};

/**
 * @brief Video object.
 */
struct esp_video {
    SLIST_ENTRY(esp_video) node;            /*!< List node */

    uint8_t id;                             /*!< Video device ID */
    const struct esp_video_ops *ops;        /*!< Video operations */
    char *dev_name;                         /*!< Video device port name */
    uint32_t caps;                          /*!< video physical device capabilities */
    uint32_t device_caps;                   /*!< video software device capabilities */

    void *priv;                             /*!< Video device private data */

    portMUX_TYPE stream_lock;               /*!< Stream list lock */
    struct esp_video_stream *stream;        /*!< Video device stream, capture-only or output-only device has 1 stream, M2M device has 2 streams */

    SemaphoreHandle_t mutex;                /*!< Video device mutex lock */
    uint8_t reference;                      /*!< video device open reference count */
};

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
                                   void *priv, uint32_t caps, uint32_t device_caps);

/**
 * @brief Destroy video object.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy(struct esp_video *video);

/**
 * @brief Open a video device, this function will initialize hardware.
 *
 * @param name video device name
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_open(const char *name);

/**
 * @brief Close a video device, this function will de-initialize hardware.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_close(struct esp_video *video);

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
esp_err_t esp_video_start_capture(struct esp_video *video, uint32_t type);

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
esp_err_t esp_video_stop_capture(struct esp_video *video, uint32_t type);

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
esp_err_t esp_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, struct esp_video_format_desc *desc);

/**
 * @brief Get video format information.
 *
 * @param video     Video object
 * @param format    V4L2 format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_format(struct esp_video *video, struct v4l2_format *format);

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
esp_err_t esp_video_set_format(struct esp_video *video, const struct v4l2_format *format);

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
esp_err_t esp_video_setup_buffer(struct esp_video *video, uint32_t type, uint32_t memory_type, uint32_t count);

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
esp_err_t esp_video_get_buffer_info(struct esp_video *video, uint32_t type, struct esp_video_buffer_info *info);

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
struct esp_video_buffer_element *esp_video_get_queued_element(struct esp_video *video, uint32_t type);

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
uint8_t *esp_video_get_queued_buffer(struct esp_video *video, uint32_t type);

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
struct esp_video_buffer_element *esp_video_get_done_element(struct esp_video *video, uint32_t type);

/**
 * @brief Process a done video buffer element.
 *
 * @param video  Video object
 * @param stream Video stream object
 * @param buffer Video buffer element object allocated by "esp_video_get_queued_element"
 *
 * @return None
 */
void esp_video_stream_done_element(struct esp_video *video, struct esp_video_stream *stream, struct esp_video_buffer_element *element);

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
esp_err_t esp_video_done_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element);

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
esp_err_t esp_video_done_buffer(struct esp_video *video, uint32_t type, uint8_t *buffer, uint32_t n);

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
struct esp_video_buffer_element *esp_video_recv_element(struct esp_video *video, uint32_t type, uint32_t ticks);

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
esp_err_t esp_video_queue_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element);

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
esp_err_t esp_video_queue_element_index(struct esp_video *video, uint32_t type, int index);

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
esp_err_t esp_video_queue_element_index_buffer(struct esp_video *video, uint32_t type, int index, uint8_t *buffer, uint32_t size);

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
uint8_t *esp_video_get_element_index_payload(struct esp_video *video, uint32_t type, int index);

/**
 * @brief Get video object by name
 *
 * @param name The video object name
 *
 * @return Video object pointer if found by name
 */
struct esp_video *esp_video_device_get_object(const char *name);

/**
 * @brief Get video stream object pointer by stream type.
 *
 * @param video  Video object
 * @param type   Video stream type
 *
 * @return Video stream object pointer
 */
struct esp_video_stream *esp_video_get_stream(struct esp_video *video, enum v4l2_buf_type type);

/**
 * @brief Get video buffer type.
 *
 * @param video  Video object
 *
 * @return the type left shift bits
 */
uint32_t esp_video_get_buffer_type_bits(struct esp_video *video);

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
esp_err_t esp_video_set_stream_buffer(struct esp_video *video, enum v4l2_buf_type type, struct esp_video_buffer *buffer);

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
esp_err_t esp_video_set_priv_data(struct esp_video *video, void *priv);

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
                                       struct esp_video_buffer_element *dst_element);

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
                                      struct esp_video_buffer_element *dst_element);

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
        struct esp_video_buffer_element **dst_element);

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
struct esp_video_buffer_element *esp_video_clone_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element);

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
esp_err_t esp_video_get_buf_type(struct esp_video *video, uint32_t *type, bool is_input);

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
esp_err_t esp_video_set_ext_controls(struct esp_video *video, const struct v4l2_ext_controls *ctrls);

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
esp_err_t esp_video_get_ext_controls(struct esp_video *video, struct v4l2_ext_controls *ctrls);

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
esp_err_t esp_video_query_ext_control(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl);

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
esp_err_t esp_video_m2m_process(struct esp_video *video, uint32_t src_type, uint32_t dst_type, esp_video_m2m_process_t proc);

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
esp_err_t esp_video_set_sensor_format(struct esp_video *video, const esp_cam_sensor_format_t *format);

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
esp_err_t esp_video_get_sensor_format(struct esp_video *video, esp_cam_sensor_format_t *format);

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
esp_err_t esp_video_query_menu(struct esp_video *video, struct v4l2_querymenu *qmenu);

#ifdef __cplusplus
}
#endif
