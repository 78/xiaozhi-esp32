/*
 * MIT-licensed minimal driver for Feetech SCS0009 / SCSCL serial servos.
 *
 * Implements only the seven calls our ScsServo wrapper actually uses, with
 * the same signatures as the upstream SCServo_lib SCSCL class so the rest
 * of the firmware doesn't change. Designed as a clean-room reimplementation
 * from the public protocol spec — does not contain any code derived from
 * the GPLv3 SCServo_lib.
 *
 * Protocol (Dynamixel 1.0-compatible half-duplex UART, 1 Mbps default):
 *   Packet:  0xFF 0xFF ID LEN INST PARAMS... CHK
 *   CHK = ~(ID + LEN + INST + Σ PARAMS) & 0xFF
 *
 * Register map references (all from the public SCS0009 / SCSCL datasheet):
 *   0x21 (33)  Operation mode      — 0=Position, 1=PWM
 *   0x28 (40)  Torque enable       — 0=off, 1=on
 *   0x2A (42)  Goal position L/H   — u16 (0..1023)
 *   0x2C (44)  Goal time L/H       — u16 (ms)
 *   0x2E (46)  Goal speed L/H      — u16 (also used as PWM target in PWM mode,
 *                                          bit 10 = direction)
 *   0x38 (56)  Present position L/H — u16
 *   0x42 (66)  Moving               — u8 (1 = still moving)
 *
 * Safety: every WritePos / WritePWM clamps its inputs to physical limits
 * before sending, so a caller that forgets to clamp can't drive the servo
 * past 0..1023 or PWM beyond ±1023.
 */
#pragma once
#include <cstdint>
#include <driver/uart.h>

class FeetechScs {
public:
    // One-time UART init. Idempotent — calling it twice on the same port
    // is a no-op.
    void begin(uart_port_t uart, int baud, int tx_pin, int rx_pin);

    // ---- Position-mode commands -------------------------------------------
    // Goal position 0..1023, time 0..65535 ms, speed 0..65535 (0 = max).
    int WritePos(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed);

    // Returns 0..1023 on success, -1 on timeout / checksum error.
    int ReadPos(uint8_t id);

    // Returns 1 if the servo is still moving, 0 if at rest, -1 on bus error.
    int ReadMove(uint8_t id);

    // ---- Torque ---------------------------------------------------------
    int EnableTorque(uint8_t id, uint8_t enable);
    int ReadToqueEnable(uint8_t id);  // typo preserved for SCSCL API parity

    // ---- PWM / wheel mode ------------------------------------------------
    // Switch operation mode. 0 = position, 1 = PWM.
    int SwitchMode(uint8_t id, uint8_t mode);

    // PWM output, signed -1023..1023. Bit 10 of the on-wire value carries
    // sign per the SCS0009 spec. SwitchMode(id, 1) must be called first.
    int WritePWM(uint8_t id, int16_t pwm);

private:
    uart_port_t _uart  = UART_NUM_1;
    bool        _ready = false;

    // Build & send a WRITE_DATA packet (INST=0x03). data may be nullptr if
    // n == 0.  Returns 0 on ACK, -1 on bus error.
    int write_reg(uint8_t id, uint8_t addr, const uint8_t* data, uint8_t n);

    // Build & send a READ_DATA packet (INST=0x02). Reads n bytes into out.
    // Returns 0 on success, -1 on bus / checksum error.
    int read_reg(uint8_t id, uint8_t addr, uint8_t n, uint8_t* out);
};
