/**
 * @file esp32_c6_devkitc1.cc
 * @brief Implementación de la placa ESP32-C6 DevKitC-1 para XiaoZhi AI
 *
 * Hardware:
 *   - ESP32-C6-DevKitC-1 (8MB Flash)
 *   - OLED SSD1306 128x64 por I2C (SDA=GPIO6, SCL=GPIO7)
 *   - Botón BOOT (GPIO9) — usado como botón de función
 *   - Micrófono I2S INMP441 (COMENTADO — conectar para habilitar)
 *     Si el INMP441 no está físicamente conectado y CONFIG_USE_INMP441_MIC
 *     está deshabilitado, el firmware arranca sin audio sin error fatal.
 *
 * Notas de pines ESP32-C6-DevKitC-1:
 *   GPIO6  → I2C SDA  (OLED / sensores)
 *   GPIO7  → I2C SCL  (OLED / sensores)
 *   GPIO9  → BOOT button (activo en bajo, pull-up interno)
 *   GPIO18 → LED RGB integrado (WS2812 en algunas revisiones)
 *   GPIO19 → USB D- (no usar para GPIO)
 *   GPIO20 → USB D+ (no usar para GPIO)
 *
 * Para habilitar el INMP441 en el futuro:
 *   1. Conectar: WS=GPIO4, SCK=GPIO5, SD=GPIO3
 *   2. Descomentar CONFIG_USE_INMP441_MIC en board_config.h
 *   3. Descomentar el bloque AudioCodec en este archivo
 */

#include "board.h"
#include "display/oled_display.h"
#include "button.h"
#include "led/single_led.h"
#include "application.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "board_config.h"

// ── Micrófono I2S INMP441 ─────────────────────────────────────────────────
// Descomentar la siguiente línea para habilitar el micrófono INMP441:
// #define CONFIG_USE_INMP441_MIC

#ifdef CONFIG_USE_INMP441_MIC
#include "audio_codecs/no_audio_codec.h"  // Reemplazar con i2s_mic si está disponible
#endif
// ──────────────────────────────────────────────────────────────────────────

static const char* TAG = "ESP32C6DevKitC1";

// ── Display I2C ───────────────────────────────────────────────────────────
// Se usa el driver I2C maestro de ESP-IDF v5.x (i2c_master)
static i2c_master_bus_handle_t i2c_bus_handle = nullptr;

static esp_err_t board_i2c_init(void)
{
    if (i2c_bus_handle != nullptr) {
        return ESP_OK;  // Ya inicializado
    }

    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port        = BOARD_I2C_PORT;
    bus_cfg.sda_io_num      = (gpio_num_t)BOARD_I2C_SDA;
    bus_cfg.scl_io_num      = (gpio_num_t)BOARD_I2C_SCL;
    bus_cfg.clk_source      = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error iniciando bus I2C: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Bus I2C OK — SDA=%d SCL=%d", BOARD_I2C_SDA, BOARD_I2C_SCL);
    return ESP_OK;
}
// ──────────────────────────────────────────────────────────────────────────


/**
 * @class Esp32C6DevKitC1Board
 * @brief Placa personalizada para ESP32-C6-DevKitC-1
 *
 * Hereda de Board y sobrescribe los métodos necesarios para
 * inicializar el display, botones, LED y (opcionalmente) audio.
 */
class Esp32C6DevKitC1Board : public Board {
private:
    OledDisplay*  display_  = nullptr;
    Button*       boot_btn_ = nullptr;
    SingleLed*    status_led_ = nullptr;

public:
    Esp32C6DevKitC1Board() {
        ESP_LOGI(TAG, "=== ESP32-C6-DevKitC-1 Board Init ===");
    }

    // ── Display ───────────────────────────────────────────────────────────
    Display* GetDisplay() override {
        if (display_ != nullptr) return display_;

        esp_err_t ret = board_i2c_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "No se pudo inicializar I2C — sin display");
            return nullptr;
        }

        // OledDisplay(bus, addr, width, height, mirror_x, mirror_y)
        display_ = new OledDisplay(
            i2c_bus_handle,
            BOARD_DISPLAY_I2C_ADDR,   // 0x3C
            BOARD_DISPLAY_WIDTH,      // 128
            BOARD_DISPLAY_HEIGHT,     // 64
            BOARD_DISPLAY_MIRROR_X,   // false
            BOARD_DISPLAY_MIRROR_Y    // false
        );

        if (display_ == nullptr) {
            ESP_LOGE(TAG, "Fallo al crear OledDisplay");
            return nullptr;
        }

        ESP_LOGI(TAG, "Display SSD1306 128x64 OK");
        return display_;
    }

    // ── Audio Codec ───────────────────────────────────────────────────────
    /**
     * Sin audio por defecto. El INMP441 (I2S) se agrega aquí cuando
     * esté físicamente conectado y CONFIG_USE_INMP441_MIC esté definido.
     *
     * Pines INMP441 (cuando se conecte):
     *   WS  → GPIO4
     *   SCK → GPIO5
     *   SD  → GPIO3
     *
     * Retorna nullptr — la aplicación maneja gracefully la ausencia de codec.
     */
    AudioCodec* GetAudioCodec() override {
#ifdef CONFIG_USE_INMP441_MIC
        // TODO: Instanciar el codec I2S para INMP441
        // Ejemplo (adaptar según la clase disponible en la versión del proyecto):
        //
        // static I2sInmp441AudioCodec codec(
        //     /* mck_io_num */   GPIO_NUM_NC,
        //     /* bck_io_num */   GPIO_NUM_5,
        //     /* ws_io_num  */   GPIO_NUM_4,
        //     /* data_in_num */  GPIO_NUM_3,
        //     /* data_out_num */ GPIO_NUM_NC,
        //     /* sample_rate */  16000,
        //     /* bits        */  16,
        //     /* mono        */  true
        // );
        // return &codec;
        ESP_LOGW(TAG, "INMP441 habilitado pero no implementado aún");
        return nullptr;
#else
        // Sin micrófono — operación normal sin audio
        ESP_LOGI(TAG, "Audio deshabilitado (sin micrófono)");
        return nullptr;
#endif
    }

    // ── Botones ───────────────────────────────────────────────────────────
    /**
     * Botón BOOT (GPIO9) — activo en bajo, pull-up interno.
     * Se usa como botón de función principal (activar asistente, etc.).
     */
    void InitializeButtons() {
        boot_btn_ = new Button(BOARD_BOOT_BUTTON_GPIO, /* active_level */ false);
        if (boot_btn_ == nullptr) {
            ESP_LOGE(TAG, "Error creando botón BOOT");
            return;
        }

        // Callback: presionar botón → activar/desactivar asistente
        boot_btn_->OnClick([this]() {
            ESP_LOGI(TAG, "Botón BOOT presionado");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.WakeWordDetected();
            } else {
                ESP_LOGI(TAG, "Asistente ya activo, ignorando clic");
            }
        });

        // Callback: mantener presionado 3s → resetear configuración WiFi
        boot_btn_->OnLongPress([this]() {
            ESP_LOGW(TAG, "Botón BOOT largo — Reseteando provisioning WiFi");
            auto& app = Application::GetInstance();
            app.ResetWifiProvisioning();
        });

        ESP_LOGI(TAG, "Botón BOOT (GPIO%d) registrado", BOARD_BOOT_BUTTON_GPIO);
    }

    // ── LED de estado ─────────────────────────────────────────────────────
    /**
     * LED integrado en GPIO18 (WS2812 en revisiones recientes del DevKitC-1).
     * Si la placa no tiene LED externo, se puede usar GPIO8 (LED azul en
     * algunas revisiones) o dejarlo como nullptr.
     */
    Led* GetLed() override {
#ifdef BOARD_STATUS_LED_GPIO
        if (status_led_ == nullptr) {
            status_led_ = new SingleLed(BOARD_STATUS_LED_GPIO);
        }
        return status_led_;
#else
        return nullptr;
#endif
    }

    // ── Inicialización general ────────────────────────────────────────────
    void Initialize() override {
        ESP_LOGI(TAG, "Inicializando ESP32-C6-DevKitC-1");

        // 1. Bus I2C para el display
        if (board_i2c_init() != ESP_OK) {
            ESP_LOGE(TAG, "Fallo crítico en I2C — continuando sin display");
        }

        // 2. Display
        GetDisplay();

        // 3. Botones
        InitializeButtons();

        // 4. LED
        GetLed();

        ESP_LOGI(TAG, "Inicialización completa");
        ESP_LOGI(TAG, "  Display : SSD1306 128x64 @ I2C 0x%02X", BOARD_DISPLAY_I2C_ADDR);
        ESP_LOGI(TAG, "  Botón   : GPIO%d (BOOT)", BOARD_BOOT_BUTTON_GPIO);
#ifdef BOARD_STATUS_LED_GPIO
        ESP_LOGI(TAG, "  LED     : GPIO%d", BOARD_STATUS_LED_GPIO);
#else
        ESP_LOGI(TAG, "  LED     : no configurado");
#endif
#ifdef CONFIG_USE_INMP441_MIC
        ESP_LOGI(TAG, "  Mic     : INMP441 (habilitado, WS=GPIO4 SCK=GPIO5 SD=GPIO3)");
#else
        ESP_LOGI(TAG, "  Mic     : no conectado (deshabilitado)");
#endif
    }
};


// ── Punto de entrada: create_board() ─────────────────────────────────────
/**
 * Función que el framework de XiaoZhi llama al inicio.
 * DEBE estar en el espacio de nombres global y sin decoración de nombre C++.
 */
extern "C" Board* create_board()
{
    return new Esp32C6DevKitC1Board();
}
// ──────────────────────────────────────────────────────────────────────────
