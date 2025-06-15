# Espressif Video Component

Espressif video component provides a solution to call POSIX API plus Linux V4L2 commands to capture data streams from multi camera sensors, and transform stream data pixel format according to Linux V4L2 M2M codec device.

[![Component Registry](https://components.espressif.com/components/espressif/esp_video/badge.svg)](https://components.espressif.com/components/espressif/esp_video)

Now we have implementations based on:

- esp_cam_sensor
- esp_h264
- esp_ipa

## Video Device

| Hardware | Video Device | Type | Input Format | Output Format |
|:-:|:-:|:-:|:-|:-|
| MIPI-CSI | /dev/video0 | Capture | / | camera output pixel format or ISP output format(1) |
| DVP | /dev/video2 | Capture  | / | camera output pixel format |
| JPEG encode | /dev/video10 | M2M | RGB565: V4L2_PIX_FMT_RGB565<br> RGB888: V4L2_PIX_FMT_RGB24<br> YUV422: V4L2_PIX_FMT_YUV422P<br> Gray8: V4L2_PIX_FMT_GREY | JPEG: V4L2_PIX_FMT_JPEG |
| H.264 encode | /dev/video11 | M2M | YUV420: V4L2_PIX_FMT_YUV420 | H.264: V4L2_PIX_FMT_H264 |
| ISP | /dev/video20 | Meta | camera output pixel format  | Metadata: V4L2_META_FMT_ESP_ISP_STATS |

- (1): if camera output pixel format is RAW8, ISP can transform it to other pixel format: RGB565, RGB888, YUV420 and YUV422

## V4L2 Control IDs

| ID | Class | Type | Permission | Description |
|:-:|:-:|:-:|:-:|:-|
| V4L2_CID_VFLIP | V4L2_CID_USER_CLASS | Bool | Read/Write | Mirror the picture vertically. |
| V4L2_CID_HFLIP | V4L2_CID_USER_CLASS | Bool | Read/Write | Mirror the picture horizontally. |
| V4L2_CID_GAIN | V4L2_CID_USER_CLASS | Menu | Read/Write | Picrure pixel gain value. |
| V4L2_CID_EXPOSURE | V4L2_CID_USER_CLASS | Integer | Read/Write | Camera sensor exposure time, value unit depends on sensor |
| V4L2_CID_EXPOSURE_ABSOLUTE | V4L2_CID_CAMERA_CLASS | Integer | Read/Write | Camera sensor exposure time, value unit is 100us. |
| V4L2_CID_TEST_PATTERN | V4L2_CID_IMAGE_PROC_CLASS | Menu | Write | Camera sensor test pattern mode. |
| V4L2_CID_JPEG_COMPRESSION_QUALITY | V4L2_CID_JPEG_CLASS | Integer | Read/Write | JPEG encoded picture quality |
| V4L2_CID_JPEG_CHROMA_SUBSAMPLING | V4L2_CID_JPEG_CLASS | Menu | Read/Write | The chroma subsampling factors describe how each component of an input image is sampled. |
| V4L2_CID_MPEG_VIDEO_H264_I_PERIOD | V4L2_CID_CODEC_CLASS | Integer | Read/Write | Period between I-frames. |
| V4L2_CID_MPEG_VIDEO_BITRATE | V4L2_CID_CODEC_CLASS | Integer | Read/Write | Video bitrate in bits per second. |
| V4L2_CID_MPEG_VIDEO_H264_MIN_QP | V4L2_CID_CODEC_CLASS | Integer | Read/Write | Minimum quantization parameter for H264. |
| V4L2_CID_MPEG_VIDEO_H264_MAX_QP | V4L2_CID_CODEC_CLASS | Integer | Read/Write | Maximum quantization parameter for H264. |
| V4L2_CID_RED_BALANCE | V4L2_CID_USER_CLASS | Integer | Read/Write | Red chroma balance. |
| V4L2_CID_BLUE_BALANCE | V4L2_CID_USER_CLASS | Integer | Read/Write | Blue chroma balance. |
| V4L2_CID_USER_ESP_ISP_BF | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | ISP bayer filter parameters. |
| V4L2_CID_USER_ESP_ISP_CCM | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | ISP color correction matrix parameters. |
| V4L2_CID_USER_ESP_ISP_SHARPEN | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | ISP sharpen parameters. |
| V4L2_CID_USER_ESP_ISP_GAMMA | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | ISP GAMMA parameters. |
| V4L2_CID_USER_ESP_ISP_DEMOSAIC | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | ISP demosaic parameters. |
| V4L2_CID_BRIGHTNESS | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | Picture brightness. |
| V4L2_CID_CONTRAST | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | Picture contrast. |
| V4L2_CID_SATURATION | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | Picture color saturation. |
| V4L2_CID_HUE | V4L2_CID_USER_CLASS | Array of uint8_t | Read/Write | Picture hue. |
|  V4L2_CID_CAMERA_STATS | V4L2_CID_CAMERA_CLASS | Array of uint8_t | Read | Camera sensor statistics. |
| V4L2_CID_CAMERA_AE_LEVEL | V4L2_CID_CAMERA_CLASS | Integer | Read/Write | Camera sensor AE target level. |
