/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DOG_STATE_IDLE = -1,                // Idle state, the dog holds its current posture
    DOG_STATE_FORWARD = 0,              // Move forward, all four legs step forward according to the gait
    DOG_STATE_BACKWARD,                 // Move backward, all four legs move in reverse
    DOG_STATE_BOW,                      // Bowing, front legs press down, hips raised—like a dog stretching or being playful
    DOG_STATE_LEAN_BACK,                // Lean back, rear legs press down, front legs raised, body leans backward
    DOG_STATE_BOW_LEAN,                 // A repeated combination of bow and lean back, simulating play or dancing
    DOG_STATE_SWAY_BACK_FORTH,          // Sway back and forth
    DOG_STATE_TURN_LEFT,                // Turn left, coordinated movement of legs to turn left
    DOG_STATE_TURN_RIGHT,               // Turn right, coordinated movement of legs to turn right
    DOG_STATE_LAY_DOWN,                 // Lay down, body lowers closer to the ground to simulate resting
    DOG_STATE_SWAY,                     // Sway left and right, body or hips move side to side
    DOG_STATE_SHAKE_HAND,               // Shake hand action
    DOG_STATE_POKE,                     // Poke action
    DOG_STATE_SHAKE_BACK_LEGS,          // Shake back legs action
    DOG_STATE_JUMP_FORWARD,             // jump forward, body jumps forward
    DOG_STATE_JUMP_BACKWARD,            // Jump backward, body jumps backward
    DOG_STATE_RETRACT_LEGS,             // Retract legs, all four legs move back to the starting position
    DOG_STATE_MAX,                      // Total number of actions
} servo_dog_state_t;

typedef struct {
    servo_dog_state_t state;      // Action ID
    uint16_t repeat_count;        // Number of times to perform the action
    uint16_t speed;               // Speed of execution
    uint16_t hold_time_ms;        // Hold duration in milliseconds
    uint8_t angle_offset;         // Angle offset for the action
} dog_action_msg_t;

void servo_dog_ctrl_init(void);

void servo_dog_send_action(servo_dog_state_t state, int repeat_count, int speed, int hold_time_ms, int angle_offset);

#ifdef __cplusplus
}
#endif
