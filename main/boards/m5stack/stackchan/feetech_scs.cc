/*
 * SPDX-License-Identifier: MIT
 *
 * Clean-room reimplementation of the SCSCL protocol layer from the public
 * Feetech SCS0009 datasheet. No code from the GPLv3 SCServo_lib is used.
 */
#include "feetech_scs.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <cstring>
#include <algorithm>

static const char* TAG = "FeetechScs";

namespace {

// SCSCL register addresses (from the public datasheet).
constexpr uint8_t REG_OPERATION_MODE   = 33;  // 0x21 — 0=Position, 1=PWM
constexpr uint8_t REG_TORQUE_ENABLE    = 40;  // 0x28
constexpr uint8_t REG_GOAL_POSITION_L  = 42;  // 0x2A — followed by H, time L/H, speed L/H
constexpr uint8_t REG_GOAL_SPEED_L     = 46;  // 0x2E — also PWM target in PWM mode
constexpr uint8_t REG_PRESENT_POSITION_L = 56;  // 0x38
constexpr uint8_t REG_MOVING           = 66;  // 0x42

// Protocol instruction codes.
constexpr uint8_t INST_READ  = 0x02;
constexpr uint8_t INST_WRITE = 0x03;

// Hard physical limits enforced inside this driver. The caller is expected
// to have already clamped to its own application limits, but we re-check
// here so a bug upstream can't accidentally drive the servo past the
// register range and physically damage the rig.
constexpr uint16_t HARD_POS_MAX = 1023;
constexpr int16_t  HARD_PWM_ABS_MAX = 1023;

// Bus response timeout. 1 Mbps × 8 bytes ≈ 80 µs; 20 ms is plenty.
constexpr TickType_t RX_TIMEOUT = pdMS_TO_TICKS(20);

uint8_t calc_checksum(const uint8_t* buf, size_t len)
{
    // Sum of bytes from ID through last param (i.e. skip the two header
    // 0xFF bytes), inverted.
    uint32_t sum = 0;
    for (size_t i = 2; i < len; i++) sum += buf[i];
    return static_cast<uint8_t>(~sum & 0xFF);
}

}  // namespace

void FeetechScs::begin(uart_port_t uart, int baud, int tx_pin, int rx_pin)
{
    if (_ready && _uart == uart) return;

    _uart = uart;

    uart_config_t cfg = {};
    cfg.baud_rate     = baud;
    cfg.data_bits     = UART_DATA_8_BITS;
    cfg.parity        = UART_PARITY_DISABLE;
    cfg.stop_bits     = UART_STOP_BITS_1;
    cfg.flow_ctrl     = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk    = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(uart, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(uart, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    if (!uart_is_driver_installed(uart)) {
        ESP_ERROR_CHECK(uart_driver_install(uart, 256, 0, 0, nullptr, 0));
    }

    _ready = true;
    ESP_LOGI(TAG, "begin uart=%d baud=%d tx=%d rx=%d", (int)uart, baud, tx_pin, rx_pin);
}

int FeetechScs::write_reg(uint8_t id, uint8_t addr, const uint8_t* data, uint8_t n)
{
    if (!_ready) return -1;

    // 0xFF 0xFF ID LEN INST ADDR PARAMS... CHK
    // LEN = (ADDR + PARAMS + CHK + INST) -> = n + 3
    uint8_t buf[16];
    if (n + 7 > sizeof(buf)) return -1;  // safety bound — we never write more than 6 params

    buf[0] = 0xFF;
    buf[1] = 0xFF;
    buf[2] = id;
    buf[3] = static_cast<uint8_t>(n + 3);
    buf[4] = INST_WRITE;
    buf[5] = addr;
    if (data && n) std::memcpy(&buf[6], data, n);
    size_t total = 6 + n;
    buf[total]   = calc_checksum(buf, total);
    total += 1;

    uart_flush_input(_uart);
    int written = uart_write_bytes(_uart, reinterpret_cast<const char*>(buf), total);
    if (written != (int)total) return -1;

    // Let the frame leave the wire before reading this half-duplex bus. begin() installs the
    // driver with tx_buffer_size 0, so uart_write_bytes returns once the bytes sit in the
    // hardware TX FIFO, not once they have been shifted out.
    //
    // This is ordering hygiene, not a fix for an observed fault. The comment that stood here
    // said otherwise and was wrong on every leg: it claimed our own outgoing bytes clip the
    // incoming ACK, but uart_diag reports buf_after 0 alongside tx_done ESP_OK, so RX never
    // sees our TX and there is nothing to clip (a real echo would also fail every call, not
    // some); the drain is about 80 us at 1 Mbaud against an RX_TIMEOUT of 20 ms; and the
    // intermittent bus errors it credited itself with fixing were the servos having no power
    // at all before VM EN was asserted (D-496). Our own re-review had already declined to call
    // the omission a defect, for the right reason.
    //
    // Kept regardless: reading a half-duplex line while we may still be driving it is the wrong
    // order whatever the margin, and uart_wait_tx_done returns as soon as the FIFO empties, so
    // the 100 ms is a ceiling and not a cost.
    if (uart_wait_tx_done(_uart, pdMS_TO_TICKS(100)) != ESP_OK) return -1;

    // Wait for status reply (0xFF 0xFF ID LEN ERR CHK = 6 bytes total) when
    // not broadcasting. Broadcast (id=0xFE) does not get a reply.
    if (id == 0xFE) return 0;

    uint8_t reply[6] = {};
    int got          = uart_read_bytes(_uart, reply, sizeof(reply), RX_TIMEOUT);
    if (got != (int)sizeof(reply))                  return -1;
    if (reply[0] != 0xFF || reply[1] != 0xFF)       return -1;
    if (reply[2] != id)                             return -1;
    if (reply[3] != 2)                              return -1;
    uint8_t exp_chk = static_cast<uint8_t>(~(reply[2] + reply[3] + reply[4]) & 0xFF);
    if (reply[5] != exp_chk)                        return -1;
    return 0;
}

int FeetechScs::read_reg(uint8_t id, uint8_t addr, uint8_t n, uint8_t* out)
{
    if (!_ready || !out || n == 0) return -1;

    // 0xFF 0xFF ID LEN(=4) INST(=2) ADDR N CHK
    uint8_t buf[8];
    buf[0] = 0xFF;
    buf[1] = 0xFF;
    buf[2] = id;
    buf[3] = 4;
    buf[4] = INST_READ;
    buf[5] = addr;
    buf[6] = n;
    buf[7] = calc_checksum(buf, 7);

    uart_flush_input(_uart);
    int written = uart_write_bytes(_uart, reinterpret_cast<const char*>(buf), 8);
    if (written != 8) return -1;

    // Same TX-drain wait as write_reg(); see the comment there for what it does and does
    // not do.
    if (uart_wait_tx_done(_uart, pdMS_TO_TICKS(100)) != ESP_OK) return -1;

    // Reply: 0xFF 0xFF ID LEN(=2+n) ERR DATA[n] CHK
    const size_t reply_len = 6u + n;
    uint8_t reply[16] = {};
    if (reply_len > sizeof(reply)) return -1;
    int got = uart_read_bytes(_uart, reply, reply_len, RX_TIMEOUT);
    if (got != (int)reply_len)                            return -1;
    if (reply[0] != 0xFF || reply[1] != 0xFF)             return -1;
    if (reply[2] != id)                                   return -1;
    if (reply[3] != static_cast<uint8_t>(n + 2))          return -1;

    uint32_t sum = reply[2] + reply[3] + reply[4];
    for (uint8_t i = 0; i < n; i++) sum += reply[5 + i];
    uint8_t exp_chk = static_cast<uint8_t>(~sum & 0xFF);
    if (reply[reply_len - 1] != exp_chk)                  return -1;

    std::memcpy(out, &reply[5], n);
    return 0;
}

int FeetechScs::WritePos(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed)
{
    // Defensive clamp — protects the mech if any caller forgets to clamp.
    if (position > HARD_POS_MAX) position = HARD_POS_MAX;

    // SCSCL series stores 16-bit registers BIG-endian over the wire: the
    // byte at the lower register address (`*_L`) is actually the *high*
    // byte of the word. (The naming in the datasheet is "L = low address",
    // not "low byte" — easy to misread.) Sending little-endian made the
    // servo see a huge value and clamp to the mechanical maximum.
    uint8_t params[6] = {
        static_cast<uint8_t>((position >> 8) & 0xFF),
        static_cast<uint8_t>(position & 0xFF),
        static_cast<uint8_t>((time_ms >> 8) & 0xFF),
        static_cast<uint8_t>(time_ms & 0xFF),
        static_cast<uint8_t>((speed >> 8) & 0xFF),
        static_cast<uint8_t>(speed & 0xFF),
    };
    return write_reg(id, REG_GOAL_POSITION_L, params, 6);
}

int FeetechScs::ReadPos(uint8_t id)
{
    uint8_t b[2];
    if (read_reg(id, REG_PRESENT_POSITION_L, 2, b) != 0) return -1;
    // Big-endian: byte at lower address is the high byte (see WritePos comment).
    return (b[0] << 8) | b[1];
}

int FeetechScs::ReadMove(uint8_t id)
{
    uint8_t b;
    if (read_reg(id, REG_MOVING, 1, &b) != 0) return -1;
    return b;
}

int FeetechScs::EnableTorque(uint8_t id, uint8_t enable)
{
    uint8_t v = enable ? 1 : 0;
    return write_reg(id, REG_TORQUE_ENABLE, &v, 1);
}

int FeetechScs::ReadToqueEnable(uint8_t id)
{
    uint8_t b;
    if (read_reg(id, REG_TORQUE_ENABLE, 1, &b) != 0) return -1;
    return b;
}

int FeetechScs::SwitchMode(uint8_t id, uint8_t mode)
{
    uint8_t v = (mode == 0) ? 0 : 1;
    return write_reg(id, REG_OPERATION_MODE, &v, 1);
}

int FeetechScs::WritePWM(uint8_t id, int16_t pwm)
{
    if (pwm >  HARD_PWM_ABS_MAX) pwm =  HARD_PWM_ABS_MAX;
    if (pwm < -HARD_PWM_ABS_MAX) pwm = -HARD_PWM_ABS_MAX;
    // SCS0009 PWM word: low 10 bits = magnitude, bit 10 = direction.
    uint16_t mag = static_cast<uint16_t>(pwm < 0 ? -pwm : pwm) & 0x03FF;
    uint16_t v   = mag | (pwm < 0 ? 0x0400 : 0x0000);
    // Big-endian word — see WritePos for the byte-order rationale.
    uint8_t params[2] = {
        static_cast<uint8_t>((v >> 8) & 0xFF),
        static_cast<uint8_t>(v & 0xFF),
    };
    return write_reg(id, REG_GOAL_SPEED_L, params, 2);
}
