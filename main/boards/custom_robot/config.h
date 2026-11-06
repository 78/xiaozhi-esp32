#pragma once
#include <driver/gpio.h>

// --- MÀN HÌNH ST7789 (Giao tiếp SPI) ---
#define DISPLAY_SCL_PIN     GPIO_NUM_12
#define DISPLAY_SDA_PIN     GPIO_NUM_11
#define DISPLAY_RES_PIN     GPIO_NUM_10
#define DISPLAY_DC_PIN      GPIO_NUM_9

// --- MICROPHONE INMP441 (Bus I2S) ---
#define AUDIO_MIC_SCK_PIN   GPIO_NUM_14
#define AUDIO_MIC_WS_PIN    GPIO_NUM_21
#define AUDIO_MIC_SD_PIN    GPIO_NUM_13

// --- MẠCH ÂM THANH MAX98357 (Bus I2S) ---
#define AUDIO_SPK_LRC_PIN   GPIO_NUM_45
#define AUDIO_SPK_BCLK_PIN  GPIO_NUM_47
#define AUDIO_SPK_DIN_PIN   GPIO_NUM_48

// --- 2 ĐỘNG CƠ SERVO MG90S (PWM) ---
#define SERVO_PAN_PIN       GPIO_NUM_1  // Quay ngang (Trái / Phải)
#define SERVO_TILT_PIN      GPIO_NUM_2  // Gật đầu (Lên / Xuống)

// --- CẢM BIẾN CHUYỂN ĐỘNG PIR ---
#define PIR_SENSOR_PIN      GPIO_NUM_3
