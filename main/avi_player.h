/*
 * @Author: Kevincoooool
 * @Date: 2021-12-06 16:24:39
 * @Description:
 * @version:
 * @Filename: Do not Edit
 * @LastEditTime: 2021-12-06 16:33:21
 * @FilePath: \6.lcd_camera_lvgl_v7\main\avi_player.h
 */
/***
 * @Descripttion :
 * @version      :
 * @Author       : Kevincoooool
 * @Date         : 2021-06-05 10:13:51
 * @LastEditors  : Kevincoooool
 * @LastEditTime : 2021-07-07 10:39:37
 * @FilePath     : \esp-idf\pro\KSDIY_ESPCAM\main\page\avi_player.h
 */

#ifndef _avi_player_
#define _avi_player_

#ifdef __cplusplus
extern "C"
{
#endif
    /*********************
     * INCLUDES
     *********************/

#include "lvgl.h"
    enum
    {

        FACE_STATIC = 0,
        FACE_HAPPY,
        FACE_ANGRY,
        FACE_BAD,
        FACE_FEAR,
        FACE_NOGOOD,
    };
    enum
    {

        CIRCLE_IN = 0,
        CIRCLE_RUN,
        CIRCLE_OUT,
        CIRCLE_END,

    };
    void Cam_Task(void *pvParameters);
    void avi_player_load(void);
    void avi_player_end(void);
    void Avi_Player_Task(void *arg);
    void play_change(uint8_t state);

        extern uint8_t new_out_play_state;

    extern uint8_t out_play_state;
    extern uint8_t in_play_state;
    extern uint8_t need_change;
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _TEST_
