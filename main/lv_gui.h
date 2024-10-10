#pragma once

#ifdef __cplusplus
extern "C" {
#endif
extern uint8_t biaoqing ;

void lv_main_page(void);
void lv_gui_start(void);
void label_ask_set_text(char *text);
void label_reply_set_text(char *text);
void sr_anim_start(void);
uint16_t Get_Set_Random(uint8_t set)
;
void sr_anim_stop(void);
#ifdef __cplusplus
}
#endif
