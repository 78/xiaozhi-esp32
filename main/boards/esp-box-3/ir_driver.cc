#include "ir_driver.h"
#include "config.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <cstring>
#include <cmath>

#define TAG "IrDriver"

// RMT resolution — 1 MHz tick = 1µs per RMT symbol unit. NEC timings are
// in the 562µs / 1687µs / 4500µs / 9000µs range, comfortably representable.
static constexpr uint32_t kRmtResolutionHz = 1'000'000;

// IR carrier — standard 38kHz, 33% duty.
static constexpr uint32_t kIrCarrierHz = 38000;
static constexpr float kIrCarrierDuty = 0.33f;

// NEC protocol decode tolerances (symbols are in ticks, here = µs).
static constexpr uint32_t kNecHeaderHighMin = 8000, kNecHeaderHighMax = 10000;
static constexpr uint32_t kNecHeaderLowMin = 4000, kNecHeaderLowMax = 5000;
static constexpr uint32_t kNecBitHigh = 562, kNecBitHighTol = 200;
static constexpr uint32_t kNecBit0Low = 562, kNecBit1Low = 1687, kNecBitLowTol = 300;

static constexpr size_t kRxQueueSize = 4;
static constexpr size_t kRxBufferSymbols = 64;

// Allocate a static RX buffer at file scope so the driver doesn't keep
// re-allocating per receive. NEC max frame is 33 symbols (header+32 bits+
// stop), so 64 is plenty.
static rmt_symbol_word_t s_rx_buffer[kRxBufferSymbols];


IrDriver::IrDriver()
    : ok_(false), tx_chan_(nullptr), rx_chan_(nullptr),
      nec_encoder_(nullptr), rx_queue_(nullptr),
      learn_started_at_ms_(0), learn_window_ms_(0), learn_consumed_(true) {
    transmit_config_ = {};
    transmit_config_.loop_count = 0;

    // ---- Power on the IR_3V3 rail ----
    // P-MOSFET Q2 gate = IO9 (per SENSOR-02 schematic). Drive LOW to enable
    // IR_3V3, which powers the IRM-H638T receiver IC. Without this the RX
    // line is floating and no pulses ever reach the RMT channel.
    {
        gpio_config_t pwr_cfg = {};
        pwr_cfg.pin_bit_mask = 1ULL << SENSOR_IR_POWER_PIN;
        pwr_cfg.mode = GPIO_MODE_OUTPUT;
        pwr_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        pwr_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        pwr_cfg.intr_type = GPIO_INTR_DISABLE;
        esp_err_t err = gpio_config(&pwr_cfg);
        if (err == ESP_OK) {
            gpio_set_level(SENSOR_IR_POWER_PIN, 0);  // LOW = MOSFET ON = IR_3V3 active
            ESP_LOGI(TAG, "IR_3V3 power gate (IO%d) driven LOW", SENSOR_IR_POWER_PIN);
        } else {
            ESP_LOGW(TAG, "IR power gate config failed: %s", esp_err_to_name(err));
        }
    }

    // ---- TX channel ----
    rmt_tx_channel_config_t tx_cfg = {};
    tx_cfg.gpio_num = SENSOR_IR_TX_PIN;
    tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_cfg.resolution_hz = kRmtResolutionHz;
    tx_cfg.mem_block_symbols = 64;
    tx_cfg.trans_queue_depth = 4;
    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &tx_chan_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(err));
        return;
    }

    rmt_carrier_config_t carrier_cfg = {};
    carrier_cfg.duty_cycle = kIrCarrierDuty;
    carrier_cfg.frequency_hz = kIrCarrierHz;
    carrier_cfg.flags.polarity_active_low = false;
    err = rmt_apply_carrier(tx_chan_, &carrier_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_apply_carrier failed: %s", esp_err_to_name(err));
        return;
    }

    ir_nec_encoder_config_t enc_cfg = {};
    enc_cfg.resolution = kRmtResolutionHz;
    err = rmt_new_ir_nec_encoder(&enc_cfg, &nec_encoder_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_new_ir_nec_encoder failed: %s", esp_err_to_name(err));
        return;
    }

    err = rmt_enable(tx_chan_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_enable(tx) failed: %s", esp_err_to_name(err));
        return;
    }

    // ---- RX channel ----
    rmt_rx_channel_config_t rx_cfg = {};
    rx_cfg.gpio_num = SENSOR_IR_RX_PIN;
    rx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_cfg.resolution_hz = kRmtResolutionHz;
    rx_cfg.mem_block_symbols = 64;
    err = rmt_new_rx_channel(&rx_cfg, &rx_chan_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_new_rx_channel failed: %s", esp_err_to_name(err));
        return;
    }

    rx_queue_ = xQueueCreate(kRxQueueSize, sizeof(rmt_rx_done_event_data_t));
    if (rx_queue_ == nullptr) {
        ESP_LOGW(TAG, "xQueueCreate failed");
        return;
    }

    rmt_rx_event_callbacks_t rx_cbs = {};
    rx_cbs.on_recv_done = RmtRxDoneCallback;
    err = rmt_rx_register_event_callbacks(rx_chan_, &rx_cbs, rx_queue_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_rx_register_event_callbacks failed: %s", esp_err_to_name(err));
        return;
    }

    err = rmt_enable(rx_chan_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_enable(rx) failed: %s", esp_err_to_name(err));
        return;
    }

    ok_ = true;
    ESP_LOGI(TAG, "IR driver initialized (TX GPIO %d, RX GPIO %d, %lu Hz carrier)",
             SENSOR_IR_TX_PIN, SENSOR_IR_RX_PIN, (unsigned long)kIrCarrierHz);
}

IrDriver::~IrDriver() {
    if (rx_chan_) {
        rmt_disable(rx_chan_);
        rmt_del_channel(rx_chan_);
    }
    if (tx_chan_) {
        rmt_disable(tx_chan_);
        rmt_del_channel(tx_chan_);
    }
    if (nec_encoder_) rmt_del_encoder(nec_encoder_);
    if (rx_queue_) vQueueDelete(rx_queue_);
}

esp_err_t IrDriver::Emit(const std::string& protocol, uint16_t address, uint16_t command) {
    if (!ok_) return ESP_ERR_INVALID_STATE;
    if (protocol != "NEC") {
        ESP_LOGW(TAG, "unsupported protocol: %s (only NEC for now)", protocol.c_str());
        return ESP_ERR_NOT_SUPPORTED;
    }

    ir_nec_scan_code_t scan_code = {address, command};
    esp_err_t err = rmt_transmit(tx_chan_, nec_encoder_, &scan_code,
                                 sizeof(scan_code), &transmit_config_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
        return err;
    }
    err = rmt_tx_wait_all_done(tx_chan_, 200);
    return err;
}

std::string IrDriver::LearnStart(uint32_t window_ms) {
    if (!ok_) return "";

    // Generate a short opaque handle so concurrent calls (shouldn't happen
    // in v1 but defensive) don't get confused.
    char buf[16];
    snprintf(buf, sizeof(buf), "ir-%08lx", (unsigned long)esp_random());
    learn_handle_ = buf;
    learn_started_at_ms_ = esp_timer_get_time() / 1000;
    learn_window_ms_ = window_ms;
    learn_consumed_ = false;

    // Cancel any in-flight RX from a previous LearnStart that timed out
    // without receiving a frame. rmt_receive() is one-shot: after a previous
    // learn window expires server-side without getting an IR signal, the
    // RMT RX channel is still "armed" and a fresh rmt_receive() returns
    // ESP_ERR_INVALID_STATE. Disable then re-enable cycles the channel
    // back to clean state. Errors are non-fatal — first call falls through.
    (void)rmt_disable(rx_chan_);
    esp_err_t err = rmt_enable(rx_chan_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_enable(rx) on LearnStart failed: %s", esp_err_to_name(err));
        return "";
    }

    // Drain any previous frames in the queue
    rmt_rx_done_event_data_t stale;
    while (xQueueReceive(rx_queue_, &stale, 0) == pdTRUE) {}

    // Begin RX
    rmt_receive_config_t rx_cfg = {};
    rx_cfg.signal_range_min_ns = 1250;            // shortest valid pulse ~1.25µs
    rx_cfg.signal_range_max_ns = 12000000;        // longest = 12ms (NEC header)
    err = rmt_receive(rx_chan_, s_rx_buffer, sizeof(s_rx_buffer), &rx_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_receive failed: %s", esp_err_to_name(err));
        return "";
    }

    ESP_LOGI(TAG, "learn mode started, handle=%s window=%lums",
             learn_handle_.c_str(), (unsigned long)window_ms);
    return learn_handle_;
}

bool IrDriver::LearnResult(const std::string& handle,
                           std::string* protocol, uint16_t* address, uint16_t* command) {
    if (!ok_ || handle != learn_handle_ || learn_consumed_) {
        return false;
    }

    rmt_rx_done_event_data_t evt;
    if (xQueueReceive(rx_queue_, &evt, 0) != pdTRUE) {
        // Nothing received — check timeout
        int64_t now_ms = esp_timer_get_time() / 1000;
        if ((uint32_t)(now_ms - learn_started_at_ms_) > learn_window_ms_) {
            learn_consumed_ = true;
            ESP_LOGI(TAG, "learn timeout — no IR signal in window");
        }
        return false;
    }

    learn_consumed_ = true;

    // Diagnostic dump: first 4 symbol pairs so we can see what the receiver
    // is actually catching (NEC, AC long-frame, garbage, etc.)
    {
        char buf[160];
        int off = snprintf(buf, sizeof(buf), "rx %u symbols:",
                           (unsigned)evt.num_symbols);
        unsigned dump_n = evt.num_symbols < 4 ? evt.num_symbols : 4;
        for (unsigned i = 0; i < dump_n && off < (int)sizeof(buf) - 32; ++i) {
            const rmt_symbol_word_t& s = evt.received_symbols[i];
            off += snprintf(buf + off, sizeof(buf) - off,
                            " [%u/%u, %u/%u]",
                            (unsigned)s.duration0, (unsigned)s.level0,
                            (unsigned)s.duration1, (unsigned)s.level1);
        }
        ESP_LOGI(TAG, "%s", buf);
    }

    if (!DecodeNecFrame(evt.received_symbols, evt.num_symbols, address, command)) {
        ESP_LOGW(TAG, "captured %u symbols but NEC decode failed",
                 (unsigned)evt.num_symbols);
        return false;
    }

    *protocol = "NEC";
    ESP_LOGI(TAG, "learned NEC: addr=0x%04x cmd=0x%04x", *address, *command);
    return true;
}

bool IrDriver::RmtRxDoneCallback(rmt_channel_handle_t channel,
                                 const rmt_rx_done_event_data_t* edata,
                                 void* user_data) {
    QueueHandle_t queue = (QueueHandle_t)user_data;
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static inline bool InRange(uint32_t value, uint32_t target, uint32_t tol) {
    return value >= (target > tol ? target - tol : 0) && value <= target + tol;
}

bool IrDriver::DecodeNecFrame(const rmt_symbol_word_t* symbols, size_t symbol_count,
                              uint16_t* address, uint16_t* command) {
    // NEC frame: 1 header symbol + 32 data symbols + 1 stop symbol = 34
    // Some receivers strip the stop, some merge it. Accept 33 or 34.
    if (symbol_count < 33) return false;

    const rmt_symbol_word_t& hdr = symbols[0];
    if (!InRange(hdr.duration0, 9000, 1000) || !InRange(hdr.duration1, 4500, 500)) {
        return false;
    }

    uint32_t bits = 0;
    for (int i = 0; i < 32; ++i) {
        const rmt_symbol_word_t& s = symbols[1 + i];
        if (!InRange(s.duration0, kNecBitHigh, kNecBitHighTol)) return false;
        if (InRange(s.duration1, kNecBit1Low, kNecBitLowTol)) {
            bits |= (1u << i);
        } else if (!InRange(s.duration1, kNecBit0Low, kNecBitLowTol)) {
            return false;
        }
    }

    // bits = command_inv:8 | command:8 | addr_inv:8 | addr:8  (LSB first)
    // Layout in NEC: addr (8), addr_inv (8), cmd (8), cmd_inv (8)
    uint16_t addr = bits & 0xFFFF;          // includes addr + addr_inv (or extended addr)
    uint16_t cmd = (bits >> 16) & 0xFFFF;   // includes cmd + cmd_inv
    *address = addr;
    *command = cmd;
    return true;
}
