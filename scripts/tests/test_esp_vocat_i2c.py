"""Run ESP-VoCat's peripheral reader against a host-side I2C failure double."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/espressif/esp-vocat/esp_vocat.cc"


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class EspVocatI2cTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "requires a host C++ compiler")
    def test_actual_reader_cache_and_touch_with_failed_transfers(self):
        board = BOARD.read_text()
        reader = function(board, "static esp_err_t ReadPeripheralRegs(")
        charge = "class Charge :" + board.split("class Charge :", 1)[1].split("class Cst816s", 1)[0]
        source = r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <vector>
using esp_err_t = int;
using i2c_master_dev_handle_t = void*;
using i2c_master_bus_handle_t = void*;
constexpr int ESP_OK=0, ESP_ERR_TIMEOUT=1, ESP_ERR_INVALID_RESPONSE=2, ESP_FAIL=3;
#define BIT0 1
#define ESP_LOGW(...)
#define ESP_LOGI(...)
#define ESP_LOGD(...)
#define ESP_ERROR_CHECK(result) assert((result)==ESP_OK)
#define pdMS_TO_TICKS(ms) (ms)
int64_t now_ms=100;
int reads=0, permanent_error=0;
std::vector<int> errors, waits;
uint16_t registers[256] = {};
uint8_t touch_data[6] = {1,0,42,0,24,0};
void* temp_sensor=nullptr; float tsens_value=0;
int temperature_sensor_get_celsius(void*,float*) { return 0; }
void vTaskDelay(int ms) { waits.push_back(ms); }
int64_t esp_timer_get_time() { return now_ms*1000; }
const char* esp_err_to_name(int) { return "fake"; }
int i2c_master_transmit_receive(void*,const uint8_t* reg,size_t,uint8_t* data,size_t len,int timeout) {
 assert(timeout==50);
 const int error=reads < static_cast<int>(errors.size()) ? errors[reads] : permanent_error;
 ++reads;
 if(error) { std::memset(data,0xff,len); return error; }
 if(len==6) std::memcpy(data,touch_data,6);
 else { data[0]=registers[*reg]&255; data[1]=registers[*reg]>>8; }
 return 0;
}
class I2cDevice {
 public: I2cDevice(void*,uint8_t) {}
 protected: void* i2c_device_=nullptr;
};
'''
        source += reader + charge + r'''
class Touch {
 public:
 struct Point { int num=0,x=-1,y=-1; } tp_;
 enum TouchEvent { TOUCH_NONE,TOUCH_PRESS,TOUCH_RELEASE,TOUCH_HOLD };
 bool was_touched_=false; int press_count_=0;
 uint8_t data_[6] = {}; uint8_t* read_buffer_=data_; void* i2c_device_=nullptr;
'''
        source += function(board, "bool UpdateTouchPoint()")
        source += function(board, "TouchEvent CheckTouchEvent()") + "};\n"
        source += r'''
int main() {
 Charge charge(nullptr,0x55);
 int level=99; bool charging=true,discharging=false;
 assert(!charge.GetSnapshot(level,charging,discharging) && reads==0);
 registers[0x2c]=50; registers[0x0a]=1;
 registers[0x14]=static_cast<uint16_t>(-40); registers[0x0c]=static_cast<uint16_t>(-50);
 errors={ESP_ERR_TIMEOUT,ESP_ERR_INVALID_RESPONSE,0};
 assert(charge.Update() && reads==6 && waits==std::vector<int>({10,20}));
 assert(charge.GetSnapshot(level,charging,discharging) && reads==6);
 assert(level==50 && !charging && discharging);
 errors.clear(); reads=0; waits.clear(); permanent_error=ESP_ERR_TIMEOUT;
 assert(!charge.Update() && reads==3 && waits==std::vector<int>({10,20}));
 assert(charge.GetSnapshot(level,charging,discharging) && level==50);
 reads=0; waits.clear(); permanent_error=42;
 assert(!charge.Update() && reads==1 && waits.empty());
 now_ms=10100; assert(charge.GetSnapshot(level,charging,discharging));
 now_ms=10101; assert(!charge.GetSnapshot(level,charging,discharging));
 permanent_error=0; registers[0x2c]=60; registers[0x0a]=0;
 assert(charge.Update() && charge.GetSnapshot(level,charging,discharging));
 assert(level==60 && charging && !discharging);
 registers[0x2c]=65535;
 assert(!charge.Update() && charge.GetSnapshot(level,charging,discharging) && level==60);
 registers[0x2c]=0; registers[0x0a]=1;
 assert(charge.Update() && charge.GetSnapshot(level,charging,discharging) && level==0);

 Touch touch; reads=0; waits.clear();
 assert(touch.UpdateTouchPoint() && touch.CheckTouchEvent()==Touch::TOUCH_PRESS);
 assert(touch.tp_.x==42 && touch.tp_.y==24 && touch.press_count_==1);
 permanent_error=ESP_ERR_INVALID_RESPONSE;
 assert(!touch.UpdateTouchPoint() && reads==2 && waits.empty());
 assert(touch.tp_.num==1 && touch.was_touched_ && touch.press_count_==1);
 permanent_error=0; touch_data[0]=0;
 assert(touch.UpdateTouchPoint() && touch.CheckTouchEvent()==Touch::TOUCH_RELEASE);
 assert(touch.press_count_==1);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            binary = str(pathlib.Path(tmp) / "peripheral-test")
            subprocess.run(["c++", "-std=c++17", "-fsanitize=undefined", "-x", "c++", "-",
                            "-o", binary], input=source, text=True, check=True)
            subprocess.run([binary], check=True)

    def test_failed_samples_do_not_dispatch_touch_or_battery_alerts(self):
        board = BOARD.read_text()
        self.assertIn("if (!touchpad->UpdateTouchPoint())", board)
        touch = function(board, "static void touch_event_task(")
        self.assertLess(touch.index("if (!touchpad->UpdateTouchPoint())"),
                        touch.index("CheckTouchEvent()"))
        battery = function(board, "static void battery_task(")
        self.assertIn("if (healthy)", battery)
        self.assertIn("healthy ? 2000 : 5000", battery)
        getter = function(board, "virtual bool GetBatteryLevel(")
        self.assertIn("GetSnapshot", getter)
        self.assertNotIn("ReadWord", getter)
        self.assertNotIn("ReadPeripheralRegs", getter)
