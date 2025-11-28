#include "esp_log.h"
#include <string.h>
#include<driver_gt30l32s4w.h>
#include<driver_gt30l32s4w_basic.h>
#include<driver_gt30l32s4w_interface.h>
#include<utf8_to_gb2312_table.h>
#include <GxEPD2_BW.h>
#include "DrawMixedString.h"

static const char *TAG = "EPD_DEMO";
static gt30l32s4w_handle_t gs_handle;        /**< gt30l32s4w handle */


DisplayClass display(   
    GxEPD2_DRIVER_CLASS(
        /*CS=*/ EPD_PIN_NUM_CS,
        /*DC=*/ EPD_PIN_NUM_DC,
        /*RST=*/ EPD_PIN_NUM_RST,
        /*BUSY=*/ EPD_PIN_NUM_BUSY
    )
);

// ...existing code...


// UTF-8 -> GB2312 表
typedef struct {
    uint8_t utf8[3];
    uint8_t gb[2];
} utf8_gb2312_t;



uint8_t gt30_init()
{
    uint8_t res;
    
    /* link function */
    DRIVER_GT30L32S4W_LINK_INIT(&gs_handle, gt30l32s4w_handle_t);
    DRIVER_GT30L32S4W_LINK_SPI_INIT(&gs_handle, gt30l32s4w_interface_spi_init);
    DRIVER_GT30L32S4W_LINK_SPI_DEINIT(&gs_handle, gt30l32s4w_interface_spi_deinit);
    DRIVER_GT30L32S4W_LINK_SPI_WRITE_READ(&gs_handle, gt30l32s4w_interface_spi_write_read);
    DRIVER_GT30L32S4W_LINK_DELAY_MS(&gs_handle, gt30l32s4w_interface_delay_ms);
    DRIVER_GT30L32S4W_LINK_DEBUG_PRINT(&gs_handle, gt30l32s4w_interface_debug_print);
    
    /* gt30l32s4w init */
    res = gt30l32s4w_init(&gs_handle);
    if (res != 0)
    {
        gt30l32s4w_interface_debug_print("gt30l32s4w: init failed.\n");
        
        return 1;
    }
    
     /* set default mode */
    res = gt30l32s4w_set_mode(&gs_handle, GT30L32S4W_BASIC_DEFAULT_MODE);
    if (res != 0)
    {
        gt30l32s4w_interface_debug_print("gt30l32s4w: set mode failed.\n");
        (void)gt30l32s4w_deinit(&gs_handle);
        
        return 1;
    }
    
    return 0;
}









bool utf8_to_gb2312(const char* utf8Char, uint8_t gb[2]) {
    if (!utf8Char || !gb) return false;
    for (int i = 0; i < 6763; i++) {
        if (strncmp((const char*)utf8_gb2312_table[i].utf8, utf8Char, 3) == 0) {
            gb[0] = utf8_gb2312_table[i].gb[0];
            gb[1] = utf8_gb2312_table[i].gb[1];
            return true;
        }
    }
    ESP_LOGW(TAG, "utf8_to_gb2312 fail for UTF-8: %02X %02X %02X",
             (unsigned char)utf8Char[0],
             (unsigned char)utf8Char[1],
             (unsigned char)utf8Char[2]);
    return false;
}





bool drawChinese(gt30l32s4w_handle_t *handle, uint16_t gbCode, int x, int y)
{
    uint8_t buf[24] = {0};  // 16x16 → 每行2字节 × 16行 = 32字节
    uint8_t ret;
    // 从字库读取中文点阵
    if ((ret = gt30l32s4w_read_char_12x12(handle, gbCode, buf)) != 0)
    { 
        ESP_LOGW(TAG, "Chinese read fail");
        ESP_LOGW(TAG,"Chinese ,ret=%d",ret);
        return false;
      
    }

    // 一次性绘制
    display.drawBitmap(x, y, buf, CHINESE_WIDTH, CHINESE_HEIGHT, GxEPD_BLACK);
    ESP_LOGW(TAG, "Chinese drawbitmap");
    return true;
}




// 读取并显示一个 ASCII 字符
int drawAscii8x16(gt30l32s4w_handle_t *handle,char asciiChar, int x, int y) {
     ESP_LOGW(TAG, "ascii test");
    uint8_t buf[26] = {0};  
    // 读取 ASCII 6x12 点阵
    uint8_t ret;
    if ((ret = gt30l32s4w_read_ascii_8x16(handle, (uint16_t)asciiChar, buf)) != 0)
    {
        ESP_LOGW(TAG, "ascii read fail");
        ESP_LOGW(TAG, "ascii, ret=%d", ret);
        return 0;  // 读取失败
        
    }

    // buf 结构：buf[0] = 字符宽度，buf[1] 开始是点阵数据
   
    const uint8_t *dot_data = &buf[0];

    // 直接使用 GxEPD2 内置的 drawBitmap
    display.drawBitmap(x, y, dot_data, ASCII_WIDTH, ASCII_HEIGHT, GxEPD_BLACK);
    ESP_LOGW(TAG, "ascii drawbitmap");
    return 0;  
}



bool isChineseUTF8(const char *str)
{
    unsigned char c = (unsigned char)str[0];
    return (c >= 0x80);  // 所有多字节 UTF-8，包括中文汉字和中文标点
}


void drawBitmapMixedString(const char* utf8Str, int x, int y)
{
    int cursorX = x;
    int cursorY = y;

    while (*utf8Str) {
        if (isChineseUTF8(utf8Str)) {
            // UTF-8 → GB2312 转换
            uint8_t gb2312[2];
            utf8_to_gb2312(utf8Str, gb2312);  // 你需要提供此函数
             ESP_LOGW(TAG, "Chinese utf8_to_gb2312");
            uint16_t gbCode = (gb2312[0] << 8) | gb2312[1];

            // 显示中文
            drawChinese(&gs_handle, gbCode, cursorX, cursorY);
            cursorX += CHINESE_WIDTH;

            utf8Str += 3; // UTF-8 中文占3字节
        } else {
            // 显示英文
            drawAscii8x16(&gs_handle, *utf8Str, cursorX, cursorY);
            cursorX += ASCII_WIDTH;

            utf8Str += 1;
        }
    }
}

// C-compatible wrapper API so other TUs don't need to include GxEPD2 headers
extern "C" {
    void drawMixedString_init()
    {
        // initialize GT30 and display with sensible defaults
        initArduino();
        SPI.begin(SPI_PIN_NUM_CLK, SPI_NUM_MISO, SPI_PIN_NUM_MOSI);
        pinMode(EPD_PIN_NUM_CS, OUTPUT);
        pinMode(EPD_PIN_NUM_DC, OUTPUT);
        pinMode(EPD_PIN_NUM_RST, OUTPUT);
        pinMode(EPD_PIN_NUM_BUSY, INPUT);
        pinMode(GT30_PIN_NUM_CS, OUTPUT);
        uint8_t rt = gt30_init();
        (void)rt;
        display.init(115200, true, 2, false);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        display.fillScreen(GxEPD_WHITE);
        display.setRotation(0);
    }

    void drawMixedString_fillScreen(int color)
    {
        display.fillScreen(color);
    }

    void drawMixedString_drawText(const char* utf8, int x, int y)
    {
        drawBitmapMixedString(utf8, x, y);
    }

    void drawMixedString_display(bool partial)
    {
        if (partial) display.display(true);
        else display.display(false);
    }

    int drawMixedString_width()
    {
        return display.width();
    }

    int drawMixedString_height()
    {
        return display.height();
    }

    void drawMixedString_drawBitmap(int x, int y, const uint8_t* data, int w, int h, int color)
    {
        display.drawBitmap(x, y, data, w, h, color);
    }
}
