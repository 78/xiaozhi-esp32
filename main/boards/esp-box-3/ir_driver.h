#ifndef _IR_DRIVER_H_
#define _IR_DRIVER_H_

#include <driver/rmt_tx.h>
#include <driver/rmt_rx.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <string>

extern "C" {
#include "ir_nec_encoder.h"
}

// IR transmitter + receiver driver for the BOX-3 SENSOR-02 sub-board.
//
// TX: RMT TX channel @ 1MHz tick + 38kHz carrier modulation drives Q1
// (L8050) → IR67-21C IR LED on GPIO 39.
// RX: RMT RX channel on GPIO 38 (IRM-H638T receiver, idle-high), captures
// timing pulses into a queue, decoded by the NEC parser.
//
// Currently only NEC protocol is supported (most consumer remotes — TVs,
// fans, ACs from Haier/Midea/etc.). RC5/Samsung can be added behind the
// same Emit/LearnResult API.
//
// Lifetime: one global instance, owned by the board. Methods are NOT
// re-entrant; the only consumer is the MCP tool callbacks on the main
// task, so no locking is added.

class IrDriver {
public:
    IrDriver();
    ~IrDriver();

    // Returns ESP_OK on success. Synchronously blocks for ~70ms (one NEC
    // frame is 67.5ms). protocol must be "NEC" in v1.
    esp_err_t Emit(const std::string& protocol, uint16_t address, uint16_t command);

    // Begin learn mode. Caller passes a 5s timeout (default). The next
    // call to LearnResult will return the decoded frame if one was seen,
    // or empty if timeout.
    // Returns a handle string the caller passes back to LearnResult.
    std::string LearnStart(uint32_t window_ms = 5000);

    // Pop the most recently learned (protocol, address, command). Returns
    // true on success and writes outputs. Returns false if no result yet
    // or learn session has expired.
    bool LearnResult(const std::string& handle,
                     std::string* protocol, uint16_t* address, uint16_t* command);

private:
    bool ok_;
    rmt_channel_handle_t tx_chan_;
    rmt_channel_handle_t rx_chan_;
    rmt_encoder_handle_t nec_encoder_;
    rmt_transmit_config_t transmit_config_;
    QueueHandle_t rx_queue_;

    // Learn session state — only one in flight at a time.
    std::string learn_handle_;
    int64_t learn_started_at_ms_;
    uint32_t learn_window_ms_;
    bool learn_consumed_;

    static bool RmtRxDoneCallback(rmt_channel_handle_t channel,
                                  const rmt_rx_done_event_data_t* edata,
                                  void* user_data);

    bool DecodeNecFrame(const rmt_symbol_word_t* symbols, size_t symbol_count,
                        uint16_t* address, uint16_t* command);
};

#endif  // _IR_DRIVER_H_
