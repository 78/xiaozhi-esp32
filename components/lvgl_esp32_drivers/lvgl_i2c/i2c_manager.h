#ifndef _I2C_MANAGER_H
#define _I2C_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif


/*

    If you copy the i2c_manager files to your own component instead of
    depending on i2c_manager, you MUST uncomment the define below
    and put in some short string that identifies your component (such
    as 'xyz'). This will cause i2c_manager to create functions named
    xyz_i2c_* instead of i2c_manager_*. See README.md for details.

*/

#define I2C_OEM lvgl


// Only here to get the I2C_NUM_0 and I2C_NUM_1 defines.
#include <driver/i2c.h>
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define CONCATX(A, B) A ## B
#define CONCAT(A, B) CONCATX(A, B)
#define STR_LITERAL(s) # s
#define STR_EXPAND(s) STR_LITERAL(s)
#define STR_QUOTE(s) STR_EXPAND(STR_EXPAND(s))

#ifdef I2C_OEM
    #define I2C_NAME_PREFIX CONCAT(I2C_OEM, _i2c)
#else
    #define I2C_NAME_PREFIX i2c_manager
#endif
#define I2C_TAG STR_EXPAND(I2C_NAME_PREFIX)

#define I2C_FN(s) CONCAT(I2C_NAME_PREFIX, s)


#define I2C_ADDR_10 ( 1 << 15 )
#define I2C_REG_16  ( 1 << 31 )
#define I2C_NO_REG  ( 1 << 30 )

esp_err_t I2C_FN(_init)(i2c_port_t port);
esp_err_t I2C_FN(_read)(i2c_port_t port, uint16_t addr, uint32_t reg, uint8_t *buffer, uint16_t size);
esp_err_t I2C_FN(_write)(i2c_port_t port, uint16_t addr, uint32_t reg, const uint8_t *buffer, uint16_t size);
esp_err_t I2C_FN(_close)(i2c_port_t port);
esp_err_t I2C_FN(_lock)(i2c_port_t port);
esp_err_t I2C_FN(_unlock)(i2c_port_t port);
esp_err_t I2C_FN(_force_unlock)(i2c_port_t port);


#ifdef I2C_OEM

    void I2C_FN(_locking)(void* leader);

#else

    void* i2c_manager_locking();

    typedef struct {
        int32_t (* read)(void *handle, uint8_t address, uint8_t reg, uint8_t *buffer, uint16_t size);
        int32_t (* write)(void *handle, uint8_t address, uint8_t reg, const uint8_t *buffer, uint16_t size);
        void *handle;
    } i2c_hal_t;

    void* i2c_hal(i2c_port_t port);

#endif


#ifdef __cplusplus
}
#endif
#endif
