#include "ethernet_board.h"

#include "audio_codec.h"
#include "display.h"
#include "sdkconfig.h"

#include <cJSON.h>
#include <driver/gpio.h>
#include <esp_eth.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_network.h>
#include <esp_netif.h>
#include <font_awesome.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/soc_caps.h>

#include <utility>

static const char* TAG = "EthernetBoard";

static esp_eth_phy_t* CreatePhy(const eth_phy_config_t& phy_config) {
#if CONFIG_XIAOZHI_ETH_PHY_IP101
    return esp_eth_phy_new_ip101(&phy_config);
#else
    return nullptr;
#endif
}

static eth_esp32_emac_config_t CreateEmacConfig() {
    eth_esp32_emac_config_t config = {};
    config.smi_gpio.mdc_num = CONFIG_XIAOZHI_ETH_MDC_GPIO;
    config.smi_gpio.mdio_num = CONFIG_XIAOZHI_ETH_MDIO_GPIO;
    config.interface = EMAC_DATA_INTERFACE_RMII;
    config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    config.clock_config.rmii.clock_gpio = static_cast<emac_rmii_clock_gpio_t>(50);
    config.dma_burst_len = ETH_DMA_BURST_LEN_32;
    config.intr_priority = 0;
#if SOC_EMAC_USE_MULTI_IO_MUX || SOC_EMAC_MII_USE_GPIO_MATRIX
    config.emac_dataif_gpio.rmii.tx_en_num = 49;
    config.emac_dataif_gpio.rmii.txd0_num = 34;
    config.emac_dataif_gpio.rmii.txd1_num = 35;
    config.emac_dataif_gpio.rmii.crs_dv_num = 28;
    config.emac_dataif_gpio.rmii.rxd0_num = 29;
    config.emac_dataif_gpio.rmii.rxd1_num = 30;
#endif
#if !SOC_EMAC_RMII_CLK_OUT_INTERNAL_LOOPBACK
    config.clock_config_out_in.rmii.clock_mode = EMAC_CLK_EXT_IN;
    config.clock_config_out_in.rmii.clock_gpio = static_cast<emac_rmii_clock_gpio_t>(-1);
#endif
    config.mdc_freq_hz = 0;
    return config;
}

std::string EthernetBoard::GetBoardType() {
    return "ethernet";
}

void EthernetBoard::SetNetworkEventCallback(NetworkEventCallback callback) {
    network_event_callback_ = std::move(callback);
}

void EthernetBoard::StartNetwork() {
    xTaskCreate(NetworkTaskEntry, "eth_net", 4096, this, 5, nullptr);
}

void EthernetBoard::NetworkTaskEntry(void* arg) {
    auto* board = static_cast<EthernetBoard*>(arg);
    board->NetworkTask();
    vTaskDelete(nullptr);
}

void EthernetBoard::NetworkTask() {
    OnNetworkEvent(NetworkEvent::Connecting, "Ethernet");

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize esp-netif: %s", esp_err_to_name(ret));
        OnNetworkEvent(NetworkEvent::Disconnected);
        return;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        OnNetworkEvent(NetworkEvent::Disconnected);
        return;
    }

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    eth_netif_ = esp_netif_new(&netif_config);
    if (eth_netif_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Ethernet netif");
        OnNetworkEvent(NetworkEvent::Disconnected);
        return;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_XIAOZHI_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_XIAOZHI_ETH_PHY_RST_GPIO;

    eth_esp32_emac_config_t emac_config = CreateEmacConfig();
    esp_eth_mac_t* mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    if (mac == nullptr) {
        ESP_LOGE(TAG, "Failed to create Ethernet MAC");
        OnNetworkEvent(NetworkEvent::Disconnected);
        return;
    }

    esp_eth_phy_t* phy = CreatePhy(phy_config);
    if (phy == nullptr) {
        ESP_LOGE(TAG, "Failed to create Ethernet PHY");
        mac->del(mac);
        OnNetworkEvent(NetworkEvent::Disconnected);
        return;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&eth_config, &eth_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install Ethernet driver: %s", esp_err_to_name(ret));
        mac->del(mac);
        phy->del(phy);
        OnNetworkEvent(NetworkEvent::Disconnected);
        return;
    }

    eth_glue_ = esp_eth_new_netif_glue(eth_handle_);
    if (eth_glue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Ethernet netif glue");
        esp_eth_driver_uninstall(eth_handle_);
        eth_handle_ = nullptr;
        mac->del(mac);
        phy->del(phy);
        OnNetworkEvent(NetworkEvent::Disconnected);
        return;
    }

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif_, eth_glue_));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, EthEventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, GotIpEventHandler, this));

    ret = esp_eth_start(eth_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Ethernet: %s", esp_err_to_name(ret));
        OnNetworkEvent(NetworkEvent::Disconnected);
    }
}

void EthernetBoard::EthEventHandler(void* arg, const char* event_base, int32_t event_id, void* event_data) {
    auto* board = static_cast<EthernetBoard*>(arg);
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t*)event_data;

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet link up, MAC %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Ethernet link down");
            board->connected_ = false;
            board->ip_address_.clear();
            board->OnNetworkEvent(NetworkEvent::Disconnected);
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet stopped");
            board->connected_ = false;
            board->ip_address_.clear();
            board->OnNetworkEvent(NetworkEvent::Disconnected);
            break;
        default:
            break;
    }
}

void EthernetBoard::GotIpEventHandler(void* arg, const char* event_base, int32_t event_id, void* event_data) {
    auto* board = static_cast<EthernetBoard*>(arg);
    auto* event = static_cast<ip_event_got_ip_t*>(event_data);
    const esp_netif_ip_info_t* ip_info = &event->ip_info;

    char ip[16] = {0};
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info->ip));
    board->connected_ = true;
    board->ip_address_ = ip;

    ESP_LOGI(TAG, "Ethernet got IP: %s", ip);
    board->OnNetworkEvent(NetworkEvent::Connected, "Ethernet");
}

void EthernetBoard::OnNetworkEvent(NetworkEvent event, const std::string& data) {
    switch (event) {
        case NetworkEvent::Connecting:
            ESP_LOGI(TAG, "Ethernet connecting");
            break;
        case NetworkEvent::Connected:
            ESP_LOGI(TAG, "Ethernet connected");
            break;
        case NetworkEvent::Disconnected:
            ESP_LOGW(TAG, "Ethernet disconnected");
            break;
        default:
            break;
    }

    if (network_event_callback_) {
        network_event_callback_(event, data);
    }
}

NetworkInterface* EthernetBoard::GetNetwork() {
    static EspNetwork network;
    return &network;
}

const char* EthernetBoard::GetNetworkStateIcon() {
    return connected_ ? FONT_AWESOME_SIGNAL_STRONG : FONT_AWESOME_SIGNAL_OFF;
}

std::string EthernetBoard::GetEthernetMacAddress() {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_ETH);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(mac_str);
}

std::string EthernetBoard::GetBoardJson() {
    std::string json = R"({"type":")" + std::string(BOARD_TYPE) + R"(",)";
    json += R"("name":")" + std::string(BOARD_NAME) + R"(",)";
    json += R"("network":"ethernet",)";
    if (!ip_address_.empty()) {
        json += R"("ip":")" + ip_address_ + R"(",)";
    }
    json += R"("mac":")" + GetEthernetMacAddress() + R"("})";
    return json;
}

void EthernetBoard::SetPowerSaveLevel(PowerSaveLevel level) {
    (void)level;
}

std::string EthernetBoard::GetDeviceStatusJson() {
    auto& board = Board::GetInstance();
    auto root = cJSON_CreateObject();

    auto audio_speaker = cJSON_CreateObject();
    if (auto codec = board.GetAudioCodec()) {
        cJSON_AddNumberToObject(audio_speaker, "volume", codec->output_volume());
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    auto screen = cJSON_CreateObject();
    if (auto backlight = board.GetBacklight()) {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    if (auto display = board.GetDisplay(); display && display->height() > 64) {
        if (auto theme = display->GetTheme()) {
            cJSON_AddStringToObject(screen, "theme", theme->name().c_str());
        }
    }
    cJSON_AddItemToObject(root, "screen", screen);

    int level = 0;
    bool charging = false;
    bool discharging = false;
    if (board.GetBatteryLevel(level, charging, discharging)) {
        auto battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }

    auto network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "type", "ethernet");
    cJSON_AddStringToObject(network, "state", connected_ ? "connected" : "disconnected");
    if (!ip_address_.empty()) {
        cJSON_AddStringToObject(network, "ip", ip_address_.c_str());
    }
    cJSON_AddItemToObject(root, "network", network);

    float temp = 0.0f;
    if (board.GetTemperature(temp)) {
        auto chip = cJSON_CreateObject();
        cJSON_AddNumberToObject(chip, "temperature", temp);
        cJSON_AddItemToObject(root, "chip", chip);
    }

    auto str = cJSON_PrintUnformatted(root);
    std::string result(str);
    cJSON_free(str);
    cJSON_Delete(root);
    return result;
}
