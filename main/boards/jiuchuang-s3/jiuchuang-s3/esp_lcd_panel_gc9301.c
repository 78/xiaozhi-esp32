/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <stdlib.h>
 #include <sys/cdefs.h>
 #include "sdkconfig.h"
 #include <string.h>
 #if CONFIG_LCD_ENABLE_DEBUG_LOG
 // The local log level must be defined before including esp_log.h
 // Set the maximum log level for this source file
 #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
 #endif
 
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "esp_lcd_panel_interface.h"
 #include "esp_lcd_panel_io.h"
 #include "esp_lcd_panel_vendor.h"
 #include "esp_lcd_panel_ops.h"
 #include "esp_lcd_panel_commands.h"
 #include "driver/gpio.h"
 #include "esp_log.h"
 #include "esp_check.h"
 #include "esp_compiler.h"
 /* GC9309NA LCD controller driver for ESP-IDF
  * SPDX-FileCopyrightText: 2024 Your Name
  * SPDX-License-Identifier: Apache-2.0
  */
 
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "esp_lcd_panel_interface.h"
  #include "esp_lcd_panel_io.h"
  #include "esp_check.h"
  #include "driver/gpio.h"
  
  
  // GC9309NA Command Set
  #define GC9309NA_CMD_SLPIN         0x10
  #define GC9309NA_CMD_SLPOUT        0x11
  #define GC9309NA_CMD_INVOFF        0x20
  #define GC9309NA_CMD_INVON         0x21
  #define GC9309NA_CMD_DISPOFF       0x28
  #define GC9309NA_CMD_DISPON        0x29
  #define GC9309NA_CMD_CASET         0x2A
  #define GC9309NA_CMD_RASET         0x2B
  #define GC9309NA_CMD_RAMWR         0x2C
  #define GC9309NA_CMD_MADCTL        0x36
  #define GC9309NA_CMD_COLMOD        0x3A
  #define GC9309NA_CMD_TEOFF         0x34
  #define GC9309NA_CMD_TEON          0x35
  #define GC9309NA_CMD_WRDISBV       0x51
  #define GC9309NA_CMD_WRCTRLD       0x53
  
  // Manufacturer Commands
  #define GC9309NA_CMD_SETGAMMA1     0xF0
  #define GC9309NA_CMD_SETGAMMA2     0xF1
  #define GC9309NA_CMD_PWRCTRL1      0x67
  #define GC9309NA_CMD_PWRCTRL2      0x68
  #define GC9309NA_CMD_PWRCTRL3      0x66
  #define GC9309NA_CMD_PWRCTRL4      0xCA
  #define GC9309NA_CMD_PWRCTRL5      0xCB
  #define GC9309NA_CMD_DINVCTRL      0xB5
  #define GC9309NA_CMD_REG_ENABLE1   0xFE
  #define GC9309NA_CMD_REG_ENABLE2   0xEF
  
  // 自检模式颜色定义
 
 
  static const char *TAG = "lcd_panel.gc9309na";
  
  typedef struct {
      esp_lcd_panel_t base;
      esp_lcd_panel_io_handle_t io;
      int reset_gpio_num;
      bool reset_level;
      int x_gap;
      int y_gap;
      uint8_t madctl_val;
      uint8_t colmod_val;
      uint16_t te_scanline;
      uint8_t fb_bits_per_pixel;
  } gc9309na_panel_t;
  
  static esp_err_t panel_gc9309na_del(esp_lcd_panel_t *panel);
  static esp_err_t panel_gc9309na_reset(esp_lcd_panel_t *panel);
  static esp_err_t panel_gc9309na_init(esp_lcd_panel_t *panel);
  static esp_err_t panel_gc9309na_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
  static esp_err_t panel_gc9309na_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
  static esp_err_t panel_gc9309na_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
  static esp_err_t panel_gc9309na_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
  static esp_err_t panel_gc9309na_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
  static esp_err_t panel_gc9309na_disp_on_off(esp_lcd_panel_t *panel, bool off);
  static esp_err_t panel_gc9309na_sleep(esp_lcd_panel_t *panel, bool sleep);
  
  
  esp_err_t esp_lcd_new_panel_gc9309na(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
  {
      esp_err_t ret = ESP_OK;
      gc9309na_panel_t *gc9309 = NULL;
  
      ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid arg");
      
      gc9309 = calloc(1, sizeof(gc9309na_panel_t));
      ESP_GOTO_ON_FALSE(gc9309, ESP_ERR_NO_MEM, err, TAG, "no mem");
  

         // Hardware reset GPIO config
      if (panel_dev_config->reset_gpio_num >= 0) {
          gpio_config_t io_conf = {
              .mode = GPIO_MODE_OUTPUT,
              .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
          };
          ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "GPIO config failed");
      }
  
      gc9309->colmod_val = 0x55; // RGB565
     // Initial register values
    
     gc9309->fb_bits_per_pixel = 16;
      gc9309->io = io;
      gc9309->reset_gpio_num = panel_dev_config->reset_gpio_num;
      gc9309->reset_level = panel_dev_config->flags.reset_active_high;
      gc9309->x_gap = 0;
      gc9309->y_gap = 0;
  
      // Function pointers
      gc9309->base.del = panel_gc9309na_del;
      gc9309->base.reset = panel_gc9309na_reset;
      gc9309->base.init = panel_gc9309na_init;
      gc9309->base.draw_bitmap = panel_gc9309na_draw_bitmap;
      gc9309->base.invert_color = panel_gc9309na_invert_color;
      gc9309->base.set_gap = panel_gc9309na_set_gap;
      gc9309->base.mirror = panel_gc9309na_mirror;
      gc9309->base.swap_xy = panel_gc9309na_swap_xy;
      gc9309->base.disp_on_off = panel_gc9309na_disp_on_off;
      gc9309->base.disp_sleep = panel_gc9309na_sleep;
  
      *ret_panel = &(gc9309->base);
      ESP_LOGI(TAG, "New GC9309NA panel @%p", gc9309);
      return ESP_OK;
  
  err:
      if (gc9309) {
          if (panel_dev_config->reset_gpio_num >= 0) {
              gpio_reset_pin(panel_dev_config->reset_gpio_num);
          }
          free(gc9309);
      }
      return ret;
  }
  
  static esp_err_t panel_gc9309na_del(esp_lcd_panel_t *panel)
  {
      gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
  
      if (gc9309->reset_gpio_num >= 0) {
          gpio_reset_pin(gc9309->reset_gpio_num);
      }
      free(gc9309);
      ESP_LOGI(TAG, "Del GC9309NA panel");
      return ESP_OK;
  }
  
  static esp_err_t panel_gc9309na_reset(esp_lcd_panel_t *panel)
  {
      gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
  
      if (gc9309->reset_gpio_num >= 0) {
          // Hardware reset
          gpio_set_level(gc9309->reset_gpio_num, gc9309->reset_level);
          vTaskDelay(pdMS_TO_TICKS(10));
          gpio_set_level(gc9309->reset_gpio_num, !gc9309->reset_level);
          vTaskDelay(pdMS_TO_TICKS(120));
      } else {
          // Software reset
         //  uint8_t unlock_cmd[] = {GC9309NA_CMD_REG_ENABLE1, GC9309NA_CMD_REG_ENABLE2};
         //  ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(gc9309->io, 0xFE, unlock_cmd, 2), 
         //                    TAG, "Unlock failed");
         //  ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(gc9309->io, LCD_CMD_SWRESET, NULL, 0),
         //                    TAG, "SW Reset failed");
          vTaskDelay(pdMS_TO_TICKS(120));
      }
      return ESP_OK;
  }
  static esp_err_t panel_gc9309na_init(esp_lcd_panel_t *panel)
 {
     gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
     esp_lcd_panel_io_handle_t io = gc9309->io;
 
     // Unlock commands
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xFE, NULL, 0), TAG, "Unlock cmd1 failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xEF, NULL, 0), TAG, "Unlock cmd2 failed");
 
     // Sleep out command
     //ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0), TAG, "Sleep out failed");
     //vTaskDelay(pdMS_TO_TICKS(80));
 
     // Timing control commands
     //ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE8, (uint8_t[]){0xA0}, 1), TAG, "Timing control failed");
     //ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE8, (uint8_t[]){0xF0}, 1), TAG, "Timing control failed");
 
     // Display on command
     //ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, NULL, 0), TAG, "Display on failed");
    // vTaskDelay(pdMS_TO_TICKS(10));
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x80, (uint8_t[]){0xC0}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x81, (uint8_t[]){0x01}, 1), TAG, "DINV failed");
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x82, (uint8_t[]){0x07}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x83, (uint8_t[]){0x38}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x88, (uint8_t[]){0x64}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x89, (uint8_t[]){0x86}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x8B, (uint8_t[]){0x3C}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x8D, (uint8_t[]){0x51}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x8E, (uint8_t[]){0x70}, 1), TAG, "DINV failed");
 
     //高低位交换 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB4, (uint8_t[]){0x80}, 1), TAG, "DINV failed");
 
     gc9309->colmod_val = 0x05; // RGB565
     gc9309->madctl_val = 0x48;  // BGR顺序，设置bit3=1（即0x08）
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, GC9309NA_CMD_COLMOD, &gc9309->colmod_val, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, GC9309NA_CMD_MADCTL, &gc9309->madctl_val, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0XBF, (uint8_t[]){0X1F}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x7d, (uint8_t[]){0x45,0x06}, 2), TAG, "DINV failed");
     // Continue from where you left off
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xEE, (uint8_t[]){0x00,0x06}, 2), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0XF4, (uint8_t[]){0x53}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF6, (uint8_t[]){0x17,0x08}, 2), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x70, (uint8_t[]){0x4F,0x4F}, 2), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x71, (uint8_t[]){0x12,0x20}, 2), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x72, (uint8_t[]){0x12,0x20}, 2), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB5, (uint8_t[]){0x50}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xBA, (uint8_t[]){0x00}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xEC, (uint8_t[]){0x71}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x7b, (uint8_t[]){0x00,0x0d}, 2), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x7c, (uint8_t[]){0x0d,0x03}, 2), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0XF5, (uint8_t[]){0x02,0x10,0x12}, 3), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (uint8_t[]){0x0C,0x11,0x0b,0x0a,0x05,0x32,0x44,0x8e,0x9a,0x29,0x2E,0x5f}, 12), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF1, (uint8_t[]){0x0B,0x11,0x0b,0x07,0x07,0x32,0x45,0xBd,0x8D,0x21,0x28,0xAf}, 12), TAG, "DINV failed");
 
     // 240x296 resolution settings
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2a, (uint8_t[]){0x00,0x00,0x00,0xef}, 4), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2b, (uint8_t[]){0x00,0x00,0x01,0x27}, 4), TAG, "DINV failed");
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x66, (uint8_t[]){0x2C}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x67, (uint8_t[]){0x18}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x68, (uint8_t[]){0x3E}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xCA, (uint8_t[]){0x0E}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xe8, (uint8_t[]){0xf0}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xCB, (uint8_t[]){0x06}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB6, (uint8_t[]){0x5C,0x40,0x40}, 3), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xCC, (uint8_t[]){0x33}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xCD, (uint8_t[]){0x33}, 1), TAG, "DINV failed");
 
     // Sleep out command
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0), TAG, "Sleep out failed");
     vTaskDelay(pdMS_TO_TICKS(80));
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xe8, (uint8_t[]){0xA0}, 1), TAG, "DINV failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xe8, (uint8_t[]){0xf0}, 1), TAG, "DINV failed");
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xfe, NULL, 0), TAG, "unlock cmd1 failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xee, NULL, 0), TAG, "unlock cmd2 failed");
 
     // Display on command
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, NULL, 0), TAG, "Display on failed");
     
     // Memory write command
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2c, NULL, 0), TAG, "Memory write failed");
     vTaskDelay(pdMS_TO_TICKS(10));
     return ESP_OK;
 }

  
  static esp_err_t panel_gc9309na_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
  {
      gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
 
 
     esp_lcd_panel_io_handle_t io = gc9309->io;
 
     x_start += gc9309->x_gap;
     x_end += gc9309->x_gap;
     y_start += gc9309->y_gap;
     y_end += gc9309->y_gap;
 
     // define an area of frame memory where MCU can access
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
         (x_start >> 8) & 0xFF,
         x_start & 0xFF,
         ((x_end - 1) >> 8) & 0xFF,
         (x_end - 1) & 0xFF,
     }, 4), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
         (y_start >> 8) & 0xFF,
         y_start & 0xFF,
         ((y_end - 1) >> 8) & 0xFF,
         (y_end - 1) & 0xFF,
     }, 4), TAG, "io tx param failed");
     // transfer frame buffer
     size_t len = (x_end - x_start) * (y_end - y_start) * gc9309->fb_bits_per_pixel / 8;
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "io tx color failed");
 
     return ESP_OK;
  }
  
  static esp_err_t panel_gc9309na_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
  {
      gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
      esp_lcd_panel_io_handle_t io = gc9309->io;
      int command = 0;
      if (invert_color_data) {
          command = LCD_CMD_INVON;
      } else {
          command = LCD_CMD_INVOFF;
      }
      ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG,
                          "io tx param failed");
      return ESP_OK;
  }
  
  static esp_err_t panel_gc9309na_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
  {
    //   gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
    //   esp_lcd_panel_io_handle_t io = gc9309->io;
    //   if (mirror_x) {
    //      gc9309->madctl_val |= LCD_CMD_MX_BIT;
    //  } else {
    //      gc9309->madctl_val &= ~LCD_CMD_MX_BIT;
    //  }
    //  if (mirror_y) {
    //      gc9309->madctl_val |= LCD_CMD_MY_BIT;
    //  } else {
    //      gc9309->madctl_val &= ~LCD_CMD_MY_BIT;
    //  }
    //  ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
    //      gc9309->madctl_val
    //  }, 1), TAG, "io tx param failed");
     return ESP_OK;
  }
  
  static esp_err_t panel_gc9309na_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
  {
    //   gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
    //   esp_lcd_panel_io_handle_t io = gc9309->io;
    //   if (swap_axes) {
    //      gc9309->madctl_val |= LCD_CMD_MV_BIT;
    //   } else {
    //      gc9309->madctl_val &= ~LCD_CMD_MV_BIT;
    //   }
    //   ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
    //      gc9309->madctl_val
    //   }, 1), TAG, "io tx param failed");
      return ESP_OK;
  }
  
  static esp_err_t panel_gc9309na_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
  {
      gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
      gc9309->x_gap = x_gap;
      gc9309->y_gap = y_gap;
      return ESP_OK;
  }
  
  static esp_err_t panel_gc9309na_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
  {
      gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
      uint8_t cmd = on_off ? GC9309NA_CMD_DISPON : GC9309NA_CMD_DISPOFF;
      return esp_lcd_panel_io_tx_param(gc9309->io, cmd, NULL, 0);
  }
  
  static esp_err_t panel_gc9309na_sleep(esp_lcd_panel_t *panel, bool sleep)
  {
      gc9309na_panel_t *gc9309 = __containerof(panel, gc9309na_panel_t, base);
      uint8_t cmd = sleep ? GC9309NA_CMD_SLPIN : GC9309NA_CMD_SLPOUT;
      esp_err_t ret = esp_lcd_panel_io_tx_param(gc9309->io, cmd, NULL, 0);
      vTaskDelay(pdMS_TO_TICKS(120));
      return ret;
  }