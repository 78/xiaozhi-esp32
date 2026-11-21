"""Real country reader + cJSON: bounded bodies/timeouts, no unsafe fallback, close on all exits."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PetoiLocaleHttpTests(unittest.TestCase):
    def test_real_reader_closes_and_validates_every_response(self):
        compiler, c_compiler = shutil.which('g++'), shutil.which('gcc')
        cjson = ROOT / 'managed_components/espressif__cjson/cJSON'
        if not compiler or not c_compiler or not (cjson / 'cJSON.c').exists():
            self.skipTest('requires host C/C++17 compilers and the fetched cJSON source')
        source = (ROOT / 'main/boards/petoi/bittle-ai-c3/runtime_locale.cc').read_text(encoding='utf-8')
        begin = source.index('std::string ReadCountry(')
        method = source[begin:source.index('\n}  // namespace', begin)]
        driver = r'''
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include "cJSON.h"
#include "boards/petoi/bittle-ai-c3/runtime_locale_policy.h"
using petoi::IsFlatCountryObject;
constexpr size_t kMaximumBody=512;
#define CONFIG_PETOI_COUNTRY_LOOKUP_URL scenario.endpoint.c_str()
struct Scenario {
    std::string body, url; size_t cursor=0,declared=0;
    std::string endpoint="http://ip-api.com/json/?fields=status,countryCode";
    bool open=true,read=true,slow=false,fail_after_body=false; int status=200;
    unsigned closed=0,destroyed=0,read_calls=0; int64_t now=0;
} scenario;
int64_t esp_timer_get_time() { return scenario.now+=(scenario.slow ? 4000000 : 100); }
template<class T> struct Result {
    bool good; T value{};
    explicit operator bool() const { return good; }
    T operator*() const { return value; }
};
struct Http {
    ~Http() { ++scenario.destroyed; }
    void Close() { ++scenario.closed; }
    void SetKeepAlive(bool value) { if(value) std::abort(); }
    void SetTimeout(int value) { if(value<=0 || value>3000) std::abort(); }
    bool Open(const std::string& method,const std::string& url) {
        if(method!="GET") std::abort();
        scenario.url=url;return scenario.open;
    }
    Result<int> GetStatusCode() { return {true,scenario.status}; }
    size_t GetBodyLength() { return scenario.declared; }
    Result<int> Read(char* data,size_t capacity) {
        ++scenario.read_calls;
        if(!scenario.read) return {false,0};
        if(scenario.fail_after_body && scenario.cursor==scenario.body.size()) return {false,0};
        size_t count=std::min(capacity,scenario.body.size()-scenario.cursor);
        std::memcpy(data,scenario.body.data()+scenario.cursor,count);scenario.cursor+=count;
        return {true,static_cast<int>(count)};
    }
};
struct NetworkInterface { std::unique_ptr<Http> CreateHttp(int) { return std::make_unique<Http>(); } };
''' + method + r'''
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"check line %d\n",__LINE__); return 1; } } while(0)
int main() {
    NetworkInterface network;
    CHECK(ReadCountry(nullptr).empty());
    scenario={};scenario.endpoint="";
    CHECK(ReadCountry(&network).empty());
    CHECK(scenario.closed==0 && scenario.destroyed==0);
    for(const char* cc:{"CN","US"}) {
        scenario={}; scenario.body=std::string("{\"status\":\"success\",\"countryCode\":\"")+cc+"\"}";
        CHECK(ReadCountry(&network)==cc);
        CHECK(scenario.closed==1 && scenario.destroyed==1);
        CHECK(scenario.url=="http://ip-api.com/json/?fields=status,countryCode");
    }
    for(const char* invalid:{"", "{", "null", "{}", "{\"countryCode\":\"CN\"}",
        "{\"status\":\"fail\",\"countryCode\":\"CN\"}",
        "{\"status\":\"success\",\"countryCode\":12}",
        "{\"status\":\"success\",\"countryCode\":\"CHN\"}",
        "{\"status\":\"success\",\"countryCode\":\"CN\"}trailing",
        "{\"status\":\"success\\u0000ignored\",\"countryCode\":\"CN\"}",
        "{\"status\":\"success\",\"countryCode\":\"CN\",\"nested\":{}}"}) {
        scenario={};scenario.body=invalid;
        CHECK(ReadCountry(&network).empty());
        CHECK(scenario.closed==1 && scenario.destroyed==1);
    }
    for(int failure=0;failure<5;++failure) {
        scenario={};scenario.body="{\"status\":\"success\",\"countryCode\":\"CN\"}";
        if(failure==0) scenario.open=false;
        if(failure==1) scenario.status=503;
        if(failure==2) scenario.declared=513;
        if(failure==3) scenario.read=false;
        if(failure==4) scenario.slow=true;
        CHECK(ReadCountry(&network).empty());
        CHECK(scenario.closed==1 && scenario.destroyed==1);
    }
    scenario={};scenario.body=std::string(600,'x');
    CHECK(ReadCountry(&network).empty());
    CHECK(scenario.read_calls<=5 && scenario.closed==1 && scenario.destroyed==1);
    scenario={};scenario.body="{\"status\":\"success\",\"countryCode\":\"CN\"}";
    scenario.declared=scenario.body.size();scenario.fail_after_body=true;
    CHECK(ReadCountry(&network)=="CN" && scenario.read_calls==1);
    scenario={};scenario.body="{\"status\":\"success\",\"countryCode\":\"CN\"}";
    scenario.declared=scenario.body.size()+1;
    CHECK(ReadCountry(&network).empty() && scenario.closed==1);
    scenario={};scenario.body="{\"status\":\"success\",\"countryCode\":\"CN\"}";
    scenario.declared=1;
    CHECK(ReadCountry(&network).empty() && scenario.closed==1);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, obj, binary = path / 'test.cc', path / 'cjson.o', path / 'test.exe'
            cpp.write_text(driver, encoding='utf-8')
            subprocess.run([c_compiler, '-std=c99', '-I', str(cjson), '-c', str(cjson / 'cJSON.c'), '-o', str(obj)],
                           check=True, capture_output=True, text=True)
            compiled = subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(cjson), '-I', str(ROOT / 'main'),
                                       str(cpp), str(obj), '-o', str(binary)], capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
