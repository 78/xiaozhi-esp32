#include "matrix_keyboard.h"
#include <esp_log.h>
#include <esp_rom_sys.h>

#define TAG "MatrixKeyboard"

#define KEYBOARD_SCAN_INTERVAL_MS 10

// Key value structure for mapping
struct KeyValue {
    const char* normal;      // Normal character
    uint8_t normal_code;     // Normal key code
    const char* shifted;     // Shifted character
    uint8_t shifted_code;    // Shifted key code (same as normal for letters)
};

// 4x14 keyboard matrix mapping (based on M5Cardputer-UserDemo)
// Row 0: `   1   2   3   4   5   6   7   8   9   0   -   =   Del
// Row 1: Tab Q   W   E   R   T   Y   U   I   O   P   [   ]   Backslash
// Row 2: Shift CapsLk A  S   D   F   G   H   J   K   L   ;   '   Enter
// Row 3: Ctrl Opt Alt Z   X   C   V   B   N   M   ,   .   /   Space
static const KeyValue KEY_MAP[4][14] = {
    // Row 0
    {{"`", KC_GRAVE, "~", KC_GRAVE},
     {"1", KC_1, "!", KC_1},
     {"2", KC_2, "@", KC_2},
     {"3", KC_3, "#", KC_3},
     {"4", KC_4, "$", KC_4},
     {"5", KC_5, "%", KC_5},
     {"6", KC_6, "^", KC_6},
     {"7", KC_7, "&", KC_7},
     {"8", KC_8, "*", KC_8},
     {"9", KC_9, "(", KC_9},
     {"0", KC_0, ")", KC_0},
     {"-", KC_MINUS, "_", KC_MINUS},
     {"=", KC_EQUAL, "+", KC_EQUAL},
     {"", KC_BACKSPACE, "", KC_BACKSPACE}},  // Del/Backspace
    // Row 1
    {{"", KC_TAB, "", KC_TAB},  // Tab
     {"q", KC_Q, "Q", KC_Q},
     {"w", KC_W, "W", KC_W},
     {"e", KC_E, "E", KC_E},
     {"r", KC_R, "R", KC_R},
     {"t", KC_T, "T", KC_T},
     {"y", KC_Y, "Y", KC_Y},
     {"u", KC_U, "U", KC_U},
     {"i", KC_I, "I", KC_I},
     {"o", KC_O, "O", KC_O},
     {"p", KC_P, "P", KC_P},
     {"[", KC_LBRACKET, "{", KC_LBRACKET},
     {"]", KC_RBRACKET, "}", KC_RBRACKET},
     {"\\", KC_BACKSLASH, "|", KC_BACKSLASH}},
    // Row 2
    {{"", KC_LSHIFT, "", KC_LSHIFT},  // Shift
     {"", KC_CAPSLOCK, "", KC_CAPSLOCK},  // CapsLock
     {"a", KC_A, "A", KC_A},
     {"s", KC_S, "S", KC_S},
     {"d", KC_D, "D", KC_D},
     {"f", KC_F, "F", KC_F},
     {"g", KC_G, "G", KC_G},
     {"h", KC_H, "H", KC_H},
     {"j", KC_J, "J", KC_J},
     {"k", KC_K, "K", KC_K},
     {"l", KC_L, "L", KC_L},
     {";", KC_SEMICOLON, ":", KC_SEMICOLON},
     {"'", KC_APOSTROPHE, "\"", KC_APOSTROPHE},
     {"", KC_ENTER, "", KC_ENTER}},  // Enter
    // Row 3
    {{"", KC_LCTRL, "", KC_LCTRL},  // Ctrl
     {"", KC_LOPT, "", KC_LOPT},  // Opt
     {"", KC_LALT, "", KC_LALT},  // Alt
     {"z", KC_Z, "Z", KC_Z},
     {"x", KC_X, "X", KC_X},
     {"c", KC_C, "C", KC_C},
     {"v", KC_V, "V", KC_V},
     {"b", KC_B, "B", KC_B},
     {"n", KC_N, "N", KC_N},
     {"m", KC_M, "M", KC_M},
     {",", KC_COMMA, "<", KC_COMMA},
     {".", KC_DOT, ">", KC_DOT},
     {"/", KC_SLASH, "?", KC_SLASH},
     {" ", KC_SPACE, " ", KC_SPACE}}
};

static inline uint64_t LogicalKeyMask(uint8_t row, uint8_t col) {
    return 1ULL << ((row * 14) + col);
}

MatrixKeyboard::MatrixKeyboard(const gpio_num_t (&address_pins)[3], const gpio_num_t (&input_pins)[7]) {
    for (int i = 0; i < 3; i++) {
        address_pins_[i] = address_pins[i];
    }
    for (int i = 0; i < 7; i++) {
        input_pins_[i] = input_pins[i];
    }
}

MatrixKeyboard::~MatrixKeyboard() {
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
}

void MatrixKeyboard::Initialize() {
    ESP_LOGI(TAG, "Initializing GPIO matrix keyboard");

    gpio_config_t out_conf = {};
    out_conf.mode = GPIO_MODE_OUTPUT;
    out_conf.intr_type = GPIO_INTR_DISABLE;
    for (auto pin : address_pins_) {
        out_conf.pin_bit_mask |= (1ULL << pin);
    }
    ESP_ERROR_CHECK(gpio_config(&out_conf));

    gpio_config_t in_conf = {};
    in_conf.mode = GPIO_MODE_INPUT;
    in_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    in_conf.intr_type = GPIO_INTR_DISABLE;
    for (auto pin : input_pins_) {
        in_conf.pin_bit_mask |= (1ULL << pin);
    }
    ESP_ERROR_CHECK(gpio_config(&in_conf));

    xTaskCreate(KeyboardTask, "keyboard_task", 4096, this, 5, &task_handle_);
}

uint64_t MatrixKeyboard::Scan() {
    uint64_t state = 0;
    for (int line = 0; line < 8; line++) {
        gpio_set_level(address_pins_[0], line & 0x01);
        gpio_set_level(address_pins_[1], (line >> 1) & 0x01);
        gpio_set_level(address_pins_[2], (line >> 2) & 0x01);
        esp_rom_delay_us(5);

        for (int j = 0; j < 7; j++) {
            if (gpio_get_level(input_pins_[j]) != 0) {
                continue;
            }
            // Scan lines 4-7 hold the even columns, 0-3 the odd ones, and the
            // rows run bottom-up (same mapping as M5Cardputer's X_map_chart).
            uint8_t col = (line > 3) ? (j * 2) : (j * 2 + 1);
            uint8_t row = 3 - ((line > 3) ? (line - 4) : line);
            state |= LogicalKeyMask(row, col);
        }
    }
    return state;
}

void MatrixKeyboard::UpdateModifierState(uint8_t row, uint8_t col, bool pressed) {
    // Shift key: row 2, col 0
    if (row == 2 && col == 0) {
        if (pressed) {
            modifier_mask_ |= KEY_MOD_SHIFT;
        } else {
            modifier_mask_ &= ~KEY_MOD_SHIFT;
        }
    }
    // Ctrl key: row 3, col 0
    else if (row == 3 && col == 0) {
        if (pressed) {
            modifier_mask_ |= KEY_MOD_CTRL;
        } else {
            modifier_mask_ &= ~KEY_MOD_CTRL;
        }
    }
    // Alt key: row 3, col 2
    else if (row == 3 && col == 2) {
        if (pressed) {
            modifier_mask_ |= KEY_MOD_ALT;
        } else {
            modifier_mask_ &= ~KEY_MOD_ALT;
        }
    }
    // Opt key: row 3, col 1
    else if (row == 3 && col == 1) {
        if (pressed) {
            modifier_mask_ |= KEY_MOD_OPT;
        } else {
            modifier_mask_ &= ~KEY_MOD_OPT;
        }
    }
    // CapsLock key: row 2, col 1 (toggle on press)
    else if (row == 2 && col == 1 && pressed) {
        caps_lock_on_ = !caps_lock_on_;
        ESP_LOGD(TAG, "CapsLock toggled: %s", caps_lock_on_ ? "ON" : "OFF");
    }
}

LegacyKeyCode MatrixKeyboard::MapLegacyKeyCode(uint8_t row, uint8_t col) {
    // Arrow keys mapping based on M5Cardputer layout:
    // UP: ; key - row 2, col 11
    // DOWN: . key - row 3, col 11
    // LEFT: , key - row 3, col 10
    // RIGHT: / key - row 3, col 12
    // ENTER: enter key - row 2, col 13

    if (row == 2 && col == 11) return KEY_UP;      // ; key
    if (row == 3 && col == 11) return KEY_DOWN;    // . key
    if (row == 3 && col == 10) return KEY_LEFT;    // , key
    if (row == 3 && col == 12) return KEY_RIGHT;   // / key
    if (row == 2 && col == 13) return KEY_ENTER;   // Enter key

    return KEY_OTHER;
}

KeyEvent MatrixKeyboard::MapKeyEvent(uint8_t row, uint8_t col, bool pressed) {
    KeyEvent event;
    event.pressed = pressed;
    event.is_modifier = false;
    event.key_code = KC_NONE;
    event.key_char = "";

    if (row >= 4 || col >= 14) {
        return event;
    }

    const KeyValue& kv = KEY_MAP[row][col];
    event.key_code = kv.normal_code;

    // Check if this is a modifier key
    if (event.key_code == KC_LSHIFT || event.key_code == KC_LCTRL ||
        event.key_code == KC_LALT || event.key_code == KC_LOPT ||
        event.key_code == KC_CAPSLOCK) {
        event.is_modifier = true;
        event.key_char = "";
        return event;
    }

    // Determine if we should use shifted version
    bool use_shifted = false;

    // Check if this is a letter key (a-z)
    bool is_letter = (event.key_code >= KC_A && event.key_code <= KC_Z);

    if (is_letter) {
        // For letters, use XOR of shift and caps lock (Shift reverses CapsLock)
        bool shift_pressed = (modifier_mask_ & KEY_MOD_SHIFT) != 0;
        use_shifted = shift_pressed != caps_lock_on_;  // XOR semantics
    } else {
        // For non-letters (numbers, symbols), only use shift
        use_shifted = (modifier_mask_ & KEY_MOD_SHIFT) != 0;
    }

    if (use_shifted) {
        event.key_char = kv.shifted;
    } else {
        event.key_char = kv.normal;
    }

    return event;
}

void MatrixKeyboard::DispatchKey(uint8_t row, uint8_t col, bool pressed) {
    ESP_LOGD(TAG, "Key %s: (%d,%d)", pressed ? "pressed" : "released", row, col);

    UpdateModifierState(row, col, pressed);

    if (key_event_callback_) {
        KeyEvent key_event = MapKeyEvent(row, col, pressed);
        key_event_callback_(key_event);
    }

    if (pressed && key_callback_) {
        LegacyKeyCode mapped_key = MapLegacyKeyCode(row, col);
        if (mapped_key != KEY_OTHER && mapped_key != KEY_NONE) {
            key_callback_(mapped_key);
        }
    }
}

void MatrixKeyboard::KeyboardTask(void* arg) {
    MatrixKeyboard* keyboard = static_cast<MatrixKeyboard*>(arg);
    uint64_t last_raw = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(KEYBOARD_SCAN_INTERVAL_MS));

        // Debounce: only accept a state seen on two consecutive scans.
        uint64_t raw = keyboard->Scan();
        if (raw != last_raw) {
            last_raw = raw;
            continue;
        }

        uint64_t changed = raw ^ keyboard->key_state_mask_;
        if (changed == 0) {
            continue;
        }
        keyboard->key_state_mask_ = raw;

        for (uint8_t row = 0; row < 4; row++) {
            for (uint8_t col = 0; col < 14; col++) {
                uint64_t mask = LogicalKeyMask(row, col);
                if (changed & mask) {
                    keyboard->DispatchKey(row, col, (raw & mask) != 0);
                }
            }
        }
    }
}
