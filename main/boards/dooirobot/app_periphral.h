#ifndef __APP_PERIPHRAL_H__
#define __APP_PERIPHRAL_H__

#include "app_servo.h"
#include "app_motor.h"
#include "app_led.h"
#include "app_action_seq.h"
#include "power_manager.h"
#include "touch_button.h"
#include "app_imu.h"
#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_tip_led_run();

void app_periphral_init();

#ifdef __cplusplus
}
#endif

#endif
