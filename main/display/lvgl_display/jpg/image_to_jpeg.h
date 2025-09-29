// image_to_jpeg.h - 图像到JPEG转换的高效编码接口
// 节省约8KB SRAM的JPEG编码实现

#ifndef IMAGE_TO_JPEG_H
#define IMAGE_TO_JPEG_H

#include <stdint.h>
#include <stddef.h>
#include <esp_camera.h>  // 包含ESP32相机驱动的定义，避免重复定义pixformat_t和camera_fb_t

#ifdef __cplusplus
extern "C" {
#endif

// JPEG输出回调函数类型
// arg: 用户自定义参数, index: 当前数据索引, data: JPEG数据块, len: 数据块长度
// 返回: 实际处理的字节数
typedef size_t (*jpg_out_cb)(void *arg, size_t index, const void *data, size_t len);

/**
 * @brief 将图像格式高效转换为JPEG
 * 
 * 这个函数使用优化的JPEG编码器进行编码，主要特点：
 * - 节省约8KB的SRAM使用（静态变量改为堆分配）
 * - 支持多种图像格式输入
 * - 高质量JPEG输出
 * 
 * @param src       源图像数据
 * @param src_len   源图像数据长度
 * @param width     图像宽度
 * @param height    图像高度  
 * @param format    图像格式 (PIXFORMAT_RGB565, PIXFORMAT_RGB888, 等)
 * @param quality   JPEG质量 (1-100)
 * @param out       输出JPEG数据指针 (需要调用者释放)
 * @param out_len   输出JPEG数据长度
 * 
 * @return true 成功, false 失败
 */
bool image_to_jpeg(uint8_t *src, size_t src_len, uint16_t width, uint16_t height, 
                   pixformat_t format, uint8_t quality, uint8_t **out, size_t *out_len);

/**
 * @brief 将图像格式转换为JPEG（回调版本）
 * 
 * 使用回调函数处理JPEG输出数据，适合流式传输或分块处理：
 * - 节省约8KB的SRAM使用（静态变量改为堆分配）
 * - 支持流式输出，无需预分配大缓冲区
 * - 通过回调函数逐块处理JPEG数据
 * 
 * @param src       源图像数据
 * @param src_len   源图像数据长度
 * @param width     图像宽度
 * @param height    图像高度
 * @param format    图像格式
 * @param quality   JPEG质量 (1-100)
 * @param cb        输出回调函数
 * @param arg       传递给回调函数的用户参数
 * 
 * @return true 成功, false 失败
 */
bool image_to_jpeg_cb(uint8_t *src, size_t src_len, uint16_t width, uint16_t height, 
                      pixformat_t format, uint8_t quality, jpg_out_cb cb, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* IMAGE_TO_JPEG_H */
