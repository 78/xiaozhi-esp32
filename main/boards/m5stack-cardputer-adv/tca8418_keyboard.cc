#include "tca8418_keyboard.h"
#include <esp_log.h>

#define TAG "TCA8418"

// TCA8418 additional registers
#define TCA8418_REG_GPIO_INT_EN_1   0x1A
#define TCA8418_REG_GPIO_INT_EN_2   0x1B
#define TCA8418_REG_GPIO_INT_EN_3   0x1C
#define TCA8418_REG_GPIO_DAT_STAT_1 0x14
#define TCA8418_REG_GPIO_DAT_STAT_2 0x15
#define TCA8418_REG_GPIO_DAT_STAT_3 0x16
#define TCA8418_REG_GPIO_DAT_OUT_1  0x17
#define TCA8418_REG_GPIO_DAT_OUT_2  0x18
#define TCA8418_REG_GPIO_DAT_OUT_3  0x19
#define TCA8418_REG_GPIO_INT_LVL_1  0x20
#define TCA8418_REG_GPIO_INT_LVL_2  0x21
#define TCA8418_REG_GPIO_INT_LVL_3  0x22
#define TCA8418_REG_DEBOUNCE_DIS_1  0x29
#define TCA8418_REG_DEBOUNCE_DIS_2  0x2A
#define TCA8418_REG_DEBOUNCE_DIS_3  0x2B
#define TCA8418_REG_GPIO_PULL_1     0x2C
#define TCA8418_REG_GPIO_PULL_2     0x2D
#define TCA8418_REG_GPIO_PULL_3     0x2E

// Config register bits
#define TCA8418_CFG_AI              0x80  // Auto-increment for read/write
#define TCA8418_CFG_GPI_E_CFG       0x40  // GPI event mode config
#define TCA8418_CFG_OVR_FLOW_M      0x20  // Overflow mode
#define TCA8418_CFG_INT_CFG         0x10  // Interrupt config
#define TCA8418_CFG_OVR_FLOW_IEN    0x08  // Overflow interrupt enable
#define TCA8418_CFG_K_LCK_IEN       0x04  // Keypad lock interrupt enable
#define TCA8418_CFG_GPI_IEN         0x02  // GPI interrupt enable

// Interrupt status bits
#define TCA8418_INT_STAT_CAD_INT    0x10  // CTRL-ALT-DEL interrupt
#define TCA8418_INT_STAT_OVR_FLOW   0x08  // Overflow interrupt
#define TCA8418_INT_STAT_K_LCK_INT  0x04  // Key lock interrupt
#define TCA8418_INT_STAT_GPI_INT    0x02  // GPI interrupt
#define TCA8418_INT_STAT_K_INT      0x01  // Key event interrupt

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

// Cardputer Adv uses TCA8418 in a 7x8 matrix, but the physical keyboard layout
// matches Cardputer's 4x14 mapping. Remap raw (row,col) from the 7x8 scan into
// the 4x14 logical layout (based on M5Cardputer-UserDemo CardputerADV branch).
static inline bool RemapRawKeyToLogical(uint8_t& row, uint8_t& col) {
    // Raw scan: row 0..6, col 0..7
    if (row >= 7 || col >= 8) {
        return false;
    }

    // Col: every raw row contributes two logical columns (left/right half)
    uint8_t mapped_col = (row * 2) + ((col > 3) ? 1 : 0);  // 0..13
    // Row: derived from raw col (wrap every 4)
    uint8_t mapped_row = (col + 4) % 4;                    // 0..3

    row = mapped_row;
    col = mapped_col;
    return true;
}

static inline uint64_t LogicalKeyMask(uint8_t row, uint8_t col) {
    // 4x14 = 56 keys, fits in 64-bit
    uint8_t idx = (row * 14) + col;
    if (idx >= 64) {
        return 0;
    }
    return 1ULL << idx;
}

Tca8418Keyboard::Tca8418Keyboard(i2c_master_bus_handle_t i2c_bus, uint8_t addr, gpio_num_t int_pin)
    : I2cDevice(i2c_bus, addr), int_pin_(int_pin) {
}

Tca8418Keyboard::~Tca8418Keyboard() {
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    gpio_isr_handler_remove(int_pin_);
}

void Tca8418Keyboard::Initialize() {
    ESP_LOGI(TAG, "Initializing TCA8418 keyboard");

    // Configure keyboard matrix
    ConfigureMatrix();

    // Flush any pending events
    FlushEvents();

    // Enable interrupts
    EnableInterrupts();

    // Configure GPIO interrupt pin
    gpio_config_t io_conf = {};
    // IRQ is active-low and can stay low while events are pending, so use ANYEDGE.
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << int_pin_);
    // Cardputer Adv board provides external pull-ups; keep internal pulls disabled.
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Install GPIO ISR service if not already installed
    gpio_install_isr_service(0);
    gpio_isr_handler_add(int_pin_, GpioIsrHandler, this);

    // Create keyboard task
    xTaskCreate(KeyboardTask, "keyboard_task", 4096, this, 5, &task_handle_);

    ESP_LOGI(TAG, "TCA8418 keyboard initialized");
}

void Tca8418Keyboard::ConfigureMatrix() {
    // Cardputer Adv keyboard is wired as a 7x8 matrix (rows: R0-R6, cols: C0-C7).
    // KP_GPIO1: R0-R7 (bits 0-7)
    // KP_GPIO2: C0-C7 (bits 0-7)
    // KP_GPIO3: C8-C9 + GPIO (unused here)
    WriteReg(TCA8418_REG_KP_GPIO_1, 0x7F);  // R0-R6
    WriteReg(TCA8418_REG_KP_GPIO_2, 0xFF);  // C0-C7
    WriteReg(TCA8418_REG_KP_GPIO_3, 0x00);  // no extended cols
}

void Tca8418Keyboard::EnableInterrupts() {
    // Enable key event interrupt
    uint8_t cfg = TCA8418_CFG_KE_IEN | TCA8418_CFG_OVR_FLOW_M | TCA8418_CFG_INT_CFG;
    WriteReg(TCA8418_REG_CFG, cfg);
}

void Tca8418Keyboard::FlushEvents() {
    // Read and discard all pending key events
    uint8_t event;
    int count = 0;
    while ((event = GetEvent()) != 0 && count < 10) {
        count++;
    }

    // Clear interrupt status
    WriteReg(TCA8418_REG_INT_STAT, 0x1F);
}

uint8_t Tca8418Keyboard::GetEvent() {
    return ReadReg(TCA8418_REG_KEY_EVENT_A);
}

void Tca8418Keyboard::UpdateModifierState(uint8_t row, uint8_t col, bool pressed) {
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

LegacyKeyCode Tca8418Keyboard::MapLegacyKeyCode(uint8_t row, uint8_t col) {
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

KeyEvent Tca8418Keyboard::MapKeyEvent(uint8_t row, uint8_t col, bool pressed) {
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

void IRAM_ATTR Tca8418Keyboard::GpioIsrHandler(void* arg) {
    Tca8418Keyboard* keyboard = static_cast<Tca8418Keyboard*>(arg);
    keyboard->isr_flag_ = true;

    // Wake up the keyboard task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (keyboard->task_handle_) {
        vTaskNotifyGiveFromISR(keyboard->task_handle_, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void Tca8418Keyboard::KeyboardTask(void* arg) {
    Tca8418Keyboard* keyboard = static_cast<Tca8418Keyboard*>(arg);

    while (true) {
        // Wait for interrupt notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Small delay for debounce / allow event FIFO to fill
        vTaskDelay(pdMS_TO_TICKS(5));

        // Drain pending key events until the IRQ condition clears.
        for (int guard = 0; guard < 128; guard++) {
            uint8_t int_stat = keyboard->ReadReg(TCA8418_REG_INT_STAT);
            if ((int_stat & TCA8418_INT_STAT_K_INT) == 0) {
                keyboard->isr_flag_ = false;
                break;
            }

            uint8_t event = keyboard->GetEvent();
            if (event == 0) {
                // No event available, try clearing and re-checking.
                keyboard->WriteReg(TCA8418_REG_INT_STAT, 0x1F);
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            bool pressed = (event & 0x80) != 0;
            uint8_t key_code = event & 0x7F;
            if (key_code == 0) {
                continue;
            }

            // Raw decode: TCA8418 key code = (row * 10) + col + 1
            uint8_t raw_row = (key_code - 1) / 10;
            uint8_t raw_col = (key_code - 1) % 10;

            // Cardputer Adv uses 7x8, so ignore events outside 0..6/0..7.
            uint8_t row = raw_row;
            uint8_t col = raw_col;
            if (!RemapRawKeyToLogical(row, col)) {
                ESP_LOGD(TAG, "Ignored key: code=%d raw_row=%d raw_col=%d", key_code, raw_row, raw_col);
                continue;
            }

            // De-duplicate spurious repeated press/release events (debounce/IRQ quirks).
            const uint64_t mask = LogicalKeyMask(row, col);
            if (mask != 0) {
                const bool was_pressed = (keyboard->key_state_mask_ & mask) != 0;
                if (pressed == was_pressed) {
                    continue;
                }
                if (pressed) {
                    keyboard->key_state_mask_ |= mask;
                } else {
                    keyboard->key_state_mask_ &= ~mask;
                }
            }

            ESP_LOGD(TAG, "Key %s: code=%d raw=(%d,%d) mapped=(%d,%d)",
                     pressed ? "pressed" : "released", key_code, raw_row, raw_col, row, col);

            keyboard->UpdateModifierState(row, col, pressed);

            if (keyboard->key_event_callback_) {
                KeyEvent key_event = keyboard->MapKeyEvent(row, col, pressed);
                keyboard->key_event_callback_(key_event);
            }

            if (pressed && keyboard->key_callback_) {
                LegacyKeyCode mapped_key = keyboard->MapLegacyKeyCode(row, col);
                if (mapped_key != KEY_OTHER && mapped_key != KEY_NONE) {
                    keyboard->key_callback_(mapped_key);
                }
            }
        }

        // Clear all interrupt status bits (K_INT, GPI, overflow, etc.)
        keyboard->WriteReg(TCA8418_REG_INT_STAT, 0x1F);
    }
}
