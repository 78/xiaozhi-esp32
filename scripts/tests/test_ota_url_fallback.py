"""Verify OTA URL selection uses shared validation before any HTTP request."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which('c++'), 'requires a host C++ compiler')
class OtaUrlFallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        build = Path(cls.tmp.name)
        source = (ROOT / 'main/ota.cc').read_text()
        start = source.index('std::string Ota::GetCheckVersionUrl()')
        end = source.index('std::unique_ptr<Http> Ota::SetupHttp()', start)
        # Validation itself is covered by the component tests. Control its result here
        # to check that the firmware honors rejection without modifying saved settings.
        driver = r'''
#include <cassert>
#include <iostream>
#include <string>
#define CONFIG_OTA_URL "https://default.example/ota/"
#define ESP_LOGW(...) ((void)0)
std::string saved;
bool valid;
namespace wifi_config { bool IsValidOtaUrl(const std::string&) {return valid;} }
struct Settings {
    Settings(const char*, bool write) {assert(!write);}
    std::string GetString(const char*) {return saved;}
};
struct Ota { std::string GetCheckVersionUrl(); };
''' + source[start:end] + r'''
int main(int argc, char** argv) {
    assert(argc == 3);
    saved = argv[1]; valid = std::string(argv[2]) == "valid";
    Ota ota;
    std::cout << ota.GetCheckVersionUrl();
    assert(saved == argv[1]);
}
'''
        (build / 'test.cc').write_text(driver)
        cls.binary = build / 'test'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        str(build / 'test.cc'), '-o', str(cls.binary)], check=True)

    def select(self, url, valid):
        return subprocess.check_output([str(self.binary), url, 'valid' if valid else 'invalid'],
                                       text=True)

    def test_empty_url_uses_default(self):
        self.assertEqual(self.select('', True), 'https://default.example/ota/')

    def test_valid_custom_url_is_preserved(self):
        for url in ('http://a', 'https://custom.example/ota/', 'http://192.168.1.2:8000/ota/'):
            self.assertEqual(self.select(url, True), url)

    def test_rejected_saved_url_uses_default(self):
        for url in ('Huawei-1ct5dn', 'ftp://example.com/', 'https://', 'x' * 256):
            self.assertEqual(self.select(url, False), 'https://default.example/ota/')


if __name__ == '__main__':
    unittest.main()
