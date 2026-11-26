#pragma once

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_check.h>
#include <esp_lcd_touch_cst816s.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

inline esp_err_t AddAmoled206Touch(i2c_master_bus_handle_t bus, gpio_num_t reset,
                                   gpio_num_t interrupt) {
    constexpr char tag[] = "AMOLED-2.06";
    auto* display = lv_display_get_default();
    ESP_RETURN_ON_FALSE(display, ESP_ERR_INVALID_STATE, tag, "Display is not initialized");

    // A sleeping CST820B may not acknowledge its address until reset.
    ESP_RETURN_ON_ERROR(gpio_set_direction(reset, GPIO_MODE_OUTPUT), tag, "Configure touch reset");
    ESP_RETURN_ON_ERROR(gpio_set_level(reset, 0), tag, "Assert touch reset");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(reset, 1), tag, "Release touch reset");
    vTaskDelay(pdMS_TO_TICKS(120));

    const struct {
        uint8_t address;
        const char* name;
        decltype(&esp_lcd_touch_new_i2c_ft5x06) create;
    } controllers[] = {
        {ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS, "CST820B", esp_lcd_touch_new_i2c_cst816s},
        {ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS, "FT6146", esp_lcd_touch_new_i2c_ft5x06},
    };
    for (const auto& controller : controllers) {
        esp_err_t err = i2c_master_probe(bus, controller.address, 100);
        if (err == ESP_ERR_NOT_FOUND) {
            continue;
        }
        ESP_RETURN_ON_ERROR(err, tag, "Probe touch at 0x%02x", controller.address);
        ESP_LOGI(tag, "Detected %s at 0x%02x", controller.name, controller.address);

        const esp_lcd_panel_io_i2c_config_t io_cfg = {
            .dev_addr = controller.address,
            .scl_speed_hz = 400000,
            .control_phase_bytes = 1,
            .lcd_cmd_bits = 8,
            .flags = {.disable_control_phase = 1},
        };
        esp_lcd_panel_io_handle_t io = nullptr;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io), tag, "Create touch IO");

        const esp_lcd_touch_config_t touch_cfg = {
            .x_max = static_cast<uint16_t>(lv_display_get_horizontal_resolution(display) - 1),
            .y_max = static_cast<uint16_t>(lv_display_get_vertical_resolution(display) - 1),
            .rst_gpio_num = GPIO_NUM_NC,  // Already reset before probing.
            .int_gpio_num = interrupt,
        };
        esp_lcd_touch_handle_t touch = nullptr;
        err = controller.create(io, &touch_cfg, &touch);
        if (err != ESP_OK) {
            esp_lcd_panel_io_del(io);
            return err;
        }
        const lvgl_port_touch_cfg_t input_cfg = {.disp = display, .handle = touch};
        if (lvgl_port_add_touch(&input_cfg) == nullptr) {
            esp_lcd_touch_del(touch);
            esp_lcd_panel_io_del(io);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    ESP_LOGW(tag, "No FT6146 or CST820B touch controller found");
    return ESP_ERR_NOT_FOUND;
}
