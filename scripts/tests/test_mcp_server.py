"""Host tests for the real MCP dispatcher (C++23, libcjson-dev, libmbedtls-dev)."""

import json
import os
import shlex
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class McpServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or shutil.which(compiler[0]) is None:
            raise unittest.SkipTest("requires a host C++23 compiler")
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        build = Path(cls.directory.name)
        # Only platform dependencies are replaced. Compile mcp_server.cc and the
        # press-to-talk tool unchanged, using the real cJSON parser/serializer.
        stubs = {
            "platform.h": r'''
                #pragma once
                #define _APPLICATION_H_
                #define SETTINGS_H
                #define BOARD_NAME "host-test"
                #include <cstdlib>
                #include <functional>
                #include <iostream>
                #include <string>
                #include <utility>
                #include <vector>
                class Application {
                public:
                    std::vector<std::function<void()>> pending;
                    static Application& GetInstance() {
                        static Application app;
                        return app;
                    }
                    void Schedule(std::function<void()> fn) {
                        pending.push_back(std::move(fn));
                    }
                    void Drain() {
                        while (!pending.empty()) {
                            auto tasks = std::move(pending);
                            pending.clear();
                            for (auto& task : tasks) task();
                        }
                    }
                    void SendMcpMessage(const std::string& payload) {
                        std::cout << "default\t" << payload << '\n';
                    }
                    void Reboot() { std::abort(); }
                    bool UpgradeFirmware(const std::string&) { std::abort(); }
                };
                class Settings {
                public:
                    Settings(const std::string&, bool = false) {}
                    int GetInt(const std::string&, int fallback) { return fallback; }
                    void SetInt(const std::string&, int) {}
                    void SetString(const std::string&, const std::string&) {}
                };
                inline void vTaskDelay(int) { std::abort(); }
                #define pdMS_TO_TICKS(value) (value)
            ''',
            "esp_system.h": r'''
                #pragma once
                #include <cstdlib>
                [[noreturn]] inline void esp_system_abort(const char*) { std::abort(); }
            ''',
            "esp_log.h": "\n".join(
                f"#define ESP_LOG{level}(...) ((void)0)" for level in "EWID"
            ),
            "esp_pthread.h": "",
            "esp_app_desc.h": r'''
                struct esp_app_desc_t { const char* version; };
                inline const esp_app_desc_t* esp_app_get_description() {
                    static esp_app_desc_t desc{"test"};
                    return &desc;
                }
            ''',
            "board.h": r'''
                #pragma once
                #include <cstdint>
                #include <cstdlib>
                #include <string>
                struct AudioCodec { void SetOutputVolume(int) { std::abort(); } };
                struct Backlight { void SetBrightness(uint8_t, bool) { std::abort(); } };
                struct Camera {
                    void SetExplainUrl(const std::string&, const std::string&) {
                        std::abort();
                    }
                };
                class Board {
                public:
                    static Board& GetInstance() { static Board board; return board; }
                    std::string GetDeviceStatusJson() { std::abort(); }
                    std::string GetSystemInfoJson() { std::abort(); }
                    AudioCodec* GetAudioCodec() { std::abort(); }
                    Backlight* GetBacklight() { return nullptr; }
                    Camera* GetCamera() { return nullptr; }
                };
            ''',
            "display.h": "",
            "lvgl_image.h": "",
            "lvgl_theme.h": "",
        }
        for name, contents in stubs.items():
            (build / name).write_text(textwrap.dedent(contents), encoding="utf-8")
        driver = build / "driver.cc"
        driver.write_text(textwrap.dedent(r'''
            #include "mcp_server.h"
            #include "press_to_talk_mcp_tool.h"
            int main(int argc, char**) {
                auto& server = McpServer::GetInstance();
                PressToTalkMcpTool press_to_talk;
                press_to_talk.Initialize();
                server.AddTool("test.error", "Return a tool error",
                    PropertyList({Property("message", kPropertyTypeString)}),
                    [](const PropertyList& args) -> ToolResult {
                        return std::unexpected(args["message"].value<std::string>());
                    });
                McpServer::ResponseSender sender;
                if (argc > 1) {
                    sender = [](const std::string& payload) {
                        std::cout << "custom\t" << payload << '\n';
                    };
                }
                std::string request;
                while (std::getline(std::cin, request)) {
                    server.ParseMessage(request, sender);
                    Application::GetInstance().Drain();
                }
            }
        '''), encoding="utf-8")
        # Allow non-system installations without fetching dependencies in tests.
        cjson_flags = shlex.split(os.environ.get("MCP_CJSON_CFLAGS", "-I/usr/include/cjson"))
        libs = shlex.split(os.environ.get("MCP_CJSON_LIBS", "-lcjson"))
        probe = subprocess.run(
            compiler + ["-std=c++23", *cjson_flags, "-x", "c++", "-", *libs,
                        "-o", str(build / "probe")],
            input="#include <expected>\n#include <cJSON.h>\n#include <mbedtls/base64.h>\n"
                  "int main() { std::expected<int, int> v; cJSON_Delete(cJSON_CreateObject()); }",
            text=True, capture_output=True,
        )
        if probe.returncode:
            raise unittest.SkipTest("requires C++23, cJSON and mbedTLS development headers: "
                                    + probe.stderr)
        cls.executable = build / "mcp_test"
        subprocess.run(
            compiler + ["-std=c++23", "-fno-exceptions", *cjson_flags,
                        f"-I{build}", f"-I{ROOT / 'main'}",
                        f"-I{ROOT / 'main/boards/common'}",
                        "-include", str(build / "platform.h"), str(driver),
                        str(ROOT / "main/mcp_server.cc"),
                        str(ROOT / "main/boards/common/press_to_talk_mcp_tool.cc"),
                        *libs, "-o", str(cls.executable)],
            check=True, capture_output=True, text=True,
        )

    def responses(self, requests, custom_sender=True):
        process = subprocess.run(
            [str(self.executable)] + (["custom-sender"] if custom_sender else []),
            input="".join(json.dumps(request) + "\n" for request in requests),
            text=True, encoding="utf-8", capture_output=True, check=True,
        )
        responses = []
        try:
            for line in process.stdout.splitlines():
                route, _, payload = line.partition("\t")
                self.assertEqual(route, "custom" if custom_sender else "default")
                responses.append(json.loads(payload))
        except json.JSONDecodeError as error:
            self.fail(f"MCP response is not valid JSON: {process.stdout!r}: {error}")
        self.assertEqual(len(responses), len(requests))
        return responses

    @staticmethod
    def call(request_id, name, arguments):
        return {"jsonrpc": "2.0", "id": request_id, "method": "tools/call",
                "params": {"name": name, "arguments": arguments}}

    def assert_error(self, response, request_id, code, message):
        self.assertEqual(response, {"jsonrpc": "2.0", "id": request_id,
                                    "error": {"code": code, "message": message}})

    def test_tool_error_message_round_trips(self):
        for message in ['plain error', 'Invalid mode: "press"', r'path C:\new\file',
                        'first\nsecond\r\n\tline', '\b\f\x01\x1f', '错误：模式无效', '']:
            for custom_sender in [True, False]:
                with self.subTest(message=message, custom_sender=custom_sender):
                    request = self.call(42, "test.error", {"message": message})
                    response, = self.responses([request], custom_sender)
                    self.assert_error(response, 42, -32603, message)

    def test_real_press_to_talk_error_and_subsequent_success(self):
        mode = '"press_to_talk"'
        requests = [self.call(1, "self.set_press_to_talk", {"mode": mode}),
                    self.call(2, "self.set_press_to_talk", {"mode": "press_to_talk"}),
                    self.call(3, "self.set_press_to_talk", {"mode": "click_to_talk"})]
        error, *successes = self.responses(requests)
        self.assert_error(error, 1, -32603, "Invalid mode: " + mode)
        for request_id, response in zip([2, 3], successes):
            self.assertEqual(response, {"jsonrpc": "2.0", "id": request_id,
                "result": {"content": [{"type": "text", "text": "true"}], "isError": False}})

    def test_unknown_method_and_tool_errors(self):
        name = 'unknown"\\\n方法'
        requests = [{"jsonrpc": "2.0", "id": 0, "method": name},
                    self.call(-1, name, {})]
        method, tool = self.responses(requests)
        self.assert_error(method, 0, -32601, "Method not implemented: " + name)
        self.assert_error(tool, -1, -32602, "Unknown tool: " + name)

    def test_missing_argument_keeps_error_code(self):
        response, = self.responses([self.call(7, "self.set_press_to_talk", {})])
        self.assert_error(response, 7, -32602, "Missing required argument: mode")


if __name__ == "__main__":
    unittest.main()
