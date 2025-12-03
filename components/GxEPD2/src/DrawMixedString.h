#include <Display_EPD_W21_spi.h>
#include "driver_gt30l32s4w.h"
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_420_GDEY042T81 
#ifndef EPD_CS
#define EPD_CS SS
#endif



// somehow there should be an easier way to do this
#define GxEPD2_BW_IS_GxEPD2_BW true
#define GxEPD2_3C_IS_GxEPD2_3C true
#define GxEPD2_4C_IS_GxEPD2_4C true
#define GxEPD2_7C_IS_GxEPD2_7C true
#define GxEPD2_1248_IS_GxEPD2_1248 true
#define GxEPD2_1248c_IS_GxEPD2_1248c true
#define IS_GxEPD(c, x) (c##x)
#define IS_GxEPD2_BW(x) IS_GxEPD(GxEPD2_BW_IS_, x)
#define IS_GxEPD2_3C(x) IS_GxEPD(GxEPD2_3C_IS_, x)
#define IS_GxEPD2_4C(x) IS_GxEPD(GxEPD2_4C_IS_, x)
#define IS_GxEPD2_7C(x) IS_GxEPD(GxEPD2_7C_IS_, x)
#define IS_GxEPD2_1248(x) IS_GxEPD(GxEPD2_1248_IS_, x)
#define IS_GxEPD2_1248c(x) IS_GxEPD(GxEPD2_1248c_IS_, x)

#include "GxEPD2_selection_check.h"


#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.

#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

// 用typedef 简化类型书写
typedef GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> DisplayClass;

// 只声明，不定义
extern DisplayClass display;








#define CHINESE_WIDTH  12
#define CHINESE_HEIGHT 12
#define ASCII_WIDTH    8
#define ASCII_HEIGHT   16


uint8_t gt30_init();





bool utf8_to_gb2312(const char* utf8Char, uint8_t gb[2]);





bool drawChinese(gt30l32s4w_handle_t *handle, uint16_t gbCode, int x, int y);




// 读取并显示一个 ASCII 字符
int drawAscii8x16(gt30l32s4w_handle_t *handle,char asciiChar, int x, int y);


bool isChineseUTF8(const char *str);



void drawBitmapMixedString(const char* utf8Str, int x, int y);

// optional C wrappers so other translation units can invoke the drawing helpers without
// including GxEPD2 headers directly.
#ifdef __cplusplus
extern "C" {
#endif
void drawMixedString_selectFastFullUpdate(bool enable);
void drawMixedString_init();
void drawMixedString_fillScreen(int color);
void drawMixedString_drawText(const char* utf8, int x, int y);
void drawMixedString_display(bool partial);
void drawMixedString_displayWindow(int x, int y, int w, int h, bool partial);
int drawMixedString_width();
int drawMixedString_height();
void drawMixedString_drawBitmap(int x, int y, const uint8_t* data, int w, int h, int color);
void drawMixedString_setPartialWindow(int x, int y, int w, int h);
void drawMixedString_firstPage();
bool drawMixedString_nextPage();
void drawMixedString_setCursor(int x, int y);
void drawMixedString_print(const char* s);
#ifdef __cplusplus
}
#endif