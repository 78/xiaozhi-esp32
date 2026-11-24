import json
import os
import shlex
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

DRIVER = r'''
#include <cstdio>
#include <cstring>
#include <expected>
#include <string>
#include <vector>

#include "cJSON.h"
#include "web_search_format.h"

struct StubError {
    std::string message;
    std::string ToString() const { return message; }
};

template <typename T>
using StubResult = std::expected<T, StubError>;

struct StubHttp {
    std::vector<std::string> chunks;
    bool fail = false;
    size_t index = 0;

    StubResult<int> Read(char* buffer, size_t buffer_size) {
        if (fail) {
            return std::unexpected(StubError{"read failed"});
        }
        if (index >= chunks.size()) {
            return 0;
        }
        const std::string& chunk = chunks[index++];
        size_t n = chunk.size() < buffer_size ? chunk.size() : buffer_size;
        std::memcpy(buffer, chunk.data(), n);
        return static_cast<int>(n);
    }
};

static std::string Repeat(const std::string& unit, int times) {
    std::string out;
    for (int i = 0; i < times; i++) {
        out += unit;
    }
    return out;
}

int main() {
    cJSON* out = cJSON_CreateObject();

    // TruncateUtf8: ASCII unchanged, CJK boundary, emoji, zero.
    cJSON_AddStringToObject(out, "truncate_ascii", TruncateUtf8("hello", 10).c_str());
    std::string cjk = "a" + Repeat("\xe4\xb8\xad", 100);  // 301 bytes
    std::string cjk_cut = TruncateUtf8(cjk, 300);
    cJSON_AddStringToObject(out, "truncate_cjk", cjk_cut.c_str());
    cJSON_AddNumberToObject(out, "truncate_cjk_len", (double)cjk_cut.size());
    cJSON_AddStringToObject(out, "truncate_emoji",
                            TruncateUtf8("\xf0\x9f\x99\x82", 3).c_str());  // "🙂" cut mid-char
    cJSON_AddStringToObject(out, "truncate_zero", TruncateUtf8("abc", 0).c_str());

    // ReadBounded: chunks, overflow, empty, exact cap, read error.
    StubHttp chunks;
    chunks.chunks = {"abc", "def"};
    auto read_chunks = ReadBounded(chunks, 100);
    cJSON_AddStringToObject(out, "read_chunks", read_chunks ? read_chunks->c_str() : "<err>");

    StubHttp overflow;
    overflow.chunks = {"1234567890", "1234567890"};
    auto read_overflow = ReadBounded(overflow, 15);
    cJSON_AddBoolToObject(out, "read_overflow_is_error", !read_overflow);
    cJSON_AddStringToObject(out, "read_overflow_msg",
                            read_overflow ? "" : read_overflow.error().c_str());

    StubHttp empty;
    auto read_empty = ReadBounded(empty, 15);
    cJSON_AddStringToObject(out, "read_empty", read_empty ? read_empty->c_str() : "<err>");

    StubHttp exact;
    exact.chunks = {"1234567890", "1234567890"};
    auto read_exact = ReadBounded(exact, 20);
    cJSON_AddStringToObject(out, "read_exact", read_exact ? read_exact->c_str() : "<err>");

    StubHttp broken;
    broken.chunks = {"abc"};
    broken.fail = true;
    auto read_broken = ReadBounded(broken, 20);
    cJSON_AddBoolToObject(out, "read_error_is_error", !read_broken);

    // FormatWebSearchResults: parse error and empty results.
    auto format_error = FormatWebSearchResults("not json {", "nothing");
    cJSON_AddBoolToObject(out, "format_parse_is_error", !format_error);

    auto format_empty = FormatWebSearchResults("{\"results\":[]}", "nothing");
    cJSON_AddStringToObject(out, "format_empty",
                            format_empty ? format_empty->c_str() : "<err>");

    // Mixed-language response: the snippet crosses the 300-byte limit inside
    // the last Chinese character (one ASCII letter + 100 x 3-byte chars).
    std::string mixed =
        std::string("{\"answer\":\"Fresh data \xf0\x9f\x99\x82 here\",") +
        "\"results\":[{\"title\":\"Тест title\",\"url\":\"https://example.com/x\","
        "\"content\":\"a" + Repeat("\xe4\xb8\xad", 100) + "\"}]}";
    auto format_mixed = FormatWebSearchResults(mixed, "mixed");
    cJSON_AddBoolToObject(out, "format_mixed_ok", (bool)format_mixed);
    if (format_mixed) {
        cJSON_AddStringToObject(out, "format_mixed_text", format_mixed->c_str());
        // Serialize as an MCP reply text content block and print it so the
        // test can verify the serialized reply still decodes.
        cJSON* reply = cJSON_CreateObject();
        cJSON_AddStringToObject(reply, "jsonrpc", "2.0");
        cJSON_AddNumberToObject(reply, "id", 7);
        cJSON* result = cJSON_CreateObject();
        cJSON* content = cJSON_CreateArray();
        cJSON* block = cJSON_CreateObject();
        cJSON_AddStringToObject(block, "type", "text");
        cJSON_AddStringToObject(block, "text", format_mixed->c_str());
        cJSON_AddItemToArray(content, block);
        cJSON_AddItemToObject(result, "content", content);
        cJSON_AddItemToObject(reply, "result", result);
        char* serialized = cJSON_PrintUnformatted(reply);
        cJSON_AddStringToObject(out, "format_mixed_reply", serialized);
        free(serialized);
        cJSON_Delete(reply);
    }

    // Result-level truncation must stay on a UTF-8 boundary.
    std::string big = "{\"results\":[";
    for (int i = 0; i < 30; i++) {
        if (i > 0) big += ",";
        big += "{\"title\":\"t" + std::to_string(i) + "\",\"content\":\"" +
               Repeat("\xe4\xb8\xad", 100) + "\"}";
    }
    big += "]}";
    auto format_big = FormatWebSearchResults(big, "big");
    cJSON_AddBoolToObject(out, "format_truncated_ok", (bool)format_big);
    if (format_big) {
        cJSON_AddNumberToObject(out, "format_truncated_len", (double)format_big->size());
        const std::string suffix = "\n... (truncated)";
        cJSON_AddBoolToObject(out, "format_truncated_suffix",
                              format_big->size() > suffix.size() &&
                                  format_big->compare(format_big->size() - suffix.size(),
                                                      suffix.size(), suffix) == 0);
        cJSON_AddStringToObject(out, "format_truncated_text", format_big->c_str());
    }

    char* printed = cJSON_PrintUnformatted(out);
    puts(printed);
    free(printed);
    cJSON_Delete(out);
    return 0;
}
'''

PROBE = r'''
#include <expected>
#include <string>
#include "cJSON.h"
#include "web_search_format.h"
int main() { return 0; }
'''


class WebSearchFormatTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or shutil.which(compiler[0]) is None:
            raise unittest.SkipTest("requires a host C++23 compiler")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            cjson_flags = shlex.split(os.environ.get("MCP_CJSON_CFLAGS", "-I/usr/include/cjson"))
            libs = shlex.split(os.environ.get("MCP_CJSON_LIBS", "-lcjson"))
            (tmp_path / "probe.cc").write_text(textwrap.dedent(PROBE))
            probe = subprocess.run(
                compiler + ["-std=c++23", *cjson_flags, "-I", str(REPO_ROOT / "main"),
                            str(tmp_path / "probe.cc"), *libs, "-o", str(tmp_path / "probe")],
                capture_output=True, text=True)
            if probe.returncode != 0:
                raise unittest.SkipTest(
                    "requires C++23 and cJSON development headers: " + probe.stderr)
            (tmp_path / "driver.cc").write_text(textwrap.dedent(DRIVER))
            build = subprocess.run(
                compiler + ["-std=c++23", *cjson_flags, "-I", str(REPO_ROOT / "main"),
                            str(tmp_path / "driver.cc"), *libs, "-o", str(tmp_path / "driver")],
                capture_output=True, text=True)
            if build.returncode != 0:
                raise AssertionError("driver failed to build: " + build.stderr)
            run = subprocess.run([str(tmp_path / "driver")], capture_output=True, text=True)
            if run.returncode != 0:
                raise AssertionError("driver failed to run: " + run.stderr)
            cls.results = json.loads(run.stdout)

    def test_truncate_utf8(self):
        r = self.results
        self.assertEqual(r["truncate_ascii"], "hello")
        # 1 ASCII + 100 x 3-byte CJK = 301 bytes; a 300-byte cut lands inside
        # the last character and must drop it entirely (1 + 99*3 = 298 bytes).
        self.assertEqual(r["truncate_cjk_len"], 298)
        self.assertEqual(r["truncate_cjk"], "a" + "中" * 99)
        r["truncate_cjk"].encode("utf-8").decode("utf-8")  # must decode strictly
        self.assertEqual(r["truncate_emoji"], "")
        self.assertEqual(r["truncate_zero"], "")

    def test_read_bounded(self):
        r = self.results
        self.assertEqual(r["read_chunks"], "abcdef")
        self.assertTrue(r["read_overflow_is_error"])
        self.assertIn("exceeds", r["read_overflow_msg"])
        self.assertEqual(r["read_empty"], "")
        self.assertEqual(r["read_exact"], "1234567890" * 2)
        self.assertTrue(r["read_error_is_error"])

    def test_format_parse_and_empty(self):
        r = self.results
        self.assertTrue(r["format_parse_is_error"])
        self.assertEqual(r["format_empty"], "No results found for: nothing")

    def test_serialized_reply_decodes(self):
        r = self.results
        self.assertTrue(r["format_mixed_ok"])
        reply = r["format_mixed_reply"]
        # The serialized reply must decode strictly as UTF-8 and as JSON.
        decoded = reply.encode("utf-8").decode("utf-8", "strict")
        parsed = json.loads(decoded)
        text = parsed["result"]["content"][0]["text"]
        self.assertEqual(text, r["format_mixed_text"])
        self.assertNotIn("\ufffd", text)
        self.assertIn("Fresh data 🙂 here", text)
        self.assertIn("Тест title", text)
        # Snippet was cut at 300 bytes: one ASCII letter + 99 CJK chars + "...".
        self.assertIn("a" + "中" * 99 + "...", text)
        self.assertNotIn("中" * 100, text)

    def test_result_truncation_boundary(self):
        r = self.results
        self.assertTrue(r["format_truncated_ok"])
        self.assertTrue(r["format_truncated_suffix"])
        self.assertLessEqual(r["format_truncated_len"], 5000 + len("\n... (truncated)"))
        text = r["format_truncated_text"]
        text.encode("utf-8").decode("utf-8", "strict")
        self.assertNotIn("\ufffd", text)


if __name__ == "__main__":
    unittest.main()
