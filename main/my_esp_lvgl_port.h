/*
 * @Author: Kevincoooool 33611679+Kevincoooool@users.noreply.github.com
 * @Date: 2024-11-02 12:44:24
 * @LastEditors: Kevincoooool 33611679+Kevincoooool@users.noreply.github.com
 * @LastEditTime: 2024-11-02 13:18:06
 * @FilePath: \xiaozhi-esp32\main\my_esp_lvgl_port.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef _my_esp_lvgl_port_
#define _my_esp_lvgl_port_

#ifdef __cplusplus
extern "C"
{
#endif
void esp_lvgl_adapter_init(void *arg);
bool esp_lvgl_lock(int timeout_ms);


void esp_lvgl_unlock(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif 
