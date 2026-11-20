"""Actual persistence helper must not retain an old SSID paired with a new language."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PetoiLocaleCacheTests(unittest.TestCase):
    def test_failed_writes_invalidate_ssid_without_aborting(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('requires a host C++17 compiler')
        source = (ROOT / 'main/boards/petoi/bittle-ai-c3/runtime_locale.cc').read_text(encoding='utf-8')
        method = source[source.index('bool StoreLocale('):source.index('std::string ReadCountry(')]
        driver = r'''
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
using nvs_handle_t=int;
constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,NVS_READWRITE=2;
std::map<std::string,std::string> values;
int fail=0,closed=0;
int nvs_open(const char* ns,int mode,nvs_handle_t* handle) {
    if(std::strcmp(ns,"language") || mode!=NVS_READWRITE) return -2;
    if(fail==1) return -1;
    *handle=1;return ESP_OK;
}
int nvs_erase_key(int,const char* key) { return values.erase(key) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND; }
int nvs_set_str(int,const char* key,const char* value) {
    if((fail==2 && std::strcmp(key,"code")==0) || (fail==3 && std::strcmp(key,"last_ssid")==0)) return -1;
    values[key]=value;return ESP_OK;
}
int nvs_commit(int) { return fail==4 ? -1 : ESP_OK; }
void nvs_close(int) { ++closed; }
''' + method + r'''
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"check line %d\n",__LINE__); return 1; } } while(0)
int main() {
    values={{"code","zh-CN"},{"last_ssid","A"}};
    CHECK(StoreLocale("en-US","B"));
    CHECK(values["code"]=="en-US" && values["last_ssid"]=="B" && closed==1);
    for(int fault=1;fault<=4;++fault) {
        fail=fault;closed=0;values={{"code","zh-CN"},{"last_ssid","A"}};
        CHECK(!StoreLocale("en-US","B"));
        if(fault==1) CHECK(values["code"]=="zh-CN" && values["last_ssid"]=="A" && closed==0);
        else CHECK(values.count("last_ssid")==0 && closed==1);
    }
    fail=0;values.clear();
    CHECK(StoreLocale("zh-CN","B"));
    CHECK(values["code"]=="zh-CN" && values["last_ssid"]=="B");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, binary = path / 'test.cc', path / 'test.exe'
            cpp.write_text(driver, encoding='utf-8')
            compiled = subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
