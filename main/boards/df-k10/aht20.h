#ifndef MAIN_BOARDS_DF_K10_AHT20_H_
#define MAIN_BOARDS_DF_K10_AHT20_H_

#include <cstdint>
#include <cstdbool>
#include "k10_i2c_device.h"

class AHT20 : public K10I2cDevice {
 public:
    explicit AHT20(i2c_master_bus_handle_t i2c_bus, uint8_t addr = 0x38);
    bool begin();
    void reset();
    bool get_measurements(float *temperature, float *humidity, bool crc_en = false);

 private:
    typedef union {
        struct {
            uint8_t rsv0 : 3;
            uint8_t cal_en : 1;
            uint8_t rsv1 : 3;
            uint8_t busy : 1;
        };
        uint8_t raw;
    } status_reg_t;

    static constexpr uint8_t  CMD_INIT                     = 0xBE;
    ///< Init command

    static constexpr uint8_t  CMD_INIT_PARAMS_1ST          = 0x08;
    ///< The first parameter of init command: 0x08

    static constexpr uint8_t  CMD_INIT_PARAMS_2ND          = 0x00;
    ///< The second parameter of init command: 0x00

    static constexpr uint16_t CMD_INIT_TIME                = 10;
    ///< Waiting time for init completion: 10ms

    static constexpr uint8_t  CMD_MEASUREMENT              = 0xAC;
    ///< Trigger measurement command

    static constexpr uint8_t  CMD_MEASUREMENT_PARAMS_1ST   = 0x33;
    ///< The first parameter of trigger measurement command: 0x33

    static constexpr uint8_t  CMD_MEASUREMENT_PARAMS_2ND   = 0x00;
    ///< The second parameter of trigger measurement command: 0x00

    static constexpr uint16_t CMD_MEASUREMENT_TIME         = 80;    ///< Measurement command completion time: 80ms
    static constexpr uint8_t  CMD_MEASUREMENT_DATA_LEN     = 6;
    ///< Return length when the measurement command is without CRC check.

    static constexpr uint8_t  CMD_MEASUREMENT_DATA_CRC_LEN = 7;
    ///< Return data length when the measurement command is with CRC check.

    static constexpr uint8_t  CMD_SOFT_RESET               = 0xBA;
    ///< Soft reset command

    static constexpr uint16_t CMD_SOFT_RESET_TIME          = 20;
    ///< Soft reset time: 20ms

    static constexpr uint8_t  CMD_STATUS                   = 0x71;
    ///< Get status word command

    float _temperature;
    float _humidity;
    bool _initialized;

    bool _check_crc(uint8_t crc, uint8_t *data, size_t len);
    bool _is_device_ready();
    bool _check_calibration();
    uint8_t _read_status();
    void _send_command(uint8_t cmd);
    void _send_command(uint8_t cmd, uint8_t arg1, uint8_t arg2);
    bool _start_measurement(bool crc_en);
};

#endif  // MAIN_BOARDS_DF_K10_AHT20_H_
