#include "ir_controller.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include <vector>
#include "mcp_server.h"

static const char *TAG = "IrController";

#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_RX_CHANNEL RMT_CHANNEL_1
#define RMT_CLK_DIV    80

IrController::IrController(gpio_num_t rx_gpio, gpio_num_t tx_gpio, bool module_internal_modulator)
    : rx_gpio_(rx_gpio),
      tx_gpio_(tx_gpio),
      module_internal_modulator_(module_internal_modulator),
      last_address_(0),
      last_command_(0),
      last_valid_(false)
{
    ESP_LOGI(TAG, "IrController init rx=%d tx=%d internal_mod=%d", rx_gpio_, tx_gpio_, module_internal_modulator_ ? 1 : 0);

    InitRmtTx();
    InitRmtRx();

    auto &mcp = McpServer::GetInstance();
    mcp.AddTool("self.ir.send", "Send NEC IR command", PropertyList({
        { "address", "number" },
        { "command", "number" },
        { "modulated", "boolean (optional)" }
    }), [this](const PropertyList &props) -> ReturnValue {
        uint16_t addr = (uint16_t) props.GetInt("address", 0);
        uint16_t cmd  = (uint16_t) props.GetInt("command", 0);
        esp_err_t err = SendNEC(addr, cmd);
        return err == ESP_OK;
    });

    mcp.AddTool("self.ir.get_last", "Get last received NEC code", PropertyList(), [this](const PropertyList& p) -> ReturnValue {
        if (!last_valid_) {
            return "{\"valid\": false}";
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"valid\": true, \"address\": %u, \"command\": %u}", (unsigned)last_address_, (unsigned)last_command_);
        return std::string(buf);
    });
}

IrController::~IrController()
{
    rmt_driver_uninstall(RMT_TX_CHANNEL);
    rmt_driver_uninstall(RMT_RX_CHANNEL);
}

esp_err_t IrController::InitRmtTx()
{
    rmt_config_t tx_cfg = {};
    tx_cfg.rmt_mode = RMT_MODE_TX;
    tx_cfg.channel = RMT_TX_CHANNEL;
    tx_cfg.gpio_num = tx_gpio_;
    tx_cfg.clk_div = RMT_CLK_DIV;
    tx_cfg.mem_block_num = 1;
    tx_cfg.tx_config.loop_en = false;
    tx_cfg.tx_config.carrier_freq_hz = module_internal_modulator_ ? 0 : 38000;
    tx_cfg.tx_config.carrier_duty_percent = module_internal_modulator_ ? 0 : 33;
    tx_cfg.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    tx_cfg.tx_config.carrier_en = module_internal_modulator_ ? false : true;

    esp_err_t err = rmt_config(&tx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_config tx failed: %d", err);
        return err;
    }
    err = rmt_driver_install(tx_cfg.channel, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_driver_install tx failed: %d", err);
        return err;
    }
    return ESP_OK;
}

esp_err_t IrController::InitRmtRx()
{
    rmt_config_t rx_cfg = {};
    rx_cfg.rmt_mode = RMT_MODE_RX;
    rx_cfg.channel = RMT_RX_CHANNEL;
    rx_cfg.gpio_num = rx_gpio_;
    rx_cfg.clk_div = RMT_CLK_DIV;
    rx_cfg.mem_block_num = 2;
    rx_cfg.rx_config.filter_en = true;
    rx_cfg.rx_config.filter_ticks_thresh = 100;
    rx_cfg.rx_config.idle_threshold = 10000;

    esp_err_t err = rmt_config(&rx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_config rx failed: %d", err);
        return err;
    }
    err = rmt_driver_install(rx_cfg.channel, 1000, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_driver_install rx failed: %d", err);
        return err;
    }

    xTaskCreate(&IrController::RmtRxTask, "ir_rmt_rx", 4096, this, configMAX_PRIORITIES - 5, NULL);
    return ESP_OK;
}

void IrController::RmtRxTask(void* arg)
{
    IrController* self = static_cast<IrController*>(arg);
    RingbufHandle_t rb = NULL;
    rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &rb);
    if (rb == NULL) {
        ESP_LOGW(TAG, "rmt rx ringbuf handle null");
        vTaskDelete(NULL);
        return;
    }
    while (true) {
        size_t rx_size = 0;
        rmt_item32_t* items = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, pdMS_TO_TICKS(1000));
        if (items) {
            int item_num = rx_size / sizeof(rmt_item32_t);
            std::vector<uint32_t> pulses;
            pulses.reserve(item_num * 2);
            for (int i = 0; i < item_num; i++) {
                uint32_t t0 = items[i].duration0;
                uint32_t t1 = items[i].duration1;
                pulses.push_back(t0);
                pulses.push_back(t1);
            }
            const uint32_t US_PER_TICK = 1;
            std::vector<uint32_t> us;
            us.reserve(pulses.size());
            for (auto t : pulses) us.push_back(t * US_PER_TICK);

            int idx = 0;
            while (idx + 1 < (int)us.size()) {
                if (us[idx] > 8500 && us[idx] < 9500 && us[idx+1] > 4000 && us[idx+1] < 5000) {
                    idx += 2;
                    uint32_t data = 0;
                    for (int b = 0; b < 32 && idx + 1 < (int)us.size(); b++) {
                        uint32_t on = us[idx];
                        uint32_t off = us[idx+1];
                        if (on < 400 || on > 800) { break; }
                        bool bit = (off > 1200);
                        data |= (bit ? (1u << b) : 0u);
                        idx += 2;
                    }
                    uint8_t addr = data & 0xFF;
                    uint8_t addr_inv = (data >> 8) & 0xFF;
                    uint8_t cmd = (data >> 16) & 0xFF;
                    uint8_t cmd_inv = (data >> 24) & 0xFF;
                    if ((uint8_t)(addr ^ addr_inv) == 0xFF && (uint8_t)(cmd ^ cmd_inv) == 0xFF) {
                        self->last_address_ = addr;
                        self->last_command_ = cmd;
                        self->last_valid_ = true;
                        ESP_LOGI(TAG, "NEC received addr=0x%02x cmd=0x%02x", addr, cmd);
                    } else {
                        ESP_LOGW(TAG, "NEC parity mismatch");
                    }
                    break;
                }
                idx++;
            }

            vRingbufferReturnItem(rb, (void*) items);
        }
    }
}

size_t IrController::BuildNecItems(uint16_t address, uint16_t command, rmt_item32_t* out_items, size_t max_items)
{
    const uint32_t T_LEAD_ON = 9000;
    const uint32_t T_LEAD_OFF = 4500;
    const uint32_t T_BIT_ON = 560;
    const uint32_t T_BIT0_OFF = 560;
    const uint32_t T_BIT1_OFF = 1690;
    const uint32_t T_STOP_ON = 560;

    uint32_t data = ((uint32_t)command << 16) | (uint32_t)address;
    size_t idx = 0;

    auto push = [&](uint32_t on, uint32_t off) {
        if (idx >= max_items) return false;
        out_items[idx].level0 = 1;
        out_items[idx].duration0 = on;
        out_items[idx].level1 = 0;
        out_items[idx].duration1 = off;
        idx++;
        return true;
    };

    if (!push(T_LEAD_ON, T_LEAD_OFF)) return idx;
    for (int b = 0; b < 32; b++) {
        bool bit = (data >> b) & 1;
        uint32_t off = bit ? T_BIT1_OFF : T_BIT0_OFF;
        if (!push(T_BIT_ON, off)) return idx;
    }
    if (!push(T_STOP_ON, 0)) return idx;
    return idx;
}

esp_err_t IrController::SendNEC(uint16_t address, uint16_t command)
{
    const size_t MAX_ITEMS = 40;
    rmt_item32_t items[MAX_ITEMS];
    memset(items, 0, sizeof(items));
    size_t item_cnt = BuildNecItems(address, command, items, MAX_ITEMS);
    if (item_cnt == 0) return ESP_ERR_NO_MEM;

    esp_err_t err = rmt_write_items(RMT_TX_CHANNEL, items, item_cnt, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_write_items failed: %d", err);
        return err;
    }
    rmt_wait_tx_done(RMT_TX_CHANNEL, pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "NEC sent addr=0x%04x cmd=0x%04x", address, command);
    return ESP_OK;
}

bool IrController::GetLastReceived(uint16_t &address, uint16_t &command)
{
    if (!last_valid_) return false;
    address = last_address_;
    command = last_command_;
    return true;
}
