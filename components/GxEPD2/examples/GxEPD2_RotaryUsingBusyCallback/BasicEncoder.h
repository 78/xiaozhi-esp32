/*
  Copyright 2021 Peter Harrison

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      https://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  ------------------------------------------------------------------------------

  BasicEncoder provides a class for reading a rotary encoder knob.
  It is not suited to motor encoders.
  The switch, if present, needs a deparate button library .

  Decoding logic based on https://www.mikrocontroller.net/articles/Drehgeber
*/

// original source from: https://github.com/micromouseonline/BasicEncoder
//
// adapted for GxEPD2_RotaryUsingBusyCallback.ino example use, without interrupts on any processor
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2


#ifndef BASIC_ENCODER_H_
#define BASIC_ENCODER_H_

#include <Arduino.h>

class BasicEncoder {
  public:
    BasicEncoder(int8_t pinA, int8_t pinB, uint8_t active_state = LOW, uint8_t steps = 4)
      : m_pin_a(pinA), m_pin_b(pinB), m_pin_active(active_state), m_steps_per_count(steps) {
      pinMode(pinA, INPUT_PULLUP);
      pinMode(pinB, INPUT_PULLUP);
      m_change = 0;
    }
    ~BasicEncoder() {}

    void begin() {
      reset();
    }

    int8_t pin_state() {
      int8_t state_now = 0;
      if (digitalRead(m_pin_a) == m_pin_active) {
        state_now |= 2;
      }
      if (digitalRead(m_pin_b) == m_pin_active) {
        state_now |= 1;
      }
      //if (state_now) Serial.println(state_now);
      return state_now;
    }

    // to update the encoder changes
    // call this method in a timmer interrupt for best performance
    // it could also be called in the main loop
    // this takes about 10-15us using digitalRead on Arduino Nano
    void service() {
      int8_t state_now = pin_state();
      state_now ^= state_now >> 1;  // two bit gray-to-binary
      static int8_t previous_state = state_now;
      int8_t difference = previous_state - state_now;
      // bit 1 has the direction, bit 0 is set if changeed
      if (difference & 1) {
        previous_state = state_now;
        int delta = (difference & 2) - 1;
        if (m_reversed) {
          delta = -delta;
        }
        m_change += delta;
        m_steps += delta;
      }
    }

    /****************************************************************

      // try this if there are unwanted transitions at the detents
      // it is half resolution so counts are doubled
      const int8_t decode_table[16] PROGMEM = {0, 0, -2, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, -2, 0, 0};
      void flaky_encoder_service(void) {
      // just get the bits - no gray-to-binary conversion needed
      int8_t state_now = pin_state();
      static int8_t encoder_state = state_now;
      encoder_state = ((encoder_state << 2) | state_now) & 0x0f;
      encoder_change += (int8_t)pgm_read_byte(&decode_table[encoder_state]);
      }

    ****************************************************************/

    // Read changes frequently enough that overflows cannot happen.
    int8_t get_change() {
      //uint8_t sreg = SREG;
      //noInterrupts();
      int8_t change = m_change;
      // the switch statement can make better code because only optimised
      // operations are used instead of generic division
      switch (m_steps_per_count) {
        case 4:
          m_change %= 4;
          change /= 4;
          break;
        case 2:
          m_change %= 2;
          change /= 2;
          break;
        default:
          m_change = 0;
          break;
      }
      //SREG = sreg;
      return change;
    }

    int get_count() {
      //uint8_t sreg = SREG;
      //noInterrupts();
      int count = m_steps / m_steps_per_count;
      //SREG = sreg;
      return count;
    }

    void reset() {
      //uint8_t sreg = SREG;
      //noInterrupts();
      m_steps = 0;
      m_change = 0;
      //SREG = sreg;
    }

    void set_reverse() {
      m_reversed = true;
    }

    void set_forward() {
      m_reversed = false;
    }

  private:
    int8_t m_pin_a = 0;
    int8_t m_pin_b = 0;
    uint8_t m_pin_active = LOW;
    uint8_t m_steps_per_count = 4;
    bool m_reversed = false;
    volatile int m_change = 0;
    int m_steps = 0;
};
#endif  // BASIC_ENCODER_H_
