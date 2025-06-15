/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_system.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_sccb_intf.h"
#include "esp_sccb_i2c.h"
#include "esp_cam_sensor.h"

#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#if CONFIG_CAMERA_OV5645
#include "ov5645.h"
#define SCCB0_CAM_DEVICE_ADDR OV5645_SCCB_ADDR
#elif CONFIG_CAMERA_SC2336
#include "sc2336.h"
#define SCCB0_CAM_DEVICE_ADDR SC2336_SCCB_ADDR
#else
#define SCCB0_CAM_DEVICE_ADDR 0x01
#endif

/* SCCB */
#define SCCB0_SCL             CONFIG_SCCB0_SCL
#define SCCB0_SDA             CONFIG_SCCB0_SDA
#define SCCB0_FREQ_HZ         CONFIG_SCCB0_FREQUENCY
#define SCCB0_PORT_NUM        I2C_NUM_0

#define TEST_MEMORY_LEAK_THRESHOLD (-100)

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

TEST_CASE("Camera sensor detect test", "[video]")
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = SCCB0_PORT_NUM,
        .scl_io_num = SCCB0_SCL,
        .sda_io_num = SCCB0_SDA,
        .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t bus_handle;
    esp_sccb_io_handle_t sccb_io;

    TEST_ESP_OK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    sccb_i2c_config_t sccb_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SCCB0_CAM_DEVICE_ADDR,
        .scl_speed_hz = SCCB0_FREQ_HZ,
    };

    TEST_ESP_OK(sccb_new_i2c_io(bus_handle, &sccb_config, &sccb_io));

    esp_cam_sensor_config_t cam0_config = {
        .sccb_handle = sccb_io,
        .reset_pin = -1,
        .pwdn_pin = -1,
        .xclk_pin = -1,
    };
#if CONFIG_CAMERA_OV5645
    esp_cam_sensor_device_t *cam0 = ov5645_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#elif CONFIG_CAMERA_SC2336
    esp_cam_sensor_device_t *cam0 = sc2336_detect(&cam0_config);
    TEST_ASSERT_MESSAGE(cam0 != NULL, "detect fail");

    TEST_ESP_OK(esp_cam_sensor_del_dev(cam0));
#endif
    TEST_ESP_OK(esp_sccb_del_i2c_io(sccb_io));

    TEST_ESP_OK(i2c_del_master_bus(bus_handle));
}

void app_main(void)
{
    /**
     * \ \     /_ _| __ \  ____|  _ \
     *  \ \   /   |  |   | __|   |   |
     *   \ \ /    |  |   | |     |   |
     *    \_/   ___|____/ _____|\___/
    */

    printf("\r\n");
    printf("\\ \\     /_ _| __ \\  ____|  _ \\  \r\n");
    printf(" \\ \\   /   |  |   | __|   |   |\r\n");
    printf("  \\ \\ /    |  |   | |     |   | \r\n");
    printf("   \\_/   ___|____/ _____|\\___/  \r\n");

    unity_run_menu();
}
