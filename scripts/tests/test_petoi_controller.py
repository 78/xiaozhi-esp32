"""Real UART actor: local/cloud dedup, token timeout, cancellation, no retry."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PetoiControllerTests(unittest.TestCase):
    def test_uart_actor_and_deferred_completions(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("requires a host C++17 compiler")
        stub = r'''
#pragma once
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NO_MEM=1,UART_NUM_1=1,UART_DATA_8_BITS=8,UART_PARITY_DISABLE=0;
constexpr int UART_STOP_BITS_1=1,UART_HW_FLOWCTRL_DISABLE=0,UART_SCLK_DEFAULT=0,UART_PIN_NO_CHANGE=-1;
constexpr int pdTRUE=1,pdPASS=1;
constexpr int GPIO_NUM_21=21,GPIO_NUM_20=20;
inline int pdMS_TO_TICKS(int n) { return n; }
struct uart_config_t { int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk; };
struct Queue { size_t size,capacity; std::deque<std::vector<unsigned char>> data; };
using QueueHandle_t=Queue*;
struct StopWorker {};
inline bool fail_queue_send=false,provide_token=true,short_write=false;
inline int64_t clock_us=0;
inline unsigned rx_calls=0;
inline std::vector<std::string> wire;
inline std::function<void()> on_write;
inline void (*worker)(void*)=nullptr;
inline void* worker_context=nullptr;
inline QueueHandle_t xQueueCreate(size_t count,size_t size) { return new Queue{size,count,{}}; }
inline int xQueueSend(QueueHandle_t q,const void* value,int) {
    if(fail_queue_send || q->data.size()==q->capacity) return 0;
    auto begin=static_cast<const unsigned char*>(value); q->data.emplace_back(begin,begin+q->size); return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q,void* value,int wait) {
    if(q->data.empty()) { if(wait) throw StopWorker{}; return 0; }
    std::memcpy(value,q->data.front().data(),q->size); q->data.pop_front(); return pdTRUE;
}
inline unsigned uxQueueMessagesWaiting(QueueHandle_t q) { return q->data.size(); }
inline void vQueueDelete(QueueHandle_t q) { delete q; }
inline int xTaskCreate(void (*f)(void*),const char*,int,void* p,int,void*) { worker=f;worker_context=p;return pdPASS; }
inline int uart_param_config(int,uart_config_t*) { return ESP_OK; }
inline int uart_set_pin(int,int,int,int,int) { return ESP_OK; }
inline int uart_driver_install(int,int,int,int,void*,int) { return ESP_OK; }
inline void uart_driver_delete(int) {}
inline void uart_flush_input(int) {}
inline int uart_write_bytes(int,const char* bytes,size_t size) {
    wire.emplace_back(bytes,size);
    if(on_write) { auto callback=std::move(on_write);on_write=nullptr;callback(); }
    return short_write ? static_cast<int>(size)-1 : static_cast<int>(size);
}
inline int uart_wait_tx_done(int,int) { return ESP_OK; }
inline int uart_read_bytes(int,uint8_t* bytes,size_t,int) {
    ++rx_calls;clock_us+=20000;
    if(!provide_token) return 0;
    bytes[0]=wire.back()[3];return 1;
}
inline int64_t esp_timer_get_time() { return clock_us; }
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
struct Application {
    uint32_t epoch=1;
    std::deque<std::function<void()>> pending;
    static Application& GetInstance() { static Application app;return app; }
    uint32_t GetMcpSessionEpoch() const { return epoch; }
    void Schedule(std::function<void()> callback) { pending.push_back(std::move(callback)); }
    void Drain() { while(!pending.empty()) { auto f=std::move(pending.front());pending.pop_front();f(); } }
};
inline void run_worker() { try { worker(worker_context); } catch(const StopWorker&) {} }
'''
        driver = r'''
#include <cassert>
#include <cstdio>
#include "robot_controller.h"
#undef assert
#define assert(value) do { if (!(value)) { std::fprintf(stderr,"check line %d: %s\n",__LINE__,#value); return 1; } } while (0)
int main() {
    petoi::RobotController controller;
    assert(controller.Initialize()==ESP_OK);
    auto& app=Application::GetInstance();
    unsigned callbacks=0;
    petoi::RobotResult last;
    auto done=[&](petoi::RobotResult result) { ++callbacks;last=result; };
    controller.BeginTurn(); controller.Submit("kup","1",false,done);
    run_worker(); assert(wire.size()==1 && callbacks==0 && rx_calls==0);
    app.Drain(); assert(callbacks==1 && last.code==petoi::RobotResultCode::Transmitted);
    controller.Submit("kup","2",false,done); assert(callbacks==2 && wire.size()==1);
    controller.BeginTurn(); controller.Submit("kup","3",false,done);
    run_worker();app.Drain();assert(wire.size()==2); // cross-turn repeat remains valid
    wire.clear(); callbacks=0;
    controller.BeginTurn("kup,ksit,d");
    controller.Submit("kup","4",false,done);
    controller.Submit("ksit","5",false,done);
    controller.Submit("d","6",false,done);
    controller.Submit("kup","7",false,done); // duplicate joins; never retransmits
    unsigned midway_callbacks=100;
    on_write=[&]() { on_write=[&]() { app.Drain();midway_callbacks=callbacks; }; };
    run_worker();assert(wire.size()==3 && callbacks==0);
    app.Drain();assert(midway_callbacks==0 && callbacks==4 && last.code==petoi::RobotResultCode::TokenObserved);
    assert(wire[0]==std::string("XA\x15" "kup",6));
    assert(wire[2]==std::string("XA\x15" "d",4));
    wire.clear();callbacks=0;provide_token=false;
    controller.BeginTurn(); controller.Submit("kup,ksit,d","8",false,done);
    run_worker();app.Drain();assert(wire.size()==1 && callbacks==1);
    assert(last.code==petoi::RobotResultCode::Timeout && last.requested==3 && last.completed==0);
    controller.Submit("kup,ksit,d","9",false,done);
    assert(wire.size()==1 && callbacks==2); // timeout may have executed: do not retry
    wire.clear();callbacks=0;provide_token=true;short_write=true;
    controller.BeginTurn();controller.Submit("kup,ksit","10",false,done);
    run_worker();app.Drain();assert(wire.size()==1 && last.code==petoi::RobotResultCode::TxError);
    short_write=false;wire.clear();callbacks=0;
    controller.BeginTurn();controller.Submit("kup,ksit,d","11",false,done);
    on_write=[&]() { controller.BeginTurn("d"); };
    run_worker();app.Drain();assert(wire.size()==2 && callbacks==1);
    assert(last.code==petoi::RobotResultCode::Cancelled && wire.back()==std::string("XA\x15" "d",4));
    controller.Submit("d","12",false,done);assert(wire.size()==2 && last.ok());
    wire.clear();callbacks=0;
    controller.BeginTurn();controller.Submit("kup,ksit","13",false,done);
    on_write=[&]() { ++app.epoch; };
    run_worker();app.Drain();assert(wire.size()==1 && callbacks==1);
    assert(last.code==petoi::RobotResultCode::Cancelled);
    wire.clear();callbacks=0;fail_queue_send=true;
    controller.BeginTurn();controller.Submit("kup","14",false,done);
    assert(callbacks==1 && last.code==petoi::RobotResultCode::Rejected && wire.empty());
    fail_queue_send=false;
    controller.BeginTurn();controller.Submit("kup","15",false,done);
    controller.Submit("ksit","15",false,done);
    assert(last.code==petoi::RobotResultCode::Rejected); // RPC id changed payload
    run_worker();app.Drain();assert(wire.size()==1);
}
'''
        board = ROOT / "main/boards/petoi/bittle-ai-c3"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "stub.h").write_text(stub, encoding="utf-8")
            for name in ("application.h", "driver/uart.h", "driver/gpio.h", "esp_timer.h",
                         "esp_log.h", "freertos/FreeRTOS.h", "freertos/queue.h", "freertos/task.h"):
                target = path / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('#include "stub.h"\n', encoding="utf-8")
            shutil.copy2(board / "robot_controller.h", path / "robot_controller.h")
            source, binary = path / "test.cc", path / "test.exe"
            source.write_text(driver, encoding="utf-8")
            result = subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(path),
                            "-I", str(board), str(source), "-o", str(binary)],
                           capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
