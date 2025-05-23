/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "iot_servo.h"
#include "servo_dog_ctrl.h"

static const char *TAG = "servo_dog_ctrl";

static QueueHandle_t dog_action_queue;

#define DONT_USE_SERVO              0

#define SERVO_FL_IO                 CONFIG_SERVO_FL_IO
#define SERVO_FR_IO                 CONFIG_SERVO_FR_IO
#define SERVO_BL_IO                 CONFIG_SERVO_BL_IO
#define SERVO_BR_IO                 CONFIG_SERVO_BR_IO

#define BOW_OFFSET                  50  //Front lying /backward offset angle

#define STEP_OFFSET                 5

#if 1 //2rd
#define FL_ANGLE_STEP_FORWARD       (CONFIG_FL_ANGLE_NEUTRAL - 20)
#define FL_ANGLE_STEP_BACKWARD      (CONFIG_FL_ANGLE_NEUTRAL + 20)
// #define FL_ANGLE_NEUTRAL            CONFIG_FL_ANGLE_NEUTRAL
#define FL_ANGLE_NEUTRAL            78

#define FR_ANGLE_STEP_FORWARD       (CONFIG_FR_ANGLE_NEUTRAL + 20)
#define FR_ANGLE_STEP_BACKWARD      (CONFIG_FR_ANGLE_NEUTRAL - 20)
// #define FR_ANGLE_NEUTRAL            CONFIG_FR_ANGLE_NEUTRAL
#define FR_ANGLE_NEUTRAL            108

#define BL_ANGLE_STEP_FORWARD       (CONFIG_BL_ANGLE_NEUTRAL + 20)
#define BL_ANGLE_STEP_BACKWARD      (CONFIG_BL_ANGLE_NEUTRAL - 20)
// #define BL_ANGLE_NEUTRAL            CONFIG_BL_ANGLE_NEUTRAL
#define BL_ANGLE_NEUTRAL            105

#define BR_ANGLE_STEP_FORWARD       (CONFIG_BR_ANGLE_NEUTRAL - 20)
#define BR_ANGLE_STEP_BACKWARD      (CONFIG_BR_ANGLE_NEUTRAL + 20)
// #define BR_ANGLE_NEUTRAL            CONFIG_BR_ANGLE_NEUTRAL
#define BR_ANGLE_NEUTRAL            60
#else
#define FL_ANGLE_STEP_FORWARD       (CONFIG_FL_ANGLE_NEUTRAL - 20)
#define FL_ANGLE_STEP_BACKWARD      (CONFIG_FL_ANGLE_NEUTRAL + 20)
// #define FL_ANGLE_NEUTRAL            CONFIG_FL_ANGLE_NEUTRAL
#define FL_ANGLE_NEUTRAL            78

#define FR_ANGLE_STEP_FORWARD       (CONFIG_FR_ANGLE_NEUTRAL + 20)
#define FR_ANGLE_STEP_BACKWARD      (CONFIG_FR_ANGLE_NEUTRAL - 20)
// #define FR_ANGLE_NEUTRAL            CONFIG_FR_ANGLE_NEUTRAL
#define FR_ANGLE_NEUTRAL            108

#define BL_ANGLE_STEP_FORWARD       (CONFIG_BL_ANGLE_NEUTRAL + 20)
#define BL_ANGLE_STEP_BACKWARD      (CONFIG_BL_ANGLE_NEUTRAL - 20)
// #define BL_ANGLE_NEUTRAL            CONFIG_BL_ANGLE_NEUTRAL
#define BL_ANGLE_NEUTRAL            110

#define BR_ANGLE_STEP_FORWARD       (CONFIG_BR_ANGLE_NEUTRAL - 20)
#define BR_ANGLE_STEP_BACKWARD      (CONFIG_BR_ANGLE_NEUTRAL + 20)
// #define BR_ANGLE_NEUTRAL            CONFIG_BR_ANGLE_NEUTRAL
#define BR_ANGLE_NEUTRAL            64
#endif

typedef enum {
    SERVO_FL = 0,
    SERVO_FR,
    SERVO_BL,
    SERVO_BR,
} servo_id_t;

static void servo_set_angle(servo_id_t servo_id, uint16_t angle)
{
#if !DONT_USE_SERVO
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, servo_id, angle);
#endif
}

static void servo_dog_neutral(int8_t offset)
{
    servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + offset);
    servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - offset);
    servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - offset);
    servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + offset);
    vTaskDelay(20 / portTICK_PERIOD_MS);
}

static void servo_dog_forward(uint8_t step_count, uint16_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;
        for (int step = 0; step < step_count; step++) {
            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - i);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD + i);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + i - STEP_OFFSET);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD - i - STEP_OFFSET);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);

            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD + i);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - i);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD - i + STEP_OFFSET);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + i + STEP_OFFSET);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        for (int i = 0; i < 20; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD + i);
            servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + i);
            servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD - i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_backward(uint8_t step_count, uint16_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;

        for (int step = 0; step < step_count; step++) {
            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD - i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + i);
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD + i - STEP_OFFSET);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - i - STEP_OFFSET);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);

            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD - i);
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - i + STEP_OFFSET);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD + i + STEP_OFFSET);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        for (int i = 0; i < 20; i++) {
            servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD - i);
            servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + i);
            servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_turn_left(uint8_t step_count, uint16_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;

        for (int step = 0; step < step_count; step++) {
            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - i + STEP_OFFSET);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - i + STEP_OFFSET);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }

            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD + i - STEP_OFFSET);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD + i - STEP_OFFSET);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD - i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD - i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
        }

        for (int i = 0; i < 20; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - i);
            servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + i);
            servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_turn_right(uint8_t step_count, uint16_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;

        for (int step = 0; step < step_count; step++) {
            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD + i);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD + i);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD - i + STEP_OFFSET);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD - i + STEP_OFFSET);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }

            for (int i = 0; i < 40; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - i - STEP_OFFSET);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - i - STEP_OFFSET);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
        }

        for (int i = 0; i < 20; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD + i);
            servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD - i);
            servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD - i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_bow(uint16_t hold_ms, uint16_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;

        // From neutrality, gradually forward
        for (int i = 0; i < BOW_OFFSET; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }

        // Pause for a while and keep the front lie on your side for a while
        vTaskDelay(hold_ms / portTICK_PERIOD_MS);

        // Slowly return to neutrality
        for (int i = 0; i < BOW_OFFSET; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - BOW_OFFSET + i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + BOW_OFFSET - i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - BOW_OFFSET + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + BOW_OFFSET - i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_lean_back(uint16_t hold_ms, uint16_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;
        // From neutral to lean back
        for (int i = 0; i < BOW_OFFSET; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }

        // Keep leaning for a while
        vTaskDelay(hold_ms / portTICK_PERIOD_MS);

        // From the slow fallback movement to the neutral state
        for (int i = 0; i < BOW_OFFSET; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + BOW_OFFSET - i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - BOW_OFFSET + i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + BOW_OFFSET - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - BOW_OFFSET + i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_bow_and_lean_back(int repeat_count, uint16_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;

        // Step 1: Middle -> Forward lying (prepare for action)
        for (int i = 0; i < BOW_OFFSET; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }

        // Mid-section cycle: forward lying <-> backward
        for (int r = 0; r < repeat_count; r++) {
            // Front lying -> Back lying
            for (int i = 0; i < BOW_OFFSET * 2; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - BOW_OFFSET + i);
                servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + BOW_OFFSET - i);
                servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - BOW_OFFSET + i);
                servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + BOW_OFFSET - i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }

            // Lean back -> Forward hang
            for (int i = 0; i < BOW_OFFSET * 2; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + BOW_OFFSET - i);
                servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - BOW_OFFSET + i);
                servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + BOW_OFFSET - i);
                servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - BOW_OFFSET + i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
        }

        // The last step: From "forward lying" -> Intermediate state (smooth finishing)
        for (int i = 0; i < BOW_OFFSET; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - BOW_OFFSET + i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + BOW_OFFSET - i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - BOW_OFFSET + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + BOW_OFFSET - i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_sway_back_and_forth(void)
{
    uint8_t step_delay_ms = 5;
    uint8_t sway_offset = 18;
    for (int i = 0; i < sway_offset; i++) {
        servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - i);
        servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + i);
        servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - i);
        servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + i);
        vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
    }

    while (sway_offset > 0 && sway_offset <= 18) {
        // Front lying -> Back lying
        for (int i = 0; i < sway_offset * 2; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - sway_offset + i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + sway_offset - i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - sway_offset + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + sway_offset - i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }

        // Lean back -> Forward hang
        for (int i = 0; i < sway_offset * 2; i++) {
            servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + sway_offset - i);
            servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - sway_offset + i);
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + sway_offset - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - sway_offset + i);
            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
        sway_offset -= 3;
    }
}

static void servo_dog_lay_down(void)
{
    for (int i = 0; i < 60; i++) {
        servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL - i);
        servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL + i);
        servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + i);
        servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - i);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void servo_dog_sway_left_right(uint8_t repeat_count, uint8_t angle_step, uint8_t speed)
{
    if (speed != 0) {
        uint16_t step_delay_ms = 500 / speed;
        servo_dog_neutral(20);
        for (int r = 0; r < repeat_count; r++) {
            for (int i = 0; i < angle_step; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD - i);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD - i);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }

            for (int i = 0; i < angle_step * 2; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD - angle_step + i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD - angle_step + i);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD - angle_step + i);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - angle_step + i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }

            for (int i = 0; i < angle_step; i++) {
                servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD + angle_step - i);
                servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + angle_step - i);
                servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + angle_step - i);
                servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD + angle_step - i);
                vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
            }
        }
    }
}

static void servo_dog_shake_hand(uint8_t repeat_count, uint16_t hold_ms)
{
    for (int r = 0; r < repeat_count; r++) {
        // Swing the left hind leg back 20 degrees
        for (int i = 0; i < 60; i++) {
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + i);
            vTaskDelay(8 / portTICK_PERIOD_MS);
        }
        // Straighten right front leg
        // const int start_angle = FR_ANGLE_NEUTRAL + 60;
        // const int end_angle = FR_ANGLE_NEUTRAL + 45;
        const int start_angle = FR_ANGLE_NEUTRAL + 72;
        const int end_angle = FR_ANGLE_NEUTRAL + 45 + 12;
        servo_set_angle(SERVO_FR, start_angle);
        // Right front leg swings 15 degrees, handshake
        for (int j = 0; j < 10 * 5; j++) {
            for (int angle = start_angle; angle >= end_angle; angle--) {
                servo_set_angle(SERVO_FR, angle);
                vTaskDelay(15 / portTICK_PERIOD_MS);
            }
            for (int angle = end_angle; angle <= start_angle; angle++) {
                servo_set_angle(SERVO_FR, angle);
                vTaskDelay(15 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(hold_ms / portTICK_PERIOD_MS);
        // Right front leg back to its original position
        for (int angle = start_angle; angle >= FR_ANGLE_NEUTRAL; angle--) {
            servo_set_angle(SERVO_FR, angle);
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
        // Left hind leg back to its original position
        for (int i = 0; i < 60; i++) {
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - 60 + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + 60 - i);
            vTaskDelay(8 / portTICK_PERIOD_MS);
        }
    }
}

static void servo_dog_jump_forward(void)
{
    servo_dog_neutral(0);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD - 10);
    servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD + 10);
    servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD - 40);
    servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD + 40);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD + 50);
    servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD - 50);
    vTaskDelay(40 / portTICK_PERIOD_MS);
    servo_set_angle(SERVO_FL, FL_ANGLE_STEP_FORWARD - 50);
    servo_set_angle(SERVO_FR, FR_ANGLE_STEP_FORWARD + 50);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD);
    servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD);
    vTaskDelay(150 / portTICK_PERIOD_MS);
    servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL);
    servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL);
    vTaskDelay(200 / portTICK_PERIOD_MS);
}

static void servo_dog_jump_backward(void)
{
    servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD + 20);
    servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD - 20);
    servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD);
    servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    servo_set_angle(SERVO_FL, FL_ANGLE_STEP_BACKWARD);
    servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD);
    servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD);
    servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL);
    servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL);
    servo_set_angle(SERVO_BL, BL_ANGLE_STEP_FORWARD);
    servo_set_angle(SERVO_BR, BR_ANGLE_STEP_FORWARD);
    vTaskDelay(150 / portTICK_PERIOD_MS);
    servo_dog_neutral(0);
}

static void servo_dog_poke(void)
{
    servo_set_angle(SERVO_FL, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    for (int i = 0; i < 5; i++) {
        servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - i);
        servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - 7 * i);
        servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + 7 * i);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 20; i++) {
            servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD + 15 - i);
            servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD - 15 + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD + 15 - i);
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 20; i++) {
            servo_set_angle(SERVO_FR, FR_ANGLE_STEP_BACKWARD - 5 + i);
            servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD + 5 - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD - 5 + i);
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
    }
    servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL);
    for (int i = 0; i < 35; i++) {
        servo_set_angle(SERVO_FL, 2 * i);
        servo_set_angle(SERVO_BL, BL_ANGLE_STEP_BACKWARD - 15 + i);
        servo_set_angle(SERVO_BR, BR_ANGLE_STEP_BACKWARD + 15 - i);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void servo_dog_shake_back_legs(void)
{
    for (int i = 0; i < 18; i++) {
        servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + 2 * i);
        servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - 2 * i);
        servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + 3 * i);
        servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - 3 * i);
        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
    for (int j = 0; j < 12 * 5; j++) {
        for (int i = 0; i < 6; i++) {
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + 54 + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - 54 + i);
            vTaskDelay(7 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 12; i++) {
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + 54 - i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - 54 - i);
            vTaskDelay(7 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i < 6; i++) {
            servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + 54 + i);
            servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - 54 + i);
            vTaskDelay(7 / portTICK_PERIOD_MS);
        }
    }
    for (int i = 0; i < 18; i++) {
        servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + 36 - 2 * i);
        servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - 36 + 2 * i);
        servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL + 54 - 3 * i);
        servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL - 54 + 3 * i);
        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}

static void servo_dog_retract_legs(void)
{
    for (int i = 0; i < 110; i++) {
        servo_set_angle(SERVO_FL, FL_ANGLE_NEUTRAL + i);
        servo_set_angle(SERVO_FR, FR_ANGLE_NEUTRAL - i);
        vTaskDelay(4 / portTICK_PERIOD_MS);
    }
    for (int i = 0; i < 103; i++) {
        servo_set_angle(SERVO_BL, BL_ANGLE_NEUTRAL - i);
        servo_set_angle(SERVO_BR, BR_ANGLE_NEUTRAL + i);
        vTaskDelay(4 / portTICK_PERIOD_MS);
    }
}

void servo_dog_ledc_stop(void)
{
    vTaskDelay(50 / portTICK_PERIOD_MS);
    for (int ch = LEDC_CHANNEL_0; ch <= LEDC_CHANNEL_3; ch++) {
        ledc_stop(LEDC_LOW_SPEED_MODE, ch, 1);
    }
}

static void servo_dog_ctrl_task(void *arg)
{
    ESP_LOGI(TAG, "Servo Test Task");
    servo_dog_neutral(0);

    dog_action_msg_t msg;
    while (1) {
        servo_dog_ledc_stop();
        if (xQueueReceive(dog_action_queue, &msg, portMAX_DELAY)) {
            ESP_LOGE(TAG, "Action: %d", msg.state);
            switch (msg.state) {
            case DOG_STATE_FORWARD:
                servo_dog_forward(msg.repeat_count, msg.speed);
                break;
            case DOG_STATE_BACKWARD:
                servo_dog_backward(msg.repeat_count, msg.speed);
                break;
            case DOG_STATE_BOW:
                servo_dog_bow(msg.hold_time_ms, msg.speed);
                break;
            case DOG_STATE_LEAN_BACK:
                servo_dog_lean_back(msg.hold_time_ms, msg.speed);
                break;
            case DOG_STATE_BOW_LEAN:
                servo_dog_bow_and_lean_back(msg.repeat_count, msg.speed);
                break;
            case DOG_STATE_SWAY_BACK_FORTH:
                servo_dog_sway_back_and_forth();
                break;
            case DOG_STATE_TURN_LEFT:
                servo_dog_turn_left(msg.repeat_count, msg.speed);
                break;
            case DOG_STATE_TURN_RIGHT:
                servo_dog_turn_right(msg.repeat_count, msg.speed);
                break;
            case DOG_STATE_LAY_DOWN:
                servo_dog_lay_down();
                break;
            case DOG_STATE_SWAY:
                servo_dog_sway_left_right(msg.repeat_count, msg.angle_offset, msg.speed / 2);
                break;
            case DOG_STATE_SHAKE_HAND:
                servo_dog_shake_hand(msg.repeat_count, msg.hold_time_ms);
                break;
            case DOG_STATE_POKE:
                servo_dog_poke();
                break;
            case DOG_STATE_SHAKE_BACK_LEGS:
                servo_dog_shake_back_legs();
                break;
            case DOG_STATE_JUMP_FORWARD:
                servo_dog_jump_forward();
                break;
            case DOG_STATE_JUMP_BACKWARD:
                servo_dog_jump_backward();
                break;
            case DOG_STATE_RETRACT_LEGS:
                servo_dog_retract_legs();
                break;
            case DOG_STATE_IDLE:
                servo_dog_neutral(0);
                break;
            case DOG_STATE_MAX:
                servo_dog_neutral(0);
                break;
            }
        }
    }
    vTaskDelete(NULL);
    ESP_LOGI(TAG, "Servo Test Task Deleted");
}

static void servo_init(void)
{
    ESP_LOGI(TAG, "Servo Control");

    // Configure the server
    servo_config_t servo_cfg = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 50,
        .timer_number = LEDC_TIMER_0,
        .channels = {
            .servo_pin = {
                SERVO_FL_IO,
                SERVO_FR_IO,
                SERVO_BL_IO,
                SERVO_BR_IO,
            },
            .ch = {
                LEDC_CHANNEL_0,
                LEDC_CHANNEL_1,
                LEDC_CHANNEL_2,
                LEDC_CHANNEL_3,
            },
        },
        .channel_number = 4,
    };

#if !DONT_USE_SERVO
    // Initialize the server
    iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);
#endif
}

void servo_dog_send_action(servo_dog_state_t state, int repeat_count, int speed, int hold_time_ms, int angle_offset)
{
    dog_action_msg_t msg;
    msg.state = state;
    msg.repeat_count = repeat_count;
    msg.speed = speed;
    msg.hold_time_ms = hold_time_ms;
    msg.angle_offset = angle_offset;

    xQueueSend(dog_action_queue, &msg, portMAX_DELAY);
}

void servo_dog_ctrl_init(void)
{
    ESP_LOGI(TAG, "Servo Control");

#if !DONT_USE_SERVO
    esp_log_level_set("*", ESP_LOG_NONE);
#endif
    // Initialize the server
    servo_init();

    dog_action_queue = xQueueCreate(2, sizeof(dog_action_msg_t));

    // Create servo_dog_ctrl_task
    xTaskCreate(servo_dog_ctrl_task, "servo_dog_ctrl_task", 2048, NULL, 5, NULL);
}
