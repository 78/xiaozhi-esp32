#ifndef TCA8418_KEYBOARD_H
#define TCA8418_KEYBOARD_H

#include "i2c_device.h"
#include "cardputer_keyboard.h"
#include <functional>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// TCA8418 Register definitions
#define TCA8418_REG_CFG             0x01
#define TCA8418_REG_INT_STAT        0x02
#define TCA8418_REG_KEY_LCK_EC      0x03
#define TCA8418_REG_KEY_EVENT_A     0x04
#define TCA8418_REG_KP_GPIO_1       0x1D
#define TCA8418_REG_KP_GPIO_2       0x1E
#define TCA8418_REG_KP_GPIO_3       0x1F

// Config register bits
#define TCA8418_CFG_KE_IEN          0x01  // Key events interrupt enable

class Tca8418Keyboard : public I2cDevice {
public:
    using KeyCallback = std::function<void(LegacyKeyCode key)>;
    using KeyEventCallback = std::function<void(const KeyEvent& event)>;

    Tca8418Keyboard(i2c_master_bus_handle_t i2c_bus, uint8_t addr, gpio_num_t int_pin);
    ~Tca8418Keyboard();

    void Initialize();
    void SetKeyCallback(KeyCallback callback) { key_callback_ = callback; }
    void SetKeyEventCallback(KeyEventCallback callback) { key_event_callback_ = callback; }

    // Get current modifier state
    uint8_t GetModifierMask() const { return modifier_mask_; }
    bool IsShiftPressed() const { return (modifier_mask_ & KEY_MOD_SHIFT) != 0; }
    bool IsCapsLockOn() const { return caps_lock_on_; }

private:
    gpio_num_t int_pin_;
    KeyCallback key_callback_;
    KeyEventCallback key_event_callback_;
    TaskHandle_t task_handle_ = nullptr;
    volatile bool isr_flag_ = false;
    uint8_t modifier_mask_ = 0;
    bool caps_lock_on_ = false;
    uint64_t key_state_mask_ = 0;  // 4x14 logical keys, bit=1 means pressed

    void ConfigureMatrix();
    void EnableInterrupts();
    void FlushEvents();
    uint8_t GetEvent();
    LegacyKeyCode MapLegacyKeyCode(uint8_t row, uint8_t col);
    KeyEvent MapKeyEvent(uint8_t row, uint8_t col, bool pressed);
    void UpdateModifierState(uint8_t row, uint8_t col, bool pressed);

    static void IRAM_ATTR GpioIsrHandler(void* arg);
    static void KeyboardTask(void* arg);
};

#endif // TCA8418_KEYBOARD_H
