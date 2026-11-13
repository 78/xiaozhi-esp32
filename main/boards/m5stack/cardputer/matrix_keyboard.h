#ifndef MATRIX_KEYBOARD_H
#define MATRIX_KEYBOARD_H

#include <functional>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "cardputer_keyboard.h"

// Original Cardputer keyboard: 56 keys on a GPIO matrix. Three address
// lines drive a 74HC138 decoder that selects one of 8 scan lines, and 7 input
// lines (active low, pulled up) read the keys on the selected line. Each scan
// line covers half of one logical row, giving the same 4x14 layout as the Adv.
// Scanned by polling; based on the M5Cardputer library's Keyboard reader.
class MatrixKeyboard {
public:
    using KeyCallback = std::function<void(LegacyKeyCode key)>;
    using KeyEventCallback = std::function<void(const KeyEvent& event)>;

    MatrixKeyboard(const gpio_num_t (&address_pins)[3], const gpio_num_t (&input_pins)[7]);
    ~MatrixKeyboard();

    void Initialize();
    void SetKeyCallback(KeyCallback callback) { key_callback_ = callback; }
    void SetKeyEventCallback(KeyEventCallback callback) { key_event_callback_ = callback; }

    uint8_t GetModifierMask() const { return modifier_mask_; }
    bool IsShiftPressed() const { return (modifier_mask_ & KEY_MOD_SHIFT) != 0; }
    bool IsCapsLockOn() const { return caps_lock_on_; }

private:
    gpio_num_t address_pins_[3];
    gpio_num_t input_pins_[7];
    KeyCallback key_callback_;
    KeyEventCallback key_event_callback_;
    TaskHandle_t task_handle_ = nullptr;
    uint8_t modifier_mask_ = 0;
    bool caps_lock_on_ = false;
    uint64_t key_state_mask_ = 0;  // 4x14 logical keys, bit=1 means pressed

    uint64_t Scan();
    void DispatchKey(uint8_t row, uint8_t col, bool pressed);
    LegacyKeyCode MapLegacyKeyCode(uint8_t row, uint8_t col);
    KeyEvent MapKeyEvent(uint8_t row, uint8_t col, bool pressed);
    void UpdateModifierState(uint8_t row, uint8_t col, bool pressed);

    static void KeyboardTask(void* arg);
};

#endif // MATRIX_KEYBOARD_H
