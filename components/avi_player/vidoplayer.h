/*
 * @Author: Kevincoooool
 * @Date: 2021-12-09 16:02:02
 * @Description: 
 * @version:  
 * @Filename: Do not Edit
 * @LastEditTime: 2021-12-09 16:14:36
 * @FilePath: \14.mjpeg_player\components\avi_player\vidoplayer.h
 */
#ifndef __VIDOPLAYER_H
#define __VIDOPLAYER_H

#ifdef __cplusplus 
extern "C" {
#endif
#define T_vids _REV(0x30306463)
#define T_auds _REV(0x30317762)
void avi_play(const char *filename);
uint32_t _REV(uint32_t value);
uint32_t read_frame(FILE *file, uint8_t *buffer, uint32_t length, uint32_t *fourcc);
#ifdef __cplusplus 
}
#endif

#endif

