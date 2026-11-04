#!/usr/bin/env python3
"""Run:  python3 test_gesture_bridge.py     (standard library only)"""
import http.client
import json
import os
import shutil
import socket
import tempfile
import unittest

import gesture_bridge as gb
import install_gestures as ig

HERE = os.path.dirname(os.path.abspath(__file__))


class FakeTools:
    def __init__(self):
        self.calls = []
        self._handlers = {"local_music_next": 1, "local_music_volume": 1, "boom": 1}

    async def call(self, name, args):
        self.calls.append((name, args))
        if name == "boom":
            raise RuntimeError("secret detail")
        return "did %s %s" % (name, json.dumps(args, sort_keys=True))


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class BridgeHttp(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.ui = os.path.join(self.tmp, "gesture_ui")
        os.makedirs(self.ui)
        with open(os.path.join(self.ui, "index.html"), "w") as f:
            f.write('<script>const KEY="%s";</script>' % gb.PLACEHOLDER)
        with open(os.path.join(self.ui, "gesture_logic.mjs"), "w") as f:
            f.write("export const x = 1;")
        self.tools = FakeTools()
        self.port = free_port()
        self.key = "k" * 30
        self.actions = {
            "swipe_right": {"tool": "local_music_next"},
            "Pointing_Up": {"tool": "local_music_volume", "args": {"change": 10}},
            "Thumb_Up": {"bot": {"emotion": "happy"}},
            "Victory": {"tool": "not_a_tool"},
            "Open_Palm": {"tool": "boom"},
        }
        self.bridge = gb.GestureBridge(self.tools, self.ui, self.key, self.actions, port=self.port)
        self.assertTrue(self.bridge.start())

    def tearDown(self):
        self.bridge.stop()
        shutil.rmtree(self.tmp)

    def req(self, method, path, body=None, headers=None, host=None):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        h = {"Host": host or "127.0.0.1:%d" % self.port}
        h.update(headers or {})
        data = None
        if body is not None:
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
            h.setdefault("Content-Type", "application/json")
        conn.request(method, path, body=data, headers=h)
        resp = conn.getresponse()
        raw = resp.read()
        conn.close()
        return resp.status, raw

    def post(self, event, key=None, **kw):
        headers = {"X-Gesture-Key": self.key if key is None else key}
        headers.update(kw.pop("headers", {}))
        status, raw = self.req("POST", "/gesture", event, headers, **kw)
        return status, json.loads(raw)

    def test_page_has_key_substituted(self):
        status, raw = self.req("GET", "/")
        self.assertEqual(status, 200)
        self.assertIn(self.key, raw.decode())
        self.assertNotIn(gb.PLACEHOLDER, raw.decode())

    def test_logic_module_is_served_as_javascript(self):
        status, raw = self.req("GET", "/gesture_logic.mjs")
        self.assertEqual(status, 200)
        self.assertIn(b"export", raw)

    def test_other_files_are_not_served(self):
        open(os.path.join(self.ui, "secret.txt"), "w").write("x")
        self.assertEqual(self.req("GET", "/secret.txt")[0], 404)
        self.assertEqual(self.req("GET", "/../gesture_bridge.py")[0], 404)

    def test_gesture_runs_mapped_tool(self):
        status, reply = self.post({"gesture": "swipe_right"})
        self.assertEqual(status, 200)
        self.assertTrue(reply["ok"])
        self.assertEqual(self.tools.calls, [("local_music_next", {})])

    def test_args_are_passed(self):
        self.post({"gesture": "Pointing_Up", "repeat": True})
        self.assertEqual(self.tools.calls, [("local_music_volume", {"change": 10})])

    def test_wrong_key_is_rejected_and_nothing_runs(self):
        status, reply = self.post({"gesture": "swipe_right"}, key="nope")
        self.assertEqual(status, 401)
        self.assertEqual(self.tools.calls, [])

    def test_missing_key_is_rejected(self):
        status, raw = self.req("POST", "/gesture", {"gesture": "swipe_right"})
        self.assertEqual(status, 401)
        self.assertEqual(self.tools.calls, [])

    def test_foreign_host_header_is_rejected(self):
        status, _ = self.post({"gesture": "swipe_right"}, host="evil.example:%d" % self.port)
        self.assertEqual(status, 403)
        self.assertEqual(self.tools.calls, [])

    def test_foreign_origin_is_rejected(self):
        status, _ = self.post({"gesture": "swipe_right"}, headers={"Origin": "https://evil.example"})
        self.assertEqual(status, 403)
        self.assertEqual(self.tools.calls, [])

    def test_own_origin_is_accepted(self):
        status, _ = self.post({"gesture": "swipe_right"}, headers={"Origin": "http://127.0.0.1:%d" % self.port})
        self.assertEqual(status, 200)

    def test_unknown_gesture_is_400(self):
        status, reply = self.post({"gesture": "rm -rf"})
        self.assertEqual(status, 400)
        self.assertEqual(self.tools.calls, [])

    def test_unmapped_gesture_does_nothing(self):
        status, reply = self.post({"gesture": "ILoveYou"})
        self.assertEqual(status, 200)
        self.assertFalse(reply["ok"])
        self.assertEqual(self.tools.calls, [])

    def test_tool_not_on_server_is_reported(self):
        status, reply = self.post({"gesture": "Victory"})
        self.assertFalse(reply["ok"])
        self.assertIn("not available", reply["message"])

    def test_crashing_tool_does_not_leak_details_or_kill_bridge(self):
        status, reply = self.post({"gesture": "Open_Palm"})
        self.assertFalse(reply["ok"])
        self.assertNotIn("secret detail", json.dumps(reply))
        self.assertEqual(self.post({"gesture": "swipe_right"})[0], 200)

    def test_bot_action_without_url_says_so(self):
        status, reply = self.post({"gesture": "Thumb_Up"})
        self.assertFalse(reply["ok"])
        self.assertIn("bot endpoint", reply["message"])

    def test_bot_action_posts_to_url(self):
        received = []

        import http.server, threading

        class Sink(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                received.append(json.loads(self.rfile.read(int(self.headers["Content-Length"]))))
                self.send_response(200)
                self.send_header("Content-Length", "0")
                self.end_headers()

            def log_message(self, *a):
                pass

        sink = http.server.HTTPServer(("127.0.0.1", 0), Sink)
        threading.Thread(target=sink.serve_forever, daemon=True).start()
        self.bridge.bot_url = "http://127.0.0.1:%d/gesture" % sink.server_address[1]
        status, reply = self.post({"gesture": "Thumb_Up"})
        sink.shutdown()
        self.assertTrue(reply["ok"], reply)
        self.assertEqual(received, [{"gesture": "Thumb_Up", "repeat": False, "emotion": "happy"}])

    def test_oversized_body_rejected(self):
        status, raw = self.req("POST", "/gesture", b"x" * 5000, {"X-Gesture-Key": self.key})
        self.assertEqual(status, 413)

    def test_bad_json_rejected(self):
        status, raw = self.req("POST", "/gesture", b"{not json", {"X-Gesture-Key": self.key})
        self.assertEqual(status, 400)

    def test_rate_limit(self):
        now = [0.0]
        self.bridge._clock = lambda: now[0]
        self.bridge._limit = 3
        codes = [self.post({"gesture": "swipe_right"})[0] for _ in range(5)]
        self.assertEqual(codes, [200, 200, 200, 429, 429])
        now[0] = 61.0
        self.assertEqual(self.post({"gesture": "swipe_right"})[0], 200)

    def test_binds_loopback_only(self):
        self.assertEqual(self.bridge._server.server_address[0], "127.0.0.1")

    def test_port_in_use_does_not_raise(self):
        other = gb.GestureBridge(self.tools, self.ui, self.key, self.actions, port=self.port)
        self.assertFalse(other.start())


class Config(unittest.TestCase):
    def write(self, data):
        fd, path = tempfile.mkstemp(suffix=".json")
        os.write(fd, json.dumps(data).encode())
        os.close(fd)
        self.addCleanup(os.unlink, path)
        return path

    def test_shipped_gestures_json_is_valid(self):
        settings, actions = gb.load_gesture_config(os.path.join(HERE, "gestures.json"))
        self.assertEqual(settings["port"], 8770)
        self.assertIn("Thumb_Up", actions)

    def test_unknown_gesture_name_is_an_error(self):
        with self.assertRaises(ValueError):
            gb.load_gesture_config(self.write({"actions": {"Jazz_Hands": {"tool": "x"}}}))

    def test_action_needs_exactly_one_kind(self):
        with self.assertRaises(ValueError):
            gb.load_gesture_config(self.write({"actions": {"wave": {"tool": "x", "bot": {}}}}))
        with self.assertRaises(ValueError):
            gb.load_gesture_config(self.write({"actions": {"wave": {}}}))

    def test_key_is_created_once_and_private(self):
        path = os.path.join(tempfile.mkdtemp(), "sub", "gesture.key")
        first = gb.load_key(path)
        self.assertGreaterEqual(len(first), 20)
        self.assertEqual(gb.load_key(path), first)
        self.assertEqual(os.stat(path).st_mode & 0o077, 0)

    def test_start_bridge_without_config_returns_none(self):
        self.assertIsNone(gb.start_bridge(FakeTools(), tempfile.mkdtemp()))


class Installer(unittest.TestCase):
    SAMPLE = (
        "def main():\n    base_dir = '.'\n    try:\n"
        "        asyncio.run(supervise(settings, tools))\n    except KeyboardInterrupt:\n        pass\n"
    )

    def test_installs_before_supervise_and_is_idempotent(self):
        new, msg = ig.patch(self.SAMPLE)
        self.assertEqual(msg, "installed")
        self.assertLess(new.index("start_bridge(tools, base_dir)"), new.index("asyncio.run(supervise"))
        compile(new, "server.py", "exec")
        again, msg2 = ig.patch(new)
        self.assertEqual(again, new)
        self.assertIn("already", msg2)

    def test_refuses_unknown_shape(self):
        with self.assertRaises(ValueError):
            ig.patch("print('hi')\n")

    def test_real_server_py_patches_and_compiles(self):
        path = os.environ.get("REAL_SERVER_PY")
        if not path or not os.path.exists(path):
            self.skipTest("set REAL_SERVER_PY to check against your server.py")
        with open(path, encoding="utf-8") as handle:
            text = handle.read()
        new, msg = ig.patch(text)
        self.assertEqual(msg, "installed")
        compile(new, "server.py", "exec")
        self.assertEqual(new.count("start_bridge("), 1)


if __name__ == "__main__":
    unittest.main(verbosity=1)
