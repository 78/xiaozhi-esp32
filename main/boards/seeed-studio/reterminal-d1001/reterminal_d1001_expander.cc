#include "reterminal_d1001_expander.h"

#include "config.h"

#include <esp_io_expander_tca95xx_16bit.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "D1001Expander"

ReTerminalD1001Expander::~ReTerminalD1001Expander() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (expander_ != nullptr) {
        esp_io_expander_del(expander_);
        expander_ = nullptr;
    }
    if (i2c_bus_ != nullptr) {
        i2c_del_master_bus(i2c_bus_);
        i2c_bus_ = nullptr;
    }
}

void ReTerminalD1001Expander::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    i2c_master_bus_config_t bus_config = {
        .i2c_port = AUDIO_I2C_PORT,
        .sda_io_num = AUDIO_I2C_SDA_PIN,
        .scl_io_num = AUDIO_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C1 master bus: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_io_expander_new_i2c_tca95xx_16bit(i2c_bus_, PCA9535_I2C_ADDRESS, &expander_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCA9535 expander at 0x%02x: %s", PCA9535_I2C_ADDRESS,
                 esp_err_to_name(ret));
        return;
    }

    // All pins used by the port are outputs on this board. The EXPANDER_BIT_*
    // macros are already bit masks, so they can be OR-ed directly.
    const uint32_t all_pins =
        static_cast<uint32_t>(EXPANDER_BIT_LCD_PWR_EN | EXPANDER_BIT_LCD_BACKLIGHT_EN |
                              EXPANDER_BIT_PWR_HOLD | EXPANDER_BIT_LCD_RST |
                              EXPANDER_BIT_PA_ENABLE | EXPANDER_BIT_TOUCH_RST |
                              EXPANDER_BIT_CAM_EN | EXPANDER_BIT_CAM_PWDN |
                              EXPANDER_BIT_CAM_RST);
    // The BSP drives every expander pin as an output (bsp_power_init:
    // set_dir 0xffff); follow it so the power-hold write cannot be lost to a
    // pin that was left as an input.
    ret = esp_io_expander_set_dir(expander_, 0xffff, IO_EXPANDER_OUTPUT);
    (void)all_pins;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure expander outputs: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "PCA9535 expander initialized");
}

void ReTerminalD1001Expander::SetLevel(uint32_t pin_mask, bool level) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (expander_ == nullptr) {
        ESP_LOGW(TAG, "expander not initialized; ignoring set_level(0x%x, %d)", (unsigned)pin_mask,
                 level);
        return;
    }
    esp_err_t ret = esp_io_expander_set_level(expander_, pin_mask, level);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "expander set_level(0x%x, %d) failed: %s", (unsigned)pin_mask, level,
                 esp_err_to_name(ret));
    }
}

void ReTerminalD1001Expander::SetPowerHold(bool on) { SetLevel(EXPANDER_BIT_PWR_HOLD, on); }

void ReTerminalD1001Expander::SetLcdPower(bool on) { SetLevel(EXPANDER_BIT_LCD_PWR_EN, on); }

void ReTerminalD1001Expander::SetLcdReset(bool asserted) {
    // Reset is active low: assert pulls the line down.
    SetLevel(EXPANDER_BIT_LCD_RST, !asserted);
}

void ReTerminalD1001Expander::SetBacklightPower(bool on) {
    SetLevel(EXPANDER_BIT_LCD_BACKLIGHT_EN, on);
}

void ReTerminalD1001Expander::SetTouchReset(bool asserted) {
    // Reset is active low: assert pulls the line down.
    SetLevel(EXPANDER_BIT_TOUCH_RST, !asserted);
}

void ReTerminalD1001Expander::SetPowerAmp(bool on) { SetLevel(EXPANDER_BIT_PA_ENABLE, on); }

void ReTerminalD1001Expander::SetBatteryReadEnable(bool on) {
    SetLevel(EXPANDER_BIT_BAT_READ_EN, on);
}

void ReTerminalD1001Expander::SetBatteryChargeEnable(bool on) {
    SetLevel(EXPANDER_BIT_BAT_CHG_EN, !on);  // active low
}

void ReTerminalD1001Expander::SetCameraPower(bool on) { SetLevel(EXPANDER_BIT_CAM_EN, on); }

void ReTerminalD1001Expander::SetCameraPowerDown(bool asserted) {
    // The BSP drives this line high for normal operation, so asserting
    // power-down pulls it low.
    SetLevel(EXPANDER_BIT_CAM_PWDN, !asserted);
}

void ReTerminalD1001Expander::SetCameraReset(bool asserted) {
    // Reset is active low: the BSP releases it by driving the line high.
    SetLevel(EXPANDER_BIT_CAM_RST, !asserted);
}

void ReTerminalD1001Expander::PowerUpCamera() {
    if (!IsInitialized()) {
        ESP_LOGE(TAG, "cannot power up the camera: expander unavailable");
        return;
    }
    // Same sequence and delays as bsp_io_expander_init() in the Seeed BSP.
    SetCameraPower(true);
    vTaskDelay(pdMS_TO_TICKS(50));
    SetCameraPowerDown(false);
    SetCameraReset(false);
    vTaskDelay(pdMS_TO_TICKS(10));
    SetCameraReset(true);
    vTaskDelay(pdMS_TO_TICKS(10));
    SetCameraReset(false);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "camera powered up");
}

void ReTerminalD1001Expander::PowerDownCamera() {
    if (!IsInitialized()) {
        return;
    }
    SetCameraReset(true);
    SetCameraPowerDown(true);
    SetCameraPower(false);
}

void ReTerminalD1001Expander::ApplyMinimalPowerSequence() {
    if (!IsInitialized()) {
        ESP_LOGE(TAG, "cannot apply power sequence: expander unavailable");
        return;
    }
    // Keep the power amplifier off until the audio codec has muted DMA data
    // ready (pop-free power-up order).
    SetPowerAmp(false);
    // Park the panel signals: rails off, both controllers held in reset. The
    // display bring-up path turns them on in the BSP order.
    SetLcdPower(false);
    SetBacklightPower(false);
    SetLcdReset(true);
    SetTouchReset(true);
    // Park the camera too: rail off and the sensor held in reset until
    // PowerUpCamera() runs the BSP sequence.
    SetCameraPower(false);
    SetCameraPowerDown(true);
    SetCameraReset(true);
    // Hold system power (vdd_3v3) and let it settle, as the BSP does.
    SetPowerHold(true);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "minimal power sequence applied");
}

void ReTerminalD1001Expander::PowerUpDisplayRails() {
    if (!IsInitialized()) {
        ESP_LOGE(TAG, "cannot power up the display rails: expander unavailable");
        return;
    }
    SetLcdPower(true);
    SetBacklightPower(true);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void ReTerminalD1001Expander::ResetLcdPanel() {
    if (!IsInitialized()) {
        ESP_LOGE(TAG, "cannot reset the LCD: expander unavailable");
        return;
    }
    // Same pulse as the Seeed BSP: released 5 ms, asserted 10 ms, released
    // 120 ms before the panel accepts initialisation commands.
    SetLcdReset(false);
    vTaskDelay(pdMS_TO_TICKS(5));
    SetLcdReset(true);
    vTaskDelay(pdMS_TO_TICKS(10));
    SetLcdReset(false);
    vTaskDelay(pdMS_TO_TICKS(120));
}

void ReTerminalD1001Expander::PowerOff() {
    ESP_LOGI(TAG, "powering off the board");
    // Same order as bsp_power_off(): LTE rail first, then the power hold.
    SetLevel(EXPANDER_BIT_LTE_PWR_EN, false);
    vTaskDelay(pdMS_TO_TICKS(500));
    SetPowerHold(false);
    // Evidence for the bench: read the expander back so the log shows whether
    // bit 8 really dropped (output register) and what the pins read (input).
    std::lock_guard<std::mutex> lock(mutex_);
    if (expander_ != nullptr) {
        uint32_t out_reg = 0xffffffff, in_reg = 0xffffffff;
        esp_io_expander_get_level(expander_, 0xffff, &in_reg);
        esp_io_expander_print_state(expander_);
        ESP_LOGW(TAG, "after PowerOff: input levels=0x%04x (bit8=%d)", (unsigned)in_reg,
                 (int)((in_reg >> 8) & 1));
        (void)out_reg;
    }
}
