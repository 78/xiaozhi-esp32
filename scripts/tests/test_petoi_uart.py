"""Exercise the board UART queue/error paths using the real C++ implementation."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class PetoiUartTest(unittest.TestCase):
    def test_nonblocking_queue_and_transmit_failure(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        board = Path(__file__).resolve().parents[2] / "main/boards/petoi/bittle-ai-c3"
        stub = r'''
#pragma once
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NO_MEM=1, ESP_ERR_INVALID_ARG=2;
constexpr int ESP_ERR_INVALID_STATE=3, ESP_ERR_TIMEOUT=4;
constexpr int UART_NUM_1=1, UART_DATA_8_BITS=8, UART_PARITY_DISABLE=0;
constexpr int UART_STOP_BITS_1=1, UART_HW_FLOWCTRL_DISABLE=0;
constexpr int UART_SCLK_DEFAULT=0, UART_PIN_NO_CHANGE=-1;
constexpr int GPIO_NUM_21=21, GPIO_NUM_20=20;
constexpr int pdTRUE=1, pdPASS=1;
using QueueHandle_t = void*;
struct uart_config_t {
    int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk;
};
inline int pdMS_TO_TICKS(int value) { return value; }
inline const char* esp_err_to_name(int error) { return error == ESP_OK ? "ESP_OK" : "TX_ERROR"; }
#define ESP_LOGE(...) ((void)0)
#define ESP_LOG_BUFFER_HEX_LEVEL(...) ((void)0)
#define ESP_LOG_DEBUG 0
inline int config_error=ESP_OK, deleted=0, queue_mode=0, transmit_count=0;
inline bool fail_queue=false, fail_task=false;
inline std::string wire;
inline void (*worker)(void*)=nullptr;
inline void* worker_context=nullptr;
inline unsigned char queued[256];
inline size_t queued_size=0;
inline int uart_param_config(int port, const uart_config_t* config) {
    assert(port == UART_NUM_1 && config->baud_rate == 9600);
    return config_error;
}
inline int uart_set_pin(int, int tx, int rx, int, int) {
    assert(tx == 21 && rx == 20); return ESP_OK;
}
inline int uart_driver_install(int, int rx, int tx, int, void*, int) {
    assert(rx == 1024 && tx == 0); return ESP_OK;
}
inline int uart_driver_delete(int) { ++deleted; return ESP_OK; }
inline QueueHandle_t xQueueCreate(int count, size_t size) {
    assert(count == 4 && size <= sizeof(queued));
    queued_size=size; return fail_queue ? nullptr : queued;
}
inline void vQueueDelete(QueueHandle_t) {}
inline int xTaskCreate(void (*fn)(void*), const char*, int, void* arg, int, void*) {
    worker=fn; worker_context=arg; return fail_task ? 0 : pdPASS;
}
inline int xQueueSend(QueueHandle_t, const void* frame, int timeout) {
    assert(timeout == 0);  // MCP callback must never wait on queue capacity.
    if (queue_mode == 1) return 0;
    std::memcpy(queued, frame, queued_size); return pdTRUE;
}
inline unsigned uxQueueMessagesWaiting(QueueHandle_t) { return 0; }
struct StopWorker {};
inline int xQueueReceive(QueueHandle_t, void* frame, int timeout) {
    assert(timeout == 20);
    if (transmit_count) throw StopWorker{};
    std::memcpy(frame, queued, queued_size); return pdTRUE;
}
inline int uart_write_bytes(int, const void* data, size_t size) {
    ++transmit_count; wire.assign(static_cast<const char*>(data), size);
    return static_cast<int>(size)-1;  // Simulate a short write.
}
inline int uart_wait_tx_done(int, int) { assert(false); return ESP_OK; }
inline int uart_read_bytes(int, void*, size_t, int timeout) {
    assert(timeout == 0); return 0;
}
'''
        program = r'''
#include "robot_uart.h"
int main() {
    petoi::RobotUart not_started;
    assert(not_started.Enqueue("kup") == ESP_ERR_INVALID_STATE);
    assert(not_started.Enqueue("kup\n") == ESP_ERR_INVALID_ARG);
    config_error=ESP_FAIL;
    assert(not_started.Initialize() == ESP_FAIL);
    config_error=ESP_OK;
    fail_queue=true;
    assert(not_started.Initialize() == ESP_ERR_NO_MEM && deleted == 1);
    fail_queue=false; fail_task=true;
    assert(not_started.Initialize() == ESP_ERR_NO_MEM && deleted == 2);
    fail_task=false;
    petoi::RobotUart uart;
    assert(uart.Initialize() == ESP_OK);
    queue_mode=1;
    assert(uart.Enqueue("kup") == ESP_ERR_TIMEOUT);
    queue_mode=0;
    assert(uart.Enqueue("kup") == ESP_OK);
    try { worker(worker_context); } catch (const StopWorker&) {}
    assert(transmit_count == 1);  // Failed physical writes must not auto-retry.
    assert(wire == std::string("XA\x15" "kup",6));
    assert(uart.GetStatus().find("last_tx_error=TX_ERROR") != std::string::npos);
    assert(uart.GetStatus().find("transmitting=false") != std::string::npos);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "stub.h").write_text(stub, encoding="utf-8")
            for name in ("driver/uart.h", "driver/gpio.h", "esp_log.h",
                         "freertos/FreeRTOS.h", "freertos/queue.h", "freertos/task.h"):
                header = root / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "stub.h"\n', encoding="utf-8")
            source = root / "uart_test.cc"
            source.write_text(program, encoding="utf-8")
            binary = root / "uart_test.exe"
            subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(root), "-I", str(board), str(source), "-o", str(binary)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
