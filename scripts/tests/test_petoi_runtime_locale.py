"""Product startup locale policy: opaque SSID cache and bounded valid GeoIP decisions."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PetoiRuntimeLocaleTests(unittest.TestCase):
    def test_cache_and_failed_country_lookup_never_invent_language_changes(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('requires a host C++17 compiler')
        driver = r'''
#include <cstdio>
#include <string>
#include "boards/petoi/bittle-ai-c3/runtime_locale_policy.h"
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"check line %d\n",__LINE__); return 1; } } while(0)
int main() {
    using namespace petoi;
    const auto zh=ProductLocale::Chinese, en=ProductLocale::English;
    CHECK(CachedLocale("zh-CN",en)==zh);
    CHECK(CachedLocale("en-US",zh)==en);
    CHECK(CachedLocale("unknown",zh)==zh);
    CHECK(CachedLocale("",en)==en);
    CHECK(std::string(LocaleCode(zh))=="zh-CN");
    CHECK(std::string(LocaleCode(en))=="en-US");
    CHECK(!NeedsCountryLookup("zh-CN","B","B"));
    CHECK(NeedsCountryLookup("zh-CN","B","B "));
    CHECK(NeedsCountryLookup("zh-CN","B","b"));
    CHECK(NeedsCountryLookup("","B","B"));
    CHECK(!NeedsCountryLookup("en-US","B",""));
    CHECK(!NeedsCountryLookup("en-US","B",std::string(33,'x')));
    auto d=DecideLocale("zh-CN","B","B","US",en);
    CHECK(d.locale==zh && !d.persist); // Same SSID uses its resolved cache.
    d=DecideLocale("en-US","A","B","CN",en);
    CHECK(d.locale==zh && d.persist);
    d=DecideLocale("zh-CN","A","B","US",zh);
    CHECK(d.locale==en && d.persist);
    for(const char* bad:{"","C","CHN","cn","CN\n","??","12"}) {
        d=DecideLocale("zh-CN","A","B",bad,en);
        CHECK(d.locale==zh && !d.persist);
        d=DecideLocale("en-US","A","B",bad,zh);
        CHECK(d.locale==en && !d.persist);
        d=DecideLocale("","","B",bad,zh);
        CHECK(d.locale==zh && !d.persist);
    }
    d=DecideLocale("","","B","CN",en);
    CHECK(d.locale==zh && d.persist);
    d=DecideLocale("zh-CN","B","","US",en);
    CHECK(d.locale==zh && !d.persist);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, binary = path / 'test.cc', path / 'test.exe'
            cpp.write_text(driver, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT / 'main'), str(cpp), '-o', str(binary)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
