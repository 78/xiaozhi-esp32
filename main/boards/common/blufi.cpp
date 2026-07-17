#include "blufi.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include "esp_bt.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "wifi_manager.h"

#define BLUFI_DEVICE_NAME "Xiaozhi-Blufi"

#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "console/console.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
extern void esp_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg);
extern int esp_blufi_gatt_svr_init(void);
extern void esp_blufi_gatt_svr_deinit(void);
extern void esp_blufi_btc_init(void);
extern void esp_blufi_btc_deinit(void);
#endif

extern "C" {
void esp_blufi_adv_start(void);

void esp_blufi_adv_stop(void);

void esp_blufi_disconnect(void);

void btc_blufi_report_error(esp_blufi_error_state_t state);

#ifdef CONFIG_BT_BLUEDROID_ENABLED
void esp_blufi_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
void esp_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg);
int esp_blufi_gatt_svr_init(void);
void esp_blufi_gatt_svr_deinit(void);
void esp_blufi_btc_init(void);
void esp_blufi_btc_deinit(void);
#endif
}

#include <wifi_station.h>
#include "esp_crc.h"
#include "ssid_manager.h"

static const char* BLUFI_TAG = "BLUFI_CLASS";

static wifi_mode_t GetWifiModeWithFallback(const WifiManager& wifi) {
    if (wifi.IsConfigMode()) {
        return WIFI_MODE_AP;
    }
    if (wifi.IsInitialized() && wifi.IsConnected()) {
        return WIFI_MODE_STA;
    }

    wifi_mode_t mode = WIFI_MODE_STA;
    esp_wifi_get_mode(&mode);
    return mode;
}

Blufi& Blufi::GetInstance() {
    static Blufi instance;
    return instance;
}

Blufi::Blufi()
    : m_sec(nullptr),
      m_ble_is_connected(false),
      m_sta_connected(false),
      m_sta_got_ip(false),
      m_provisioned(false),
      m_deinited(false),
      m_sta_ssid_len(0),
      m_sta_is_connecting(false) {
    memset(&m_sta_config, 0, sizeof(m_sta_config));
    memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
    memset(m_sta_ssid, 0, sizeof(m_sta_ssid));
    memset(&m_sta_conn_info, 0, sizeof(m_sta_conn_info));
}

Blufi::~Blufi() {
    if (m_sec) {
        _security_deinit();
    }
}

esp_err_t Blufi::init() {
    esp_err_t ret = ESP_FAIL;
    inited_ = true;
    m_provisioned = false;
    m_deinited = false;

    // Start WiFi scan early to have results ready when user connects
    auto& wifi_manager = WifiManager::GetInstance();
    if (!wifi_manager.IsInitialized() || !wifi_manager.IsConfigMode()) {
        // start scan immediately
        start_wifi_scan();
    } else {
        ESP_LOGE(BLUFI_TAG,
                 "Blufi and WiFi hotspot network configuration cannot "
                 "be used simultaneously.");
        return ret;
    }

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    ret = _controller_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "BLUFI controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    ret = _host_and_cb_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "BLUFI host and cb init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(BLUFI_TAG, "BLUFI VERSION %04x", esp_blufi_get_version());
    return ESP_OK;
}

esp_err_t Blufi::deinit() {
    esp_err_t ret = ESP_OK;

    if (inited_) {
        if (m_deinited) {
            return ESP_OK;
        }
        m_deinited = true;
        ret = _host_deinit();
        if (ret) {
            ESP_LOGE(BLUFI_TAG, "Host deinit failed: %s", esp_err_to_name(ret));
        }
#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
        ret = _controller_deinit();
        if (ret) {
            ESP_LOGE(BLUFI_TAG, "Controller deinit failed: %s", esp_err_to_name(ret));
        }
#endif
    }
    return ret;
}

#ifdef CONFIG_BT_BLUEDROID_ENABLED
esp_err_t Blufi::_host_init() {
    esp_err_t ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s init bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ESP_LOGI(BLUFI_TAG, "BD ADDR: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
    return ESP_OK;
}

esp_err_t Blufi::_host_deinit() {
    esp_err_t ret = esp_blufi_profile_deinit();
    if (ret != ESP_OK)
        return ret;

    ret = esp_bluedroid_disable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s disable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ret = esp_bluedroid_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s deinit bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Blufi::_gap_register_callback() {
    esp_err_t rc = esp_ble_gap_register_callback(esp_blufi_gap_event_handler);
    if (rc) {
        return rc;
    }
    return esp_blufi_profile_init();
}

esp_err_t Blufi::_host_and_cb_init() {
    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = &_event_callback_trampoline,
        .negotiate_data_handler = &_negotiate_data_handler_trampoline,
        .encrypt_func = &_encrypt_func_trampoline,
        .decrypt_func = &_decrypt_func_trampoline,
        .checksum_func = &_checksum_func_trampoline,
    };

    esp_err_t ret = _host_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialise host failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s blufi register failed, error code = %x", __func__, ret);
        return ret;
    }
    ret = _gap_register_callback();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s gap register failed, error code = %x", __func__, ret);
        return ret;
    }
    return ESP_OK;
}
#endif /* CONFIG_BT_BLUEDROID_ENABLED */

#ifdef CONFIG_BT_NIMBLE_ENABLED
// Stubs for NimBLE specific store functionality
void ble_store_config_init();

void Blufi::_nimble_on_reset(int reason) {
    ESP_LOGE(BLUFI_TAG, "NimBLE Resetting state; reason=%d", reason);
}

void Blufi::_nimble_on_sync() { esp_blufi_profile_init(); }

void Blufi::_nimble_host_task(void* param) {
    ESP_LOGI(BLUFI_TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t Blufi::_host_init() {
    ble_hs_cfg.reset_cb = _nimble_on_reset;
    ble_hs_cfg.sync_cb = _nimble_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;

    ble_hs_cfg.sm_io_cap = 4;
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif

    int rc = esp_blufi_gatt_svr_init();
    assert(rc == 0);

    ble_store_config_init();
    esp_blufi_btc_init();

    esp_err_t err = esp_nimble_enable(_nimble_host_task);
    if (err) {
        ESP_LOGE(BLUFI_TAG, "%s failed: %s", __func__, esp_err_to_name(err));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Blufi::_host_deinit(void) {
    esp_err_t ret = nimble_port_stop();
    if (ret == ESP_OK) {
        esp_nimble_deinit();
    }
    esp_blufi_gatt_svr_deinit();
    ret = esp_blufi_profile_deinit();
    esp_blufi_btc_deinit();
    return ret;
}

esp_err_t Blufi::_gap_register_callback(void) { return ESP_OK; }

esp_err_t Blufi::_host_and_cb_init() {
    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = &_event_callback_trampoline,
        .negotiate_data_handler = &_negotiate_data_handler_trampoline,
        .encrypt_func = &_encrypt_func_trampoline,
        .decrypt_func = &_decrypt_func_trampoline,
        .checksum_func = &_checksum_func_trampoline,
    };

    esp_err_t ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s blufi register failed, error code = %x", __func__, ret);
        return ret;
    }

    // Host init must be called after registering callbacks for NimBLE
    ret = _host_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialise host failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}
#endif /* CONFIG_BT_NIMBLE_ENABLED */

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
esp_err_t Blufi::_controller_init() {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

#ifdef CONFIG_BT_NIMBLE_ENABLED
    ret = esp_nimble_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "esp_nimble_init() failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif
    return ESP_OK;
}

esp_err_t Blufi::_controller_deinit() {
    esp_err_t ret = esp_bt_controller_disable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s disable controller failed: %s", __func__, esp_err_to_name(ret));
    }
    ret = esp_bt_controller_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s deinit controller failed: %s", __func__, esp_err_to_name(ret));
    }
    return ret;
}
#endif

namespace {

constexpr uint8_t kDhParamLength = 0x00;
constexpr uint8_t kDhParamData = 0x01;
constexpr size_t kBlufiIvSize = 16;
constexpr size_t kBlufiHashSize = 32;
constexpr char kBlufiEncryptDomain[] = "blufi_enc";
constexpr char kBlufiDecryptDomain[] = "blufi_dec";
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
constexpr auto kBlufiHashError = ESP_BLUFI_CALC_SHA_256_ERROR;
#else
// IDF 5 does not expose a SHA-256-specific BluFi error code.
constexpr auto kBlufiHashError = ESP_BLUFI_CALC_MD5_ERROR;
#endif

}  // namespace

void Blufi::_security_init() {
    _security_deinit();

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(BLUFI_TAG, "psa_crypto_init failed: %d", status);
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    m_sec = new BlufiSecurity();
    if (m_sec == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Failed to allocate security context");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }
    memset(m_sec, 0, sizeof(BlufiSecurity));
    m_sec->enc_operation = psa_cipher_operation_init();
    m_sec->dec_operation = psa_cipher_operation_init();
}

void Blufi::_security_deinit() {
    if (m_sec == nullptr) {
        return;
    }

    psa_cipher_abort(&m_sec->enc_operation);
    psa_cipher_abort(&m_sec->dec_operation);
    if (m_sec->aes_key != PSA_KEY_ID_NULL) {
        psa_destroy_key(m_sec->aes_key);
    }
    free(m_sec->dh_param);
    memset(m_sec, 0, sizeof(BlufiSecurity));
    delete m_sec;
    m_sec = nullptr;
}

void Blufi::_dh_negotiate_data_handler(uint8_t* data, int len, uint8_t** output_data,
                                       int* output_len, bool* need_free) {
    if (m_sec == nullptr || data == nullptr || output_data == nullptr || output_len == nullptr ||
        need_free == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Security not initialized in DH handler");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    if (len < 3) {
        ESP_LOGE(BLUFI_TAG, "DH handler: data too short");
        btc_blufi_report_error(ESP_BLUFI_DATA_FORMAT_ERROR);
        return;
    }

    uint8_t type = data[0];
    switch (type) {
        case kDhParamLength:
            m_sec->dh_param_len = (data[1] << 8) | data[2];
            if (m_sec->dh_param_len <= 0 || m_sec->dh_param_len > DH_PARAM_LEN_MAX) {
                ESP_LOGE(BLUFI_TAG, "Invalid DH parameter length: %d", m_sec->dh_param_len);
                m_sec->dh_param_len = 0;
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }

            free(m_sec->dh_param);
            m_sec->dh_param = nullptr;
            psa_cipher_abort(&m_sec->enc_operation);
            psa_cipher_abort(&m_sec->dec_operation);
            m_sec->enc_operation = psa_cipher_operation_init();
            m_sec->dec_operation = psa_cipher_operation_init();
            if (m_sec->aes_key != PSA_KEY_ID_NULL) {
                psa_destroy_key(m_sec->aes_key);
                m_sec->aes_key = PSA_KEY_ID_NULL;
            }

            m_sec->dh_param = (uint8_t*)malloc(m_sec->dh_param_len);
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH malloc failed");
                m_sec->dh_param_len = 0;
                btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
                return;
            }
            break;
        case kDhParamData: {
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH param not allocated");
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
            if (len != m_sec->dh_param_len + 1) {
                ESP_LOGE(BLUFI_TAG, "Invalid DH parameter packet length: %d, expected %d", len,
                         m_sec->dh_param_len + 1);
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }

            memcpy(m_sec->dh_param, &data[1], m_sec->dh_param_len);

            const uint8_t* param = m_sec->dh_param;
            const uint8_t* const end = param + m_sec->dh_param_len;
            auto read_field = [&param, end](const uint8_t** value, size_t* value_len) -> bool {
                if (end - param < 2) {
                    return false;
                }
                *value_len = (static_cast<size_t>(param[0]) << 8) | param[1];
                param += 2;
                if (*value_len == 0 || static_cast<size_t>(end - param) < *value_len) {
                    return false;
                }
                *value = param;
                param += *value_len;
                return true;
            };

            const uint8_t* prime = nullptr;
            const uint8_t* generator = nullptr;
            const uint8_t* peer_public_key = nullptr;
            size_t prime_len = 0;
            size_t generator_len = 0;
            size_t peer_public_key_len = 0;
            if (!read_field(&prime, &prime_len) || !read_field(&generator, &generator_len) ||
                !read_field(&peer_public_key, &peer_public_key_len) || param != end) {
                ESP_LOGE(BLUFI_TAG, "Malformed DH parameter data");
                btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
                return;
            }

            // PSA FFDH supports named RFC 7919 groups instead of caller-supplied P/G. The IDF 6
            // BluFi protocol uses ffdhe3072; P and G remain in the packet for framing compatibility.
            constexpr size_t kDhKeyBits = 3072;
            constexpr size_t kDhKeyBytes = kDhKeyBits / 8;
            if (prime_len != kDhKeyBytes || peer_public_key_len != kDhKeyBytes ||
                generator_len != 1 || generator[0] != 2) {
                ESP_LOGE(BLUFI_TAG,
                         "Unsupported DH group (P=%u, G=%u, public=%u); ffdhe3072 is required",
                         static_cast<unsigned>(prime_len), static_cast<unsigned>(generator_len),
                         static_cast<unsigned>(peer_public_key_len));
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }

            psa_key_attributes_t attributes = psa_key_attributes_init();
            psa_set_key_type(&attributes, PSA_KEY_TYPE_DH_KEY_PAIR(PSA_DH_FAMILY_RFC7919));
            psa_set_key_bits(&attributes, kDhKeyBits);
            psa_set_key_algorithm(&attributes, PSA_ALG_FFDH);
            psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);

            psa_key_id_t private_key = PSA_KEY_ID_NULL;
            psa_status_t status = psa_generate_key(&attributes, &private_key);
            psa_reset_key_attributes(&attributes);
            if (status != PSA_SUCCESS) {
                ESP_LOGE(BLUFI_TAG, "psa_generate_key failed: %d", status);
                btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }

            size_t public_key_len = 0;
            status = psa_export_public_key(private_key, m_sec->self_public_key,
                                           sizeof(m_sec->self_public_key), &public_key_len);
            if (status != PSA_SUCCESS || public_key_len != kDhKeyBytes) {
                ESP_LOGE(BLUFI_TAG, "psa_export_public_key failed: %d, length: %u", status,
                         static_cast<unsigned>(public_key_len));
                psa_destroy_key(private_key);
                btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }

            status = psa_raw_key_agreement(PSA_ALG_FFDH, private_key, peer_public_key,
                                           peer_public_key_len, m_sec->share_key,
                                           sizeof(m_sec->share_key), &m_sec->share_len);
            psa_destroy_key(private_key);
            if (status != PSA_SUCCESS) {
                ESP_LOGE(BLUFI_TAG, "psa_raw_key_agreement failed: %d", status);
                btc_blufi_report_error(ESP_BLUFI_ENCRYPT_ERROR);
                return;
            }

            size_t hash_len = 0;
            status = psa_hash_compute(PSA_ALG_SHA_256, m_sec->share_key, m_sec->share_len,
                                      m_sec->psk, sizeof(m_sec->psk), &hash_len);
            if (status != PSA_SUCCESS || hash_len != sizeof(m_sec->psk)) {
                ESP_LOGE(BLUFI_TAG, "psa_hash_compute failed: %d", status);
                btc_blufi_report_error(kBlufiHashError);
                return;
            }

            attributes = psa_key_attributes_init();
            psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
            psa_set_key_bits(&attributes, PSK_LEN * 8);
            psa_set_key_algorithm(&attributes, PSA_ALG_CTR);
            psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
            status = psa_import_key(&attributes, m_sec->psk, sizeof(m_sec->psk), &m_sec->aes_key);
            psa_reset_key_attributes(&attributes);
            if (status != PSA_SUCCESS) {
                ESP_LOGE(BLUFI_TAG, "psa_import_key failed: %d", status);
                btc_blufi_report_error(ESP_BLUFI_ENCRYPT_ERROR);
                return;
            }

            auto setup_cipher = [this](psa_cipher_operation_t* operation, const char* domain) {
                uint8_t material[sizeof(kBlufiEncryptDomain) - 1 + SHARE_KEY_LEN];
                memcpy(material, domain, sizeof(kBlufiEncryptDomain) - 1);
                memcpy(material + sizeof(kBlufiEncryptDomain) - 1, m_sec->share_key,
                       m_sec->share_len);
                uint8_t hash[kBlufiHashSize];
                size_t hash_len = 0;
                psa_status_t status = psa_hash_compute(
                    PSA_ALG_SHA_256, material,
                    sizeof(kBlufiEncryptDomain) - 1 + m_sec->share_len, hash, sizeof(hash),
                    &hash_len);
                memset(material, 0, sizeof(material));
                if (status != PSA_SUCCESS || hash_len != sizeof(hash)) {
                    return status == PSA_SUCCESS ? PSA_ERROR_GENERIC_ERROR : status;
                }
                *operation = psa_cipher_operation_init();
                status = psa_cipher_encrypt_setup(operation, m_sec->aes_key, PSA_ALG_CTR);
                if (status == PSA_SUCCESS) {
                    status = psa_cipher_set_iv(operation, hash, kBlufiIvSize);
                }
                memset(hash, 0, sizeof(hash));
                return status;
            };

            status = setup_cipher(&m_sec->enc_operation, kBlufiEncryptDomain);
            if (status == PSA_SUCCESS) {
                // CTR decryption uses the same cipher primitive with an independent counter.
                status = setup_cipher(&m_sec->dec_operation, kBlufiDecryptDomain);
            }
            if (status != PSA_SUCCESS) {
                ESP_LOGE(BLUFI_TAG, "PSA cipher setup failed: %d", status);
                psa_cipher_abort(&m_sec->enc_operation);
                psa_cipher_abort(&m_sec->dec_operation);
                psa_destroy_key(m_sec->aes_key);
                m_sec->aes_key = PSA_KEY_ID_NULL;
                btc_blufi_report_error(ESP_BLUFI_ENCRYPT_ERROR);
                return;
            }

            *output_data = m_sec->self_public_key;
            *output_len = public_key_len;
            *need_free = false;
            ESP_LOGI(BLUFI_TAG, "DH negotiation completed successfully");

            free(m_sec->dh_param);
            m_sec->dh_param = nullptr;
            m_sec->dh_param_len = 0;
            break;
        }
        default:
            ESP_LOGE(BLUFI_TAG, "DH handler unknown type: %d", type);
            btc_blufi_report_error(ESP_BLUFI_DATA_FORMAT_ERROR);
    }
}

int Blufi::_aes_encrypt(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    (void)iv8;
    if (!m_sec || m_sec->aes_key == PSA_KEY_ID_NULL || !crypt_data || crypt_len < 0) {
        ESP_LOGE(BLUFI_TAG, "Invalid parameters for AES encryption");
        return -ESP_ERR_INVALID_ARG;
    }
    if (crypt_len == 0) {
        return 0;
    }

    std::vector<uint8_t> output(PSA_CIPHER_ENCRYPT_OUTPUT_SIZE(
        PSA_KEY_TYPE_AES, PSA_ALG_CTR, static_cast<size_t>(crypt_len)));
    size_t output_len = 0;
    psa_status_t status = psa_cipher_update(&m_sec->enc_operation, crypt_data, crypt_len,
                                            output.data(), output.size(), &output_len);
    if (status != PSA_SUCCESS || output_len != static_cast<size_t>(crypt_len)) {
        ESP_LOGE(BLUFI_TAG, "AES encrypt failed: %d, length: %u", status,
                 static_cast<unsigned>(output_len));
        return status == PSA_SUCCESS ? -ESP_FAIL : status;
    }
    memcpy(crypt_data, output.data(), output_len);
    return static_cast<int>(output_len);
}

int Blufi::_aes_decrypt(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    (void)iv8;
    if (!m_sec || m_sec->aes_key == PSA_KEY_ID_NULL || !crypt_data || crypt_len < 0) {
        ESP_LOGE(BLUFI_TAG, "Invalid parameters for AES decryption");
        return -ESP_ERR_INVALID_ARG;
    }
    if (crypt_len == 0) {
        return 0;
    }

    std::vector<uint8_t> output(PSA_CIPHER_DECRYPT_OUTPUT_SIZE(
        PSA_KEY_TYPE_AES, PSA_ALG_CTR, static_cast<size_t>(crypt_len)));
    size_t output_len = 0;
    psa_status_t status = psa_cipher_update(&m_sec->dec_operation, crypt_data, crypt_len,
                                            output.data(), output.size(), &output_len);
    if (status != PSA_SUCCESS || output_len != static_cast<size_t>(crypt_len)) {
        ESP_LOGE(BLUFI_TAG, "AES decrypt failed: %d, length: %u", status,
                 static_cast<unsigned>(output_len));
        return status == PSA_SUCCESS ? -ESP_FAIL : status;
    }
    memcpy(crypt_data, output.data(), output_len);
    return static_cast<int>(output_len);
}

uint16_t Blufi::_crc_checksum(uint8_t iv8, uint8_t* data, int len) {
    return esp_crc16_be(0, data, len);
}

int Blufi::_get_softap_conn_num() {
    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsInitialized() || !wifi.IsConfigMode()) {
        return 0;
    }

    wifi_sta_list_t sta_list{};
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}

bool Blufi::start_wifi_scan() {
    ESP_LOGI(BLUFI_TAG, "Starting dedicated WiFi scan");

    // Already running: caller can rely on the in-flight scan and await its done event.
    if (m_scan_in_progress) {
        ESP_LOGW(BLUFI_TAG, "Scan already in progress, skipping");
        return true;
    }

    m_scan_in_progress = true;

    // Get current WiFi mode
    wifi_mode_t current_mode;
    esp_err_t err = esp_wifi_get_mode(&current_mode);

    if (current_mode == WIFI_MODE_AP) {
        // If in AP mode, temporarily switch to APSTA to allow scanning
        ESP_LOGI(BLUFI_TAG, "WiFi in AP mode");
        err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            ESP_LOGE(BLUFI_TAG, "Failed to set WiFi mode to STA: %s", esp_err_to_name(err));
            m_scan_in_progress = false;
            return false;
        }
        // Need to restart WiFi for mode change to take effect
        err = esp_wifi_start();
        if (err != ESP_OK) {
            ESP_LOGE(BLUFI_TAG, "Failed to start WiFi after mode switch: %s", esp_err_to_name(err));
            m_scan_in_progress = false;
            return false;
        }
        // Register scan event handler
        esp_event_handler_instance_t scan_event_instance;
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &Blufi::_wifi_scan_event_handler, this,
                                            &scan_event_instance);

        // Start scan
        err = esp_wifi_scan_start(NULL, false);
        if (err != ESP_OK) {
            ESP_LOGE(BLUFI_TAG, "Failed to start WiFi scan: %s", esp_err_to_name(err));
            m_scan_in_progress = false;
            return false;
        }
    } else if (current_mode == WIFI_MODE_STA || current_mode == WIFI_MODE_APSTA) {
        // Ensure WiFi driver is started (may have been stopped during config mode transition)
        err = esp_wifi_start();
        if (err != ESP_OK && err != ESP_ERR_WIFI_STATE) {
            ESP_LOGE(BLUFI_TAG, "Failed to start WiFi before scan: %s", esp_err_to_name(err));
            m_scan_in_progress = false;
            return false;
        }
        err = esp_wifi_scan_start(NULL, false);
        if (err != ESP_OK) {
            ESP_LOGE(BLUFI_TAG, "Failed to start WiFi scan: %s", esp_err_to_name(err));
            m_scan_in_progress = false;
            return false;
        }
    } else {
        ESP_LOGE(BLUFI_TAG, "Unexpected WiFi mode: %d", current_mode);
        m_scan_in_progress = false;
        return false;
    }

    ESP_LOGI(BLUFI_TAG, "WiFi scan started");
    return true;
}

void Blufi::_send_wifi_list() {
    if (m_ap_records.empty()) {
        ESP_LOGW(BLUFI_TAG, "No AP records available, sending WiFi scan fail");
        esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        return;
    }

    ESP_LOGI(BLUFI_TAG, "Sending WiFi list with %d APs", m_ap_records.size());

    std::vector<esp_blufi_ap_record_t> blufi_ap_list;
    for (const auto& ap : m_ap_records) {
        esp_blufi_ap_record_t blufi_ap;
        memset(&blufi_ap, 0, sizeof(blufi_ap));
        memcpy(blufi_ap.ssid, ap.ssid, std::min((size_t)32, sizeof(ap.ssid)));
        blufi_ap.rssi = ap.rssi;
        blufi_ap_list.push_back(blufi_ap);
    }

    esp_blufi_send_wifi_list(blufi_ap_list.size(), blufi_ap_list.data());

    m_ap_records.clear();
    start_wifi_scan();
}

void Blufi::_wifi_scan_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                     void* event_data) {
    Blufi* self = static_cast<Blufi*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(BLUFI_TAG, "WiFi scan done");

        uint16_t ap_num = 0;
        esp_wifi_scan_get_ap_num(&ap_num);

        if (ap_num == 0) {
            ESP_LOGW(BLUFI_TAG, "No APs found");
            self->m_ap_records.clear();
        } else {
            if (self->m_scan_should_save_ssid) {
                self->m_ap_records.resize(ap_num);
                esp_wifi_scan_get_ap_records(&ap_num, self->m_ap_records.data());

                ESP_LOGI(BLUFI_TAG, "Found %d APs", ap_num);
                for (const auto& ap : self->m_ap_records) {
                    ESP_LOGI(BLUFI_TAG, "  SSID: %s, RSSI: %d, Authmode: %d", (char*)ap.ssid,
                             ap.rssi, ap.authmode);
                }
            }
        }
        self->m_scan_in_progress = false;
        // Dispatch a pending GET_WIFI_LIST response if one is waiting on this scan.
        if (self->m_send_list_after_scan) {
            self->m_send_list_after_scan = false;
            self->_send_wifi_list();
        }
    }
}

void Blufi::_handle_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t* param) {
    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(BLUFI_TAG, "BLUFI init finish");
            esp_ble_gap_set_device_name(BLUFI_DEVICE_NAME);
            esp_blufi_adv_start();
            break;
        case ESP_BLUFI_EVENT_DEINIT_FINISH:
            ESP_LOGI(BLUFI_TAG, "BLUFI deinit finish");
            break;
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            ESP_LOGI(BLUFI_TAG, "BLUFI ble connect");
            m_ble_is_connected = true;
            esp_blufi_adv_stop();
            _security_init();
            break;
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            ESP_LOGI(BLUFI_TAG, "BLUFI ble disconnect");
            m_ble_is_connected = false;
            _security_deinit();
            if (!m_provisioned) {
                esp_blufi_adv_start();
            } else {
                esp_blufi_adv_stop();
                if (!m_deinited) {
                    xTaskCreate(
                        [](void* ctx) {
                            static_cast<Blufi*>(ctx)->deinit();
                            vTaskDelete(nullptr);
                        },
                        "blufi_deinit", 4096, this, 5, nullptr);
                }
            }
            break;
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE: {
            ESP_LOGI(BLUFI_TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
            auto& wifi_manager = WifiManager::GetInstance();
            if (!wifi_manager.IsInitialized() && !wifi_manager.Initialize()) {
                ESP_LOGE(BLUFI_TAG, "Failed to initialize WifiManager for opmode change");
                break;
            }
            switch (param->wifi_mode.op_mode) {
                case WIFI_MODE_STA:
                    wifi_manager.StartStation();
                    break;
                case WIFI_MODE_AP:
                    wifi_manager.StartConfigAp();
                    break;
                case WIFI_MODE_APSTA:
                    ESP_LOGW(BLUFI_TAG, "APSTA mode not supported, starting station only");
                    wifi_manager.StartStation();
                    break;
                default:
                    wifi_manager.StopStation();
                    wifi_manager.StopConfigAp();
                    break;
            }
            break;
        }
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP: {
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi connect to AP via esp-wifi-connect");
            std::string ssid(reinterpret_cast<const char*>(m_sta_config.sta.ssid));
            std::string password(reinterpret_cast<const char*>(m_sta_config.sta.password));

            SsidManager::GetInstance().AddSsid(ssid, password);
            m_scan_should_save_ssid = false;

            m_sta_ssid_len = static_cast<int>(std::min(ssid.size(), sizeof(m_sta_ssid)));
            memcpy(m_sta_ssid, ssid.c_str(), m_sta_ssid_len);
            memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
            m_sta_connected = false;
            m_sta_got_ip = false;
            m_sta_is_connecting = true;
            m_sta_conn_info = {};
            m_sta_conn_info.sta_ssid = m_sta_ssid;
            m_sta_conn_info.sta_ssid_len = m_sta_ssid_len;

            auto& wifi_manager = WifiManager::GetInstance();

            if (wifi_manager.IsInitialized()) {
                if (wifi_manager.IsConfigMode()) {
                    wifi_manager.StopConfigAp();
                }
                wifi_manager.StopStation();
            }

            if (!wifi_manager.IsInitialized() && !wifi_manager.Initialize()) {
                ESP_LOGE(BLUFI_TAG, "Failed to initialize WifiManager");
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(500));

            wifi_manager.StartStation();

            xTaskCreate(
                [](void* ctx) {
                    auto* self = static_cast<Blufi*>(ctx);
                    auto& wifi = WifiManager::GetInstance();
                    constexpr int kConnectTimeoutMs = 10000;
                    constexpr TickType_t kDelayTick = pdMS_TO_TICKS(200);
                    int waited_ms = 0;

                    while (waited_ms < kConnectTimeoutMs && !wifi.IsConnected()) {
                        vTaskDelay(kDelayTick);
                        waited_ms += 200;
                    }

                    wifi_mode_t mode = GetWifiModeWithFallback(wifi);
                    const int softap_conn_num = _get_softap_conn_num();

                    if (wifi.IsConnected()) {
                        self->m_sta_is_connecting = false;
                        self->m_sta_connected = true;
                        self->m_sta_got_ip = true;
                        self->m_provisioned = true;

                        auto current_ssid = wifi.GetSsid();
                        if (!current_ssid.empty()) {
                            self->m_sta_ssid_len = static_cast<int>(
                                std::min(current_ssid.size(), sizeof(self->m_sta_ssid)));
                            memcpy(self->m_sta_ssid, current_ssid.c_str(), self->m_sta_ssid_len);
                        }

                        wifi_ap_record_t ap_info{};
                        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                            memcpy(self->m_sta_bssid, ap_info.bssid, sizeof(self->m_sta_bssid));
                        }

                        esp_blufi_extra_info_t info = {};
                        memcpy(info.sta_bssid, self->m_sta_bssid, sizeof(self->m_sta_bssid));
                        info.sta_bssid_set = true;
                        info.sta_ssid = self->m_sta_ssid;
                        info.sta_ssid_len = self->m_sta_ssid_len;
                        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS,
                                                        softap_conn_num, &info);
                        ESP_LOGI(BLUFI_TAG, "connected to WiFi");

                        if (self->m_ble_is_connected) {
                            esp_blufi_disconnect();
                        }
                    } else {
                        self->m_sta_is_connecting = false;
                        self->m_sta_connected = false;
                        self->m_sta_got_ip = false;

                        esp_blufi_extra_info_t info = {};
                        info.sta_ssid = self->m_sta_ssid;
                        info.sta_ssid_len = self->m_sta_ssid_len;
                        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL,
                                                        softap_conn_num, &info);
                        ESP_LOGE(BLUFI_TAG, "Failed to connect to WiFi via esp-wifi-connect");
                    }
                    vTaskDelete(nullptr);
                },
                "blufi_wifi_conn", 4096, this, 5, nullptr);
            break;
        }
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi disconnect from AP");
            if (WifiManager::GetInstance().IsInitialized()) {
                WifiManager::GetInstance().StopStation();
            }
            m_sta_is_connecting = false;
            m_sta_connected = false;
            m_sta_got_ip = false;
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
            auto& wifi = WifiManager::GetInstance();
            wifi_mode_t mode = GetWifiModeWithFallback(wifi);
            const int softap_conn_num = _get_softap_conn_num();

            if (wifi.IsInitialized() && wifi.IsConnected()) {
                m_sta_connected = true;
                m_sta_got_ip = true;

                auto current_ssid = wifi.GetSsid();
                if (!current_ssid.empty()) {
                    m_sta_ssid_len =
                        static_cast<int>(std::min(current_ssid.size(), sizeof(m_sta_ssid)));
                    memcpy(m_sta_ssid, current_ssid.c_str(), m_sta_ssid_len);
                }

                esp_blufi_extra_info_t info;
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, m_sta_bssid, 6);
                info.sta_ssid = m_sta_ssid;
                info.sta_ssid_len = m_sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_conn_num,
                                                &info);
            } else if (m_sta_is_connecting) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_conn_num,
                                                &m_sta_conn_info);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_conn_num,
                                                &m_sta_conn_info);
            }
            ESP_LOGI(BLUFI_TAG, "BLUFI get wifi status");
            break;
        }
        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            memcpy(m_sta_config.sta.bssid, param->sta_bssid.bssid, 6);
            m_sta_config.sta.bssid_set = true;
            ESP_LOGI(BLUFI_TAG, "Recv STA BSSID");
            break;
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            strncpy((char*)m_sta_config.sta.ssid, (char*)param->sta_ssid.ssid,
                    param->sta_ssid.ssid_len);
            m_sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA SSID: %s", m_sta_config.sta.ssid);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            strncpy((char*)m_sta_config.sta.password, (char*)param->sta_passwd.passwd,
                    param->sta_passwd.passwd_len);
            m_sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA PASSWORD : %s", m_sta_config.sta.password);
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
            ESP_LOGI(BLUFI_TAG, "BLUFI get wifi list");
            // Case 1: a scan is already in flight (init scan or refresh scan started by
            // the previous _send_wifi_list()). Defer the response to its done handler
            // instead of blocking the BluFi task.
            if (m_scan_in_progress) {
                m_send_list_after_scan = true;
                break;
            }
            // Case 2: cache is populated. Respond immediately; _send_wifi_list() also
            // kicks off an async refresh scan to keep the cache fresh.
            if (!m_ap_records.empty()) {
                _send_wifi_list();
                break;
            }
            // Case 3: no cache (e.g. driver was stopped during a config-mode transition,
            // init scan never completed). Trigger a real scan and dispatch from the
            // scan-done handler. If the scan cannot start, return an error frame so the
            // App exits its wait state instead of timing out.
            m_scan_should_save_ssid = true;
            m_send_list_after_scan = true;
            if (!start_wifi_scan()) {
                m_send_list_after_scan = false;
                esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
            }
            break;
        }
        default:
            ESP_LOGW(BLUFI_TAG, "Unhandled event: %d", event);
            break;
    }
}

void Blufi::_event_callback_trampoline(esp_blufi_cb_event_t event, esp_blufi_cb_param_t* param) {
    GetInstance()._handle_event(event, param);
}

void Blufi::_negotiate_data_handler_trampoline(uint8_t* data, int len, uint8_t** output_data,
                                               int* output_len, bool* need_free) {
    GetInstance()._dh_negotiate_data_handler(data, len, output_data, output_len, need_free);
}

int Blufi::_encrypt_func_trampoline(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    return GetInstance()._aes_encrypt(iv8, crypt_data, crypt_len);
}

int Blufi::_decrypt_func_trampoline(uint8_t iv8, uint8_t* crypt_data, int crypt_len) {
    return GetInstance()._aes_decrypt(iv8, crypt_data, crypt_len);
}

uint16_t Blufi::_checksum_func_trampoline(uint8_t iv8, uint8_t* data, int len) {
    return _crc_checksum(iv8, data, len);
}
