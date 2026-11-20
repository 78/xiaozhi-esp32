"""Exercise the real shared MCP envelope under a per-allocation heap limit."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class McpEnvelopeMemoryTests(unittest.TestCase):
    def test_large_payload_has_no_doubled_allocation(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('requires a host C++17 compiler')
        source = (ROOT / 'main/protocols/protocol.cc').read_text(encoding='utf-8')
        method = source[source.index('void Protocol::SendMcpMessage('):
                        source.index('bool Protocol::IsTimeout()')]
        driver = r'''
#include <cstdlib>
#include <cstdio>
#include <new>
#include <string>
#include <limits>
static size_t allocation_limit = std::numeric_limits<size_t>::max();
void* operator new(size_t n) {
    if (n > allocation_limit) throw std::bad_alloc();
    if (auto p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
struct Protocol {
    std::string session_id_, expected;
    bool matched = false;
    void SendText(const std::string& s) { matched = s == expected; }
    void SendMcpMessage(const std::string& payload);
};
''' + method + r'''
int main() {
    // Shared serializer feeds both MQTT and WebSocket; preserve every wire byte.
    for (size_t length : {size_t(0), size_t(16), size_t(2048), size_t(4203), size_t(7990)}) {
        for (const char* session : {"", "session-12345678"}) {
            Protocol protocol;
            protocol.session_id_ = session;
            std::string payload = "{\"value\":\"" + std::string(length, 'x') + "\"}";
            protocol.expected = "{\"session_id\":\"" + protocol.session_id_ +
                "\",\"type\":\"mcp\",\"payload\":" + payload + "}";
            allocation_limit = protocol.expected.size() + 1;
            try { protocol.SendMcpMessage(payload); }
            catch (const std::bad_alloc&) { std::fprintf(stderr,"oversized allocation\n"); return 1; }
            allocation_limit = std::numeric_limits<size_t>::max();
            if (!protocol.matched) { std::fprintf(stderr,"wire bytes changed\n"); return 2; }
        }
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, binary = path / 'test.cc', path / 'test.exe'
            cpp.write_text(driver, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(cpp), '-o', str(binary)], check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
