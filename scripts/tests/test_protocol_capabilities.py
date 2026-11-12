"""Exercise the real hello/capability builders and OTA request assembly on the host."""

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


class ProtocolCapabilitiesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or not shutil.which(compiler[0]):
            raise unittest.SkipTest("requires a host C++23 compiler")
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        build = Path(cls.directory.name)
        flags = shlex.split(os.environ.get("MCP_CJSON_CFLAGS", "-I/usr/include/cjson"))
        libs = shlex.split(os.environ.get("MCP_CJSON_LIBS", "-lcjson"))
        probe = subprocess.run(
            compiler + ["-std=c++23", *flags, "-x", "c++", "-", *libs, "-o", str(build / "probe")],
            input="#include <expected>\n#include <cJSON.h>\nint main() { cJSON_Delete(cJSON_CreateObject()); }\n",
            text=True, capture_output=True,
        )
        if probe.returncode:
            raise unittest.SkipTest("requires C++23 and cJSON: " + probe.stderr)
        protocol = (ROOT / "main/protocols/protocol.cc").read_text()
        capabilities = protocol[protocol.index("void Protocol::AddClientCapabilities"):
                                protocol.index("\nvoid Protocol::OnIncomingJson")]
        hello_methods = []
        for name, owner in [("mqtt_protocol.cc", "MqttProtocol"),
                            ("websocket_protocol.cc", "WebsocketProtocol")]:
            source = (ROOT / "main/protocols" / name).read_text()
            start = source.index("std::string " + owner + "::GetHelloMessage()")
            hello_methods.append(source[start:source.index("\nvoid " + owner + "::ParseServerHello", start)])
        ota = (ROOT / "main/ota.cc").read_text()
        start = ota.index("    std::string data = board.GetSystemInfoJson();")
        assembly = ota[start:ota.index("    std::string method =", start)]
        source = textwrap.dedent(r'''
            #include <expected>
            #include <iostream>
            #include <iterator>
            #include <string>
            #include <cJSON.h>
            #include "cjson_utils.h"
            #define OPUS_FRAME_DURATION_MS 60
            struct FontCapability {
                bool glyph_push = false;
                std::string bundle = "test-font", charset = "common";
                int size = 20, bpp = 4;
            };
            struct Assets {
                FontCapability capability;
                static Assets& GetInstance() { static Assets assets; return assets; }
                const FontCapability& text_font_capability() const { return capability; }
            };
            struct Protocol {
                static void AddClientCapabilities(cJSON*, bool include_text_font = true);
            };
            struct MqttProtocol : Protocol { std::string GetHelloMessage(); };
            struct WebsocketProtocol : Protocol {
                int version_ = 1;
                std::string GetHelloMessage();
            };
            struct NetworkError { static NetworkError ProtocolError() { return {}; } };
            struct Board {
                std::string body;
                std::string GetSystemInfoJson() { return body; }
            };
        ''') + capabilities + "\n" + "\n".join(hello_methods) + r'''
            std::expected<std::string, NetworkError> OtaBody(const std::string& input) {
                Board board{input};
        ''' + assembly + r'''
                return data;
            }
            int main(int argc, char** argv) {
                Assets::GetInstance().capability.glyph_push = argc > 1 && std::string(argv[1]) == "glyph";
                MqttProtocol mqtt;
                WebsocketProtocol ws;
                std::cout << mqtt.GetHelloMessage() << '\n' << ws.GetHelloMessage() << '\n';
                std::string input{std::istreambuf_iterator<char>(std::cin), {}};
                auto body = OtaBody(input);
                if (!body) return 2;
                std::cout << *body << '\n';
            }
        '''
        (build / "capabilities.cc").write_text(source)
        cls.binaries = {}
        for aec in (False, True):
            binary = build / ("capabilities-aec" if aec else "capabilities")
            extra = ["-DCONFIG_USE_SERVER_AEC=1"] if aec else []
            result = subprocess.run(
                compiler + ["-std=c++23", "-fno-exceptions", f"-I{ROOT / 'main'}", *flags, *extra,
                            str(build / "capabilities.cc"),
                            *libs, "-o", str(binary)], capture_output=True, text=True,
            )
            if result.returncode:
                raise RuntimeError("Capability host compile failed:\n" + result.stderr)
            cls.binaries[aec] = binary

    def messages(self, body, aec=False, glyph=False):
        result = subprocess.run(
            [str(self.binaries[aec]), "glyph" if glyph else ""],
            input=body, text=True, capture_output=True, check=True,
        )

        def unique(pairs):
            self.assertEqual(len(pairs), len(dict(pairs)), "Duplicate JSON keys")
            return dict(pairs)

        # The OTA body can retain whitespace, so decode JSON values rather than lines.
        decoder = json.JSONDecoder(object_pairs_hook=unique)
        messages = []
        remaining = result.stdout
        while remaining.strip():
            value, end = decoder.raw_decode(remaining.lstrip())
            messages.append(value)
            remaining = remaining.lstrip()[end:]
        self.assertEqual(len(messages), 3)
        return messages

    def test_capabilities_agree_across_ota_and_transports(self):
        for aec in (False, True):
            for glyph in (False, True):
                with self.subTest(aec=aec, glyph=glyph):
                    mqtt, ws, ota = self.messages(
                        '{"version":2,"application":{"version":"2.5.4"},"uuid":"test-id"}',
                        aec, glyph,
                    )
                    features = mqtt["features"]
                    self.assertEqual(features, ws["features"])
                    self.assertEqual(features, ota["protocol"]["features"])
                    self.assertEqual(set(features),
                                     {"mcp", "mcp_20260728", "udp_probe", "aec", "glyph_push"})
                    self.assertTrue(features["mcp"])
                    self.assertTrue(features["mcp_20260728"])
                    self.assertIs(features["udp_probe"], True)
                    self.assertEqual((features["aec"], features["glyph_push"]), (aec, glyph))
                    self.assertEqual((mqtt["transport"], mqtt["version"]), ("udp", 3))
                    self.assertEqual((ws["transport"], ws["version"]), ("websocket", 1))
                    self.assertEqual(ota["uuid"], "test-id")
                    self.assertEqual(ota["application"], {"version": "2.5.4"})
                    self.assertNotIn("text_font", ota["protocol"])
                    if glyph:
                        self.assertEqual(mqtt["text_font"], ws["text_font"])
                        self.assertEqual(mqtt["text_font"]["bundle"], "test-font")
                    else:
                        self.assertNotIn("text_font", mqtt)
                        self.assertNotIn("text_font", ws)

    def test_empty_and_whitespace_root_objects_remain_valid(self):
        for body in ("{}", "{  }", "  \n{}\n", '{ "version": 2 }\n'):
            with self.subTest(body=body):
                _, _, ota = self.messages(body)
                self.assertIn("protocol", ota)


if __name__ == "__main__":
    unittest.main()
