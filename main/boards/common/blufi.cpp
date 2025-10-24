#include "blufi.h"
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_bt.h"

// Bluedroid specific
#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#endif

// NimBLE specific
#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "console/console.h"
extern void esp_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
extern int esp_blufi_gatt_svr_init(void);
extern void esp_blufi_gatt_svr_deinit(void);
extern void esp_blufi_btc_init(void);
extern void esp_blufi_btc_deinit(void);
#endif

extern "C" {
    // Blufi Advertising & Connection
    void esp_blufi_adv_start(void);
    void esp_blufi_adv_stop(void);
    void esp_blufi_disconnect(void);

    // Internal BTC layer functions needed for error reporting
    void btc_blufi_report_error(esp_blufi_error_state_t state);

    // Bluedroid specific GAP event handler
#ifdef CONFIG_BT_BLUEDROID_ENABLED
    void esp_blufi_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif

    // NimBLE specific internal functions
#ifdef CONFIG_BT_NIMBLE_ENABLED
    void esp_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
    int esp_blufi_gatt_svr_init(void);
    void esp_blufi_gatt_svr_deinit(void);
    void esp_blufi_btc_init(void);
    void esp_blufi_btc_deinit(void);
#endif
}

// mbedTLS for security
#include "mbedtls/md5.h"
#include "esp_crc.h"
#include "esp_random.h"
#include "ssid_manager.h"

// Logging Tag
static const char* BLUFI_TAG = "BLUFI_CLASS";


Blufi& Blufi::GetInstance() {
    static Blufi instance;
    return instance;
}

Blufi::Blufi() :
    m_sec(nullptr),
    m_ble_is_connected(false),
    m_sta_connected(false),
    m_sta_got_ip(false),
    m_sta_ssid_len(0),
    m_sta_is_connecting(false)
{
    // Initialize member variables
    memset(&m_sta_config, 0, sizeof(m_sta_config));
    memset(&m_ap_config, 0, sizeof(m_ap_config));
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
    esp_err_t ret;

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
    esp_err_t ret;
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
    if (ret != ESP_OK) return ret;
    
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
    if(rc){
        return rc;
    }
    return esp_blufi_profile_init();
}

esp_err_t Blufi::_host_and_cb_init() {
    esp_blufi_callbacks_t blufi_callbacks = {
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
    if(ret){
        ESP_LOGE(BLUFI_TAG, "%s blufi register failed, error code = %x", __func__, ret);
        return ret;
    }
    ret = _gap_register_callback();
    if(ret){
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

void Blufi::_nimble_on_sync() {
    // This is called when the host and controller are synced.
    // It's a good place to initialize the Blufi profile.
    esp_blufi_profile_init();
}

void Blufi::_nimble_host_task(void *param) {
    ESP_LOGI(BLUFI_TAG, "BLE Host Task Started");
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
    nimble_port_freertos_deinit();
}

esp_err_t Blufi::_host_init() {
    // esp_nimble_init() is called by controller_init for NimBLE
    ble_hs_cfg.reset_cb = _nimble_on_reset;
    ble_hs_cfg.sync_cb = _nimble_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;

    // Security Manager settings (can be customized)
    ble_hs_cfg.sm_io_cap = 4; // IO capability: No Input, No Output
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif

    int rc = esp_blufi_gatt_svr_init();
    assert(rc == 0);

    ble_store_config_init(); // Configure the BLE storage
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

esp_err_t Blufi::_gap_register_callback(void) {
    return ESP_OK; // For NimBLE, GAP callbacks are handled differently
}

esp_err_t Blufi::_host_and_cb_init() {
    esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = &_event_callback_trampoline,
        .negotiate_data_handler = &_negotiate_data_handler_trampoline,
        .encrypt_func = &_encrypt_func_trampoline,
        .decrypt_func = &_decrypt_func_trampoline,
        .checksum_func = &_checksum_func_trampoline,
    };

    esp_err_t ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if(ret){
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
    // For NimBLE, host init needs to be done after controller init
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
#endif // Generic controller init


static int myrand(void *rng_state, unsigned char *output, size_t len) {
    esp_fill_random(output, len);
    return 0;
}

void Blufi::_security_init() {
    m_sec = new BlufiSecurity();
    if (m_sec == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Failed to allocate security context");
        return;
    }
    memset(m_sec, 0, sizeof(BlufiSecurity));
    m_sec->dhm = new mbedtls_dhm_context();
    m_sec->aes = new mbedtls_aes_context();

    mbedtls_dhm_init(m_sec->dhm);
    mbedtls_aes_init(m_sec->aes);

    memset(m_sec->iv, 0x0, sizeof(m_sec->iv));
}

void Blufi::_security_deinit() {
    if (m_sec == nullptr) return;

    if (m_sec->dh_param) {
        free(m_sec->dh_param);
    }
    mbedtls_dhm_free(m_sec->dhm);
    mbedtls_aes_free(m_sec->aes);
    delete m_sec->dhm;
    delete m_sec->aes;
    delete m_sec;
    m_sec = nullptr;
}

void Blufi::_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free) {
    if (m_sec == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Security not initialized in DH handler");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }
    
    uint8_t type = data[0];
    switch (type) {
        case 0x00: /* DH_PARAM_LEN */
            m_sec->dh_param_len = (data[1] << 8) | data[2];
            if (m_sec->dh_param) {
                free(m_sec->dh_param);
                m_sec->dh_param = nullptr;
            }
            m_sec->dh_param = (uint8_t *)malloc(m_sec->dh_param_len);
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH malloc failed");
                btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
            }
            break;
        case 0x01: /* DH_PARAM_DATA */ {
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH param not allocated");
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
            uint8_t *param = m_sec->dh_param;
            memcpy(m_sec->dh_param, &data[1], m_sec->dh_param_len);
            int ret = mbedtls_dhm_read_params(m_sec->dhm, &param, &param[m_sec->dh_param_len]);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_read_params failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
                return;
            }

            const int dhm_len = mbedtls_dhm_get_len(m_sec->dhm);
            ret = mbedtls_dhm_make_public(m_sec->dhm, dhm_len, m_sec->self_public_key, DH_SELF_PUB_KEY_LEN, myrand, NULL);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_make_public failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }

            ret = mbedtls_dhm_calc_secret(m_sec->dhm, m_sec->share_key, SHARE_KEY_LEN, &m_sec->share_len, myrand, NULL);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_calc_secret failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_ENCRYPT_ERROR);
                return;
            }

            ret = mbedtls_md5(m_sec->share_key, m_sec->share_len, m_sec->psk);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_md5 failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
                return;
            }

            mbedtls_aes_setkey_enc(m_sec->aes, m_sec->psk, PSK_LEN * 8);

            *output_data = &m_sec->self_public_key[0];
            *output_len = dhm_len;
            *need_free = false;

            free(m_sec->dh_param);
            m_sec->dh_param = NULL;
        }
            break;
        default:
             ESP_LOGE(BLUFI_TAG, "DH handler unknown type: %d", type);
    }
}

int Blufi::_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    if (!m_sec) return -1;
    size_t iv_offset = 0;
    uint8_t iv0[16];
    memcpy(iv0, m_sec->iv, 16);
    iv0[0] = iv8;
    return mbedtls_aes_crypt_cfb128(m_sec->aes, MBEDTLS_AES_ENCRYPT, crypt_len, &iv_offset, iv0, crypt_data, crypt_data);
}

int Blufi::_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    if (!m_sec) return -1;
    size_t iv_offset = 0;
    uint8_t iv0[16];
    memcpy(iv0, m_sec->iv, 16);
    iv0[0] = iv8;
    return mbedtls_aes_crypt_cfb128(m_sec->aes, MBEDTLS_AES_DECRYPT, crypt_len, &iv_offset, iv0, crypt_data, crypt_data);
}

uint16_t Blufi::_crc_checksum(uint8_t iv8, uint8_t *data, int len) {
    return esp_crc16_be(0, data, len);
}


int Blufi::_get_softap_conn_num() {
    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}

void Blufi::_handle_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
     switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(BLUFI_TAG, "BLUFI init finish");
            esp_blufi_adv_start();
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
            esp_blufi_adv_start();
            break;
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
            ESP_LOGI(BLUFI_TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
            ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode));
            break;
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi connect to AP");
            // Here, we would connect to Wi-Fi. The original code restarts, which is one strategy.
            // Another is to start the connection process directly.
            esp_wifi_set_config(WIFI_IF_STA, &m_sta_config);
            esp_wifi_connect();
            m_sta_is_connecting = true;
            m_sta_conn_info = {}; // Reset connection info
            break;
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi disconnect from AP");
            esp_wifi_disconnect();
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            if (m_sta_connected) {
                esp_blufi_extra_info_t info;
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, m_sta_bssid, 6);
                info.sta_bssid_set = true;
                info.sta_ssid = m_sta_ssid;
                info.sta_ssid_len = m_sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, m_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, _get_softap_conn_num(), &info);
            } else if (m_sta_is_connecting) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, _get_softap_conn_num(), &m_sta_conn_info);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, _get_softap_conn_num(), &m_sta_conn_info);
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
            strncpy((char *)m_sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
            m_sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA SSID: %s", m_sta_config.sta.ssid);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            strncpy((char *)m_sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
            m_sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA PASSWORD");
            break;
        default:
            ESP_LOGW(BLUFI_TAG, "Unhandled event: %d", event);
            break;
    }
}


void Blufi::_event_callback_trampoline(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    GetInstance()._handle_event(event, param);
}

void Blufi::_negotiate_data_handler_trampoline(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free) {
    GetInstance()._dh_negotiate_data_handler(data, len, output_data, output_len, need_free);
}

int Blufi::_encrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    return GetInstance()._aes_encrypt(iv8, crypt_data, crypt_len);
}

int Blufi::_decrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    return GetInstance()._aes_decrypt(iv8, crypt_data, crypt_len);
}

uint16_t Blufi::_checksum_func_trampoline(uint8_t iv8, uint8_t *data, int len) {
    return _crc_checksum(iv8, data, len);
}