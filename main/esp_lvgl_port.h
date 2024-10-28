#ifndef _esp_lvgl_port_
#define _esp_lvgl_port_

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
