/**
 * @file board_config.h
 * @brief Definición de pines y parámetros hardware para ESP32-C6-DevKitC-1
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │  MAPA DE PINES — ESP32-C6-DevKitC-1                        │
 * ├──────────┬──────────┬───────────────────────────────────────┤
 * │  GPIO   │  Función │  Notas                                │
 * ├──────────┼──────────┼───────────────────────────────────────┤
 * │  GPIO6  │ I2C SDA  │  OLED SSD1306 (y otros dispositivos) │
 * │  GPIO7  │ I2C SCL  │  OLED SSD1306 (y otros dispositivos) │
 * │  GPIO9  │ BOOT BTN │  Activo en bajo, pull-up interno      │
 * │  GPIO18 │ LED RGB  │  WS2812 en algunas revisiones         │
 * ├──────────┼──────────┼───────────────────────────────────────┤
 * │  INMP441 I2S (COMENTADO — sin conectar)                    │
 * │  GPIO3  │ I2S SD   │  Datos de entrada del micrófono       │
 * │  GPIO4  │ I2S WS   │  Word Select (L/R clock)              │
 * │  GPIO5  │ I2S SCK  │  Bit Clock                            │
 * └──────────┴──────────┴───────────────────────────────────────┘
 *
 * PINES A EVITAR en ESP32-C6-DevKitC-1:
 *   GPIO19, GPIO20 → USB D-/D+ (comunicación con PC)
 *   GPIO11, GPIO12, GPIO13, GPIO14, GPIO15, GPIO24, GPIO25 → Flash SPI
 */

#pragma once

// ════════════════════════════════════════════════════════════════
//  I2C — Display OLED SSD1306
// ════════════════════════════════════════════════════════════════
#define BOARD_I2C_PORT          I2C_NUM_0
#define BOARD_I2C_SDA           6       // GPIO6
#define BOARD_I2C_SCL           7       // GPIO7
#define BOARD_I2C_FREQ_HZ       400000  // 400 kHz (Fast Mode)

// Display SSD1306 128×64
#define BOARD_DISPLAY_I2C_ADDR  0x3C    // Dirección estándar SSD1306 (0x3C o 0x3D)
#define BOARD_DISPLAY_WIDTH     128
#define BOARD_DISPLAY_HEIGHT    64
#define BOARD_DISPLAY_MIRROR_X  false
#define BOARD_DISPLAY_MIRROR_Y  false

// ════════════════════════════════════════════════════════════════
//  Botones
// ════════════════════════════════════════════════════════════════
#define BOARD_BOOT_BUTTON_GPIO  9       // GPIO9 — Botón BOOT del DevKitC-1

// ════════════════════════════════════════════════════════════════
//  LED de estado
// ════════════════════════════════════════════════════════════════
// GPIO18 → LED RGB / WS2812 en el ESP32-C6-DevKitC-1
// Comentar si la revisión de la placa no tiene LED integrado
#define BOARD_STATUS_LED_GPIO   18

// ════════════════════════════════════════════════════════════════
//  Micrófono I2S INMP441 — SIN CONECTAR
//  ⚠  Estas definiciones están COMENTADAS porque el INMP441
//     no está físicamente conectado al ESP32-C6-DevKitC-1.
//  Para habilitar en el futuro:
//    1. Conectar el INMP441 a los pines indicados.
//    2. Descomentar CONFIG_USE_INMP441_MIC en esp32_c6_devkitc1.cc
//    3. Descomentar las siguientes líneas.
// ════════════════════════════════════════════════════════════════
// #define BOARD_INMP441_I2S_NUM     I2S_NUM_0
// #define BOARD_INMP441_GPIO_SD     3     // GPIO3 — datos (DOUT del INMP441)
// #define BOARD_INMP441_GPIO_WS     4     // GPIO4 — word select
// #define BOARD_INMP441_GPIO_SCK    5     // GPIO5 — bit clock
// #define BOARD_INMP441_SAMPLE_RATE 16000 // 16 kHz recomendado para reconocimiento
// #define BOARD_INMP441_BITS        16    // 16 bits por muestra
// #define BOARD_INMP441_MONO        true  // El INMP441 es mono

// ════════════════════════════════════════════════════════════════
//  Speaker / Amplificador — NO CONECTADO
//  Comentar / habilitar si se agrega un amplificador I2S externo
// ════════════════════════════════════════════════════════════════
// #define BOARD_SPEAKER_BCLK        GPIO_NUM_NC
// #define BOARD_SPEAKER_WS          GPIO_NUM_NC
// #define BOARD_SPEAKER_DOUT        GPIO_NUM_NC

// ════════════════════════════════════════════════════════════════
//  Identificación de placa
// ════════════════════════════════════════════════════════════════
#define BOARD_NAME              "ESP32-C6-DevKitC-1"
#define BOARD_FLASH_SIZE_MB     8
