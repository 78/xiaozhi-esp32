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
                #include "camera.h"
                struct HostCamera : Camera {
                    std::string legacy_url, legacy_token;
                    int captures = 0;
                    void SetExplainUrl(const std::string& url, const std::string& token) override {
                        legacy_url = url; legacy_token = token;
                    }
                    bool Capture() override { ++captures; return true; }
                    bool SetHMirror(bool) override { return true; }
                    bool SetVFlip(bool) override { return true; }
                    std::expected<std::string, std::string> Explain(const std::string& question) override {
                        return Explain(question, legacy_url, legacy_token);
                    }
                    std::expected<std::string, std::string> Explain(
                        const std::string& question, const std::string& url,
                        const std::string& token) override {
                        if (url == "https://fail.example/") {
                            return std::unexpected("Upload failed");
                        }
                        return url + "|" + token + "|" + question;
                    }
                };
                class Board {
                public:
                    static Board& GetInstance() { static Board board; return board; }
                    std::string GetDeviceStatusJson() { std::abort(); }
                    std::string GetSystemInfoJson() { std::abort(); }
                    AudioCodec* GetAudioCodec() { std::abort(); }
                    Backlight* GetBacklight() { return nullptr; }
                    HostCamera* GetCamera() {
                        static HostCamera camera;
                        return &camera;
                    }
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
            #include "board.h"
            int main(int argc, char** argv) {
                auto& server = McpServer::GetInstance();
                const std::string mode = argc > 2 ? argv[2] : "";
                PressToTalkMcpTool press_to_talk;
                if (mode != "empty" && mode != "user-only") {
                    press_to_talk.Initialize();
                    server.AddTool("test.error", "Return a tool error",
                        PropertyList({Property("message", kPropertyTypeString)}),
                        [](const PropertyList& args) -> ToolResult {
                            return std::unexpected(args["message"].value<std::string>());
                        });
                    server.AddTool("test.integer", "Return integer",
                        PropertyList({Property("value", kPropertyTypeInteger)}),
                        [](const PropertyList& args) -> ToolResult {
                            return args["value"].value<int>();
                        });
                    server.AddTool("test.image", "Return image", PropertyList(),
                        [](const PropertyList&) -> ToolResult {
                            return new ImageContent("image/jpeg", "abc");
                        });
                    server.AddTool("test.captures", "Capture count", PropertyList(),
                        [](const PropertyList&) -> ToolResult {
                            return Board::GetInstance().GetCamera()->captures;
                        });
                    server.AddTool("self.camera.take_photo", "Take photo",
                        PropertyList({Property("question", kPropertyTypeString)}),
                        [](const PropertyList& args, const McpRequestContext& context) -> ToolResult {
                            auto camera = Board::GetInstance().GetCamera();
                            camera->Capture();
                            auto question = args["question"].value<std::string>();
                            auto value = context.vision
                                ? camera->Explain(question, context.vision->url, context.vision->token)
                                : camera->Explain(question);
                            if (!value) return std::unexpected(value.error());
                            return *value;
                        });
                }
                if (mode != "empty") {
                    server.AddUserOnlyTool("test.user", "User-only", PropertyList(),
                        [](const PropertyList&) -> ToolResult { return true; });
                }
                if (mode == "pages") {
                    for (int i = 0; i < 40; ++i) {
                        server.AddTool("page." + std::to_string(i), std::string(700, 'x'), PropertyList(),
                            [](const PropertyList&) -> ToolResult { return true; });
                    }
                }
                if (mode == "oversize") {
                    server.AddTool("oversize", std::string(9000, 'x'), PropertyList(),
                        [](const PropertyList&) -> ToolResult { return true; });
                }
                McpServer::ResponseSender sender;
                if (argc > 1 && std::string(argv[1]) == "custom-sender") {
                    sender = [](const std::string& payload) {
                        std::cout << "custom\t" << payload << '\n';
                    };
                }
                std::string request;
                while (std::getline(std::cin, request)) {
                    server.ParseMessage(request, sender);
                    if (mode != "deferred") Application::GetInstance().Drain();
                }
                Application::GetInstance().Drain();
            }
        '''), encoding="utf-8")
        # Allow non-system installations without fetching dependencies in tests.
        cjson_flags = shlex.split(os.environ.get("MCP_CJSON_CFLAGS", "-I/usr/include/cjson"))
        libs = shlex.split(os.environ.get("MCP_CJSON_LIBS", "-lcjson -lmbedcrypto"))
        probe = subprocess.run(
            compiler + ["-std=c++23", *cjson_flags, "-x", "c++", "-", *libs,
                        "-o", str(build / "probe")],
            input="#include <expected>\n#include <cJSON.h>\n#include <mbedtls/base64.h>\n"
                  "int main() { std::expected<int, int> v; cJSON_Delete(cJSON_CreateObject()); "
                  "size_t n; return mbedtls_base64_encode(nullptr, 0, &n, nullptr, 0); }",
            text=True, capture_output=True,
        )
        if probe.returncode:
            raise unittest.SkipTest("requires C++23, cJSON and mbedTLS development headers: "
                                    + probe.stderr)
        cls.executable = build / "mcp_test"
        compiled = subprocess.run(
            compiler + ["-std=c++23", "-fno-exceptions", *cjson_flags,
                        f"-I{build}", f"-I{ROOT / 'main'}",
                        f"-I{ROOT / 'main/boards/common'}",
                        "-include", str(build / "platform.h"), str(driver),
                        str(ROOT / "main/mcp_server.cc"),
                        str(ROOT / "main/boards/common/press_to_talk_mcp_tool.cc"),
                        *libs, "-o", str(cls.executable)],
            capture_output=True, text=True,
        )
        if compiled.returncode:
            raise RuntimeError("MCP host compile failed:\\n" + compiled.stderr)

    def responses(self, requests, custom_sender=True, mode="", expected_count=None):
        process = subprocess.run(
            [str(self.executable), "custom-sender" if custom_sender else "default", mode],
            input="".join((request if isinstance(request, str) else json.dumps(request)) + "\n"
                          for request in requests),
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
        self.assertEqual(len(responses), len(requests) if expected_count is None else expected_count)
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

    @staticmethod
    def modern(request_id, method="tools/list", params=None, capabilities=None):
        return {"jsonrpc": "2.0", "id": request_id, "method": method, "params": {
            **(params or {}),
            "_meta": {
                "io.modelcontextprotocol/protocolVersion": "2026-07-28",
                "io.modelcontextprotocol/clientInfo": {"name": "host", "version": "1"},
                "io.modelcontextprotocol/clientCapabilities": capabilities or {},
            },
        }}

    @classmethod
    def photo(cls, request_id, vision=None):
        caps = {} if vision is None else {"extensions": {"me.xiaozhi/vision": vision}}
        return cls.modern(request_id, "tools/call",
                          {"name": "self.camera.take_photo", "arguments": {"question": "look"}},
                          caps)

    @staticmethod
    def legacy_init(url="https://legacy.example/", token="legacy-secret"):
        return {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "capabilities": {"vision": {"url": url, "token": token}}}}

    def test_discovery_before_conversation_hello(self):
        request = self.modern(10000, "server/discover")
        response, = self.responses([request])
        result = response["result"]
        self.assertEqual(response["id"], 10000)
        self.assertEqual(result["supportedVersions"], ["2026-07-28", "2024-11-05"])
        self.assertEqual(result["capabilities"],
                         {"tools": {}, "extensions": {"me.xiaozhi/vision": {}}})
        self.assertEqual(result["_meta"]["io.modelcontextprotocol/serverInfo"],
                         {"name": "host-test", "version": "test"})
        self.assertEqual(result["resultType"], "complete")
        self.assertEqual((result["ttlMs"], result["cacheScope"]), (60000, "private"))

    def test_requests_select_their_own_era(self):
        requests = [self.legacy_init(), self.modern("modern-list"),
                    {"jsonrpc": "2.0", "id": 3, "method": "tools/list"},
                    self.modern(4, "tools/call",
                                {"name": "test.integer", "arguments": {"value": 7}})]
        initialize, modern_list, legacy_list, call = self.responses(requests)
        self.assertEqual(initialize["result"]["protocolVersion"], "2024-11-05")
        self.assertNotIn("resultType", initialize["result"])
        self.assertEqual(modern_list["result"]["tools"], legacy_list["result"]["tools"])
        self.assertEqual(modern_list["result"]["resultType"], "complete")
        self.assertNotIn("resultType", legacy_list["result"])
        self.assertNotIn("ttlMs", legacy_list["result"])
        self.assertEqual(call["result"]["content"][0]["text"], "7")

    def test_string_and_safe_integer_ids_round_trip(self):
        ids = ['quote"\\\n标识', "", -9007199254740991, 9007199254740991]
        replies = self.responses([self.modern(i, "tools/call",
                                 {"name": "test.integer", "arguments": {"value": 3}}) for i in ids])
        self.assertEqual([r["id"] for r in replies], ids)
        for reply, original in zip(replies, ids):
            self.assertIs(type(reply["id"]), type(original))

    def test_invalid_ids_do_not_execute_tools(self):
        requests = [self.call(i, "self.camera.take_photo", {"question": "look"})
                    for i in [None, True, {}, [], 1.25, 9007199254740992]]
        replies = self.responses(requests + [self.call(9, "test.captures", {})])
        for response in replies[:-1]:
            self.assertEqual(response["error"]["code"], -32600)
            self.assertNotIn("id", response)
        self.assertEqual(replies[-1]["result"]["content"][0]["text"], "0")

    def test_notifications_do_not_execute_or_reply(self):
        notification = {"jsonrpc": "2.0", "method": "tools/call", "params": {
            "name": "self.camera.take_photo", "arguments": {"question": "look"}}}
        response, = self.responses([{"jsonrpc": "2.0", "method": "notifications/initialized"},
                                   notification, self.call(8, "test.captures", {})], expected_count=1)
        self.assertEqual(response["result"]["content"][0]["text"], "0")

    def test_parse_errors_reject_trailing_data(self):
        valid = json.dumps(self.call(1, "self.camera.take_photo", {"question": "look"}))
        replies = self.responses(["{", valid + " garbage", [], {"jsonrpc": "1.0", "id": 5,
                                  "method": "tools/list"}, self.call(9, "test.captures", {})])
        self.assertEqual([r["error"]["code"] for r in replies[:-1]],
                         [-32700, -32700, -32600, -32600])
        self.assertEqual(replies[-1]["result"]["content"][0]["text"], "0")

    def test_modern_metadata_cannot_fall_back_to_legacy(self):
        key = "io.modelcontextprotocol/"
        cases = [
            {"jsonrpc": "2.0", "id": 1, "method": "server/discover"},
            {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {"_meta": []}},
        ]
        for bad_meta in [{key + "protocolVersion": "2026-07-28"},
                         {key + "clientCapabilities": {}},
                         {key + "protocolVersion": 20260728, key + "clientCapabilities": {}},
                         {key + "protocolVersion": "2026-07-28", key + "clientCapabilities": []},
                         {key + "protocolVersion": "2026-07-28",
                          key + "clientCapabilities": {"extensions": []}},
                         {key + "protocolVersion": "2026-07-28", key + "clientCapabilities": {},
                          key + "clientInfo": {"name": 1, "version": "1"}}]:
            cases.append({"jsonrpc": "2.0", "id": 3, "method": "tools/list",
                          "params": {"_meta": bad_meta}})
        for response in self.responses(cases):
            self.assertEqual(response["error"]["code"], -32602)

    def test_unsupported_version_and_modern_initialize(self):
        request = self.modern(11)
        version = 'unknown"\\\nversion'
        request["params"]["_meta"]["io.modelcontextprotocol/protocolVersion"] = version
        mismatch, initialize = self.responses([request, self.modern(12, "initialize")])
        self.assertEqual(mismatch["error"]["code"], -32022)
        self.assertEqual(mismatch["error"]["data"],
                         {"supported": ["2026-07-28", "2024-11-05"], "requested": version})
        self.assertEqual(initialize["error"]["code"], -32601)

    def test_vision_missing_capability_does_not_use_initialize(self):
        _, response, count = self.responses([
            self.legacy_init(), self.photo("no-vision"), self.call(3, "test.captures", {})])
        self.assertEqual(response["error"]["code"], -32021)
        self.assertEqual(response["error"]["data"], {
            "requiredCapabilities": {"extensions": {"me.xiaozhi/vision": {}}}})
        self.assertEqual(count["result"]["content"][0]["text"], "0")

    def test_invalid_vision_has_no_capture_side_effect(self):
        visions = [True, [], {}, {"url": 5}, {"url": ""},
                   {"url": "ftp://a"}, {"url": "wss://a"}, {"url": "https://"},
                   {"url": "http:///a"}, {"url": "http://?a"}, {"url": "http://a b"},
                   {"url": "http://a\nb"}, {"url": "https://a", "token": 7},
                   {"url": "https://a", "token": "secret\r\nHeader: bad"}]
        replies = self.responses([self.photo(i+10, v) for i, v in enumerate(visions)] +
                                 [self.call(99, "test.captures", {})])
        for response in replies[:-1]:
            self.assertEqual(response["error"]["code"], -32602)
            self.assertEqual(response["error"]["message"], "Invalid vision configuration")
        self.assertEqual(replies[-1]["result"]["content"][0]["text"], "0")

    def test_modern_and_legacy_vision_are_isolated(self):
        requests = [self.legacy_init(),
                    self.photo("a", {"url": "https://a.example/", "token": "a-secret"}),
                    self.photo("b", {"url": "https://b.example/"}),
                    self.photo("fail", {"url": "https://fail.example/", "token": "failed-secret"}),
                    self.call(6, "self.camera.take_photo", {"question": "legacy"}),
                    self.photo("after-fail", {"url": "http://local.example/", "token": ""})]
        _, a, b, failure, legacy, after = self.responses(requests)
        self.assertEqual(a["result"]["content"][0]["text"], "https://a.example/|a-secret|look")
        self.assertEqual(b["result"]["content"][0]["text"], "https://b.example/||look")
        self.assertTrue(failure["result"]["isError"])
        self.assertEqual(legacy["result"]["content"][0]["text"],
                         "https://legacy.example/|legacy-secret|legacy")
        self.assertEqual(after["result"]["content"][0]["text"], "http://local.example/||look")
        for response in (a, b, failure, legacy, after):
            self.assertNotIn("vision", response.get("result", {}).get("_meta", {}))

    def test_deferred_calls_own_metadata_and_response_ids(self):
        requests = [self.photo('first"\\id', {"url": "https://first.example/", "token": "first"}),
                    self.photo("second", {"url": "https://second.example/", "token": "second"}),
                    self.legacy_init("https://last-legacy.example/", "legacy-last"),
                    self.call("legacy", "self.camera.take_photo", {"question": "last"})]
        for sender in (True, False):
            replies = {r["id"]: r for r in self.responses(requests, sender, mode="deferred")}
            self.assertEqual(replies['first"\\id']["result"]["content"][0]["text"],
                             "https://first.example/|first|look")
            self.assertEqual(replies["second"]["result"]["content"][0]["text"],
                             "https://second.example/|second|look")
            self.assertEqual(replies["legacy"]["result"]["content"][0]["text"],
                             "https://last-legacy.example/|legacy-last|last")

    def test_tool_execution_failure_keeps_legacy_error_and_modern_result(self):
        error = 'error"\\\nmessage'
        legacy, modern = self.responses([self.call(1, "test.error", {"message": error}),
            self.modern(2, "tools/call", {"name": "test.error", "arguments": {"message": error}})])
        self.assert_error(legacy, 1, -32603, error)
        self.assertEqual(modern["result"]["resultType"], "complete")
        self.assertEqual(modern["result"]["content"], [{"type": "text", "text": error}])
        self.assertTrue(modern["result"]["isError"])

    def test_modern_integer_rejects_fractions(self):
        bad, good = self.responses([
            self.modern(1, "tools/call", {"name": "test.integer", "arguments": {"value": 1.5}}),
            self.modern(2, "tools/call", {"name": "test.integer", "arguments": {"value": 4}})])
        self.assertEqual(bad["error"]["code"], -32602)
        self.assertEqual(good["result"]["content"][0]["text"], "4")

    def test_modern_image_uses_standard_content_shape(self):
        legacy, modern = self.responses([self.call(1, "test.image", {}),
                                         self.modern(2, "tools/call",
                                                     {"name": "test.image", "arguments": {}})])
        self.assertIsInstance(legacy["result"]["content"][0]["image"], str)
        self.assertEqual(modern["result"]["content"],
                         [{"type": "image", "mimeType": "image/jpeg", "data": "YWJj"}])

    def test_empty_and_user_only_catalogs(self):
        response, discovery = self.responses(
            [self.modern(1), self.modern(2, "server/discover")], mode="empty")
        self.assertEqual(response["result"]["tools"], [])
        self.assertEqual(discovery["result"]["capabilities"], {"tools": {}})
        regular, users = self.responses(
            [self.modern(1), self.modern(2, params={"withUserTools": True})], mode="user-only")
        self.assertEqual(regular["result"]["tools"], [])
        self.assertEqual([t["name"] for t in users["result"]["tools"]], ["test.user"])

    def test_pagination_payload_budget_and_filtering(self):
        tools = []
        cursor = None
        for page in range(20):
            params = {"withUserTools": False}
            if cursor:
                params["cursor"] = cursor
            result, = self.responses([self.modern(page, params=params)], mode="pages")
            self.assertLessEqual(len(json.dumps(result, separators=(",", ":")).encode()), 8000)
            self.assertEqual((result["result"]["ttlMs"], result["result"]["cacheScope"]),
                             (60000, "private"))
            tools.extend(result["result"]["tools"])
            cursor = result["result"].get("nextCursor")
            if not cursor:
                break
        else:
            self.fail("Pagination failed to complete")
        names = [tool["name"] for tool in tools]
        self.assertEqual(len(names), len(set(names)))
        self.assertNotIn("test.user", names)
        self.assertEqual([n for n in names if n.startswith("page.")],
                         ["page." + str(i) for i in range(40)])
        users, = self.responses([self.modern(1, params={"withUserTools": True})])
        self.assertIn("test.user", [t["name"] for t in users["result"]["tools"]])

    def test_invalid_list_params_and_oversize_tool(self):
        for params in [{"cursor": 1}, {"withUserTools": 1}, {"cursor": "unknown"}]:
            response, = self.responses([self.modern(1, params=params)])
            self.assertEqual(response["error"]["code"], -32602)
        response, = self.responses([self.modern(1, params={"cursor": "oversize"})], mode="oversize")
        self.assertEqual(response["error"]["code"], -32603)


if __name__ == "__main__":
    unittest.main()
