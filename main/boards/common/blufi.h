#pragma once

#include <aes/esp_aes.h>
#include <cassert>
#include <cstring>
#include <vector>
#include "esp_blufi_api.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "wifi_manager.h"

class Blufi {
public:
    /**
     * @brief Get the singleton instance of the Blufi class.
     */
    static Blufi &GetInstance();

    /**
     * @brief Start WiFi scan for Blufi provisioning
     * This method intelligently handles WiFi scanning based on current WiFi state:
     * - If WiFi config mode is active, it uses the existing scan results from WifiConfigurationAp
     * - Otherwise, it performs a dedicated scan without interfering with normal WiFi operations
     */
    void start_wifi_scan();

    /**
     * @brief Initializes the Bluetooth controller, host, and Blufi profile.
     * This is the main entry point to start the Blufi process.
     * @return ESP_OK on success, otherwise an error code.
     */
    esp_err_t init();

    /**
     * @brief Deinitializes Blufi and the Bluetooth stack.
     * @return ESP_OK on success, otherwise an error code.
     */
    esp_err_t deinit();

    // Delete copy constructor and assignment operator for singleton
    Blufi(const Blufi &) = delete;

    Blufi &operator=(const Blufi &) = delete;

private:
    bool inited_ = false;

    Blufi();

    ~Blufi();

    // Initialization logic
    static esp_err_t _controller_init();

    static esp_err_t _controller_deinit();

    static esp_err_t _host_init();

    static esp_err_t _host_deinit();

    static esp_err_t _gap_register_callback();

    static esp_err_t _host_and_cb_init();

    void _security_init();

    void _security_deinit();

    void _dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len,
                                    bool *need_free);

    int _aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    int _aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    static uint16_t _crc_checksum(uint8_t iv8, uint8_t *data, int len);

    void _handle_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

    static int _get_softap_conn_num();

    // WiFi scan methods
    void _send_wifi_list();
    void _start_dedicated_wifi_scan();
    static void _wifi_scan_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                         void *event_data);

    // These C-style functions are registered with ESP-IDF and call the corresponding instance
    // methods.

    static void _event_callback_trampoline(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

    static void _negotiate_data_handler_trampoline(uint8_t *data, int len, uint8_t **output_data,
                                                   int *output_len, bool *need_free);

    static int _encrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    static int _decrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

    static uint16_t _checksum_func_trampoline(uint8_t iv8, uint8_t *data, int len);

#ifdef CONFIG_BT_NIMBLE_ENABLED
    static void _nimble_on_reset(int reason);
    static void _nimble_on_sync();
    static void _nimble_host_task(void *param);
#endif

    // Security context, formerly blufi_sec struct
    struct BlufiSecurity {
#define DH_SELF_PUB_KEY_LEN 128
        uint8_t self_public_key[DH_SELF_PUB_KEY_LEN];
#define SHARE_KEY_LEN 128
        uint8_t share_key[SHARE_KEY_LEN];
        size_t share_len;
#define PSK_LEN 16
        uint8_t psk[PSK_LEN];
        uint8_t *dh_param;
        int dh_param_len;
        uint8_t iv[16];
        mbedtls_dhm_context *dhm;
        esp_aes_context *aes;
    };

    BlufiSecurity *m_sec;

    // State variables
    wifi_config_t m_sta_config{};
    bool m_ble_is_connected;
    bool m_sta_connected;
    bool m_sta_got_ip;
    bool m_provisioned;
    bool m_deinited;
    uint8_t m_sta_bssid[6]{};
    uint8_t m_sta_ssid[32]{};
    int m_sta_ssid_len;
    bool m_sta_is_connecting;
    esp_blufi_extra_info_t m_sta_conn_info{};

    // WiFi scan related
    std::vector<wifi_ap_record_t> m_ap_records;
    bool m_scan_in_progress = false;
    bool m_scan_should_save_ssid = true;
};
