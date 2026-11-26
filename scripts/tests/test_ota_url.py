"""Run the XiaoZhi OTA URL validator on the host."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CASES = json.loads((ROOT / 'scripts/tests/ota_url_cases.json').read_text())
PREFIX = 'https://example.com/'
CASES += [dict(url=PREFIX + 'a' * (n - len(PREFIX)), valid=n == 255) for n in (255, 256)]


class UrlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        idf = Path(os.environ.get('IDF_PATH', '/nonexistent'))
        parser = idf / 'components/http_parser'
        if not shutil.which('c++') or not (parser / 'http_parser.c').exists():
            raise unittest.SkipTest('requires a host C/C++ compiler and exported ESP-IDF')
        tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(tmp.cleanup)
        build = Path(tmp.name)
        parser_object = build / 'parser.o'
        subprocess.run(['cc', '-c', str(parser / 'http_parser.c'), '-o',
                        str(parser_object)], check=True)

        # Compile the firmware validator without the rest of the OTA stack.
        source = (ROOT / 'main/ota.cc').read_text()
        start = source.index('bool Ota::IsValidUrl(')
        end = source.index('Ota::Ota()', start)
        header = (ROOT / 'main/ota.h').read_text()
        max_length = next(line for line in header.splitlines() if 'kMaxUrlLength =' in line)
        driver = build / 'validate.cc'
        driver.write_text('''
#include <cctype>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <strings.h>
#include "http_parser.h"
struct Ota {
''' + max_length + '''
    static bool IsValidUrl(std::string_view url);
};
''' + source[start:end] + '''
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::cout << Ota::IsValidUrl(argv[1]);
}
''')
        cls.binary = build / 'validate'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I',
                        str(parser), str(driver), str(parser_object), '-o',
                        str(cls.binary)], check=True)

    def test_url_cases(self):
        for case in CASES:
            with self.subTest(url=case['url']):
                out = subprocess.check_output([str(self.binary), case['url']], text=True)
                self.assertEqual(out, '1' if case['valid'] else '0')
