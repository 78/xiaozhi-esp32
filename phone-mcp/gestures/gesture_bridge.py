"""Gesture bridge: lets the phone's camera page trigger your existing MCP tools.

Runs inside server.py (install_gestures.py adds one call). It starts a tiny web server on
127.0.0.1 ONLY, so nothing outside the phone can reach it:

    GET  /               the camera page (gesture_ui/index.html)
    POST /gesture        {"gesture": "swipe_right"}  ->  runs the action mapped in gestures.json

Open  http://127.0.0.1:8770/  in Chrome on the same phone and press Start camera.

Safety: only the loopback address is bound, every request must carry the secret key that is
embedded in the page (kept in ~/.jarvis/gesture.key), the Host/Origin headers must be local
(blocks web pages on other sites), and actions are rate limited. Only tools you list in
gestures.json can run.
"""

import asyncio
import collections
import hmac
import http.server
import json
import logging
import os
import secrets
import threading
import time
import urllib.error
import urllib.request

log = logging.getLogger("xiaozhi-phone-mcp")

DEFAULT_PORT = 8770
MAX_BODY = 2048
PLACEHOLDER = "__GESTURE_KEY__"
KNOWN_GESTURES = {
    "Open_Palm", "Closed_Fist", "Pointing_Up", "Thumb_Up", "Thumb_Down", "Victory", "ILoveYou",
    "swipe_left", "swipe_right", "wave",
}
STATIC = {
    "/gesture_logic.mjs": ("gesture_logic.mjs", "text/javascript; charset=utf-8"),
}


def load_key(path):
    """A random key that survives restarts, so an open page keeps working after run.sh restarts the server."""
    try:
        with open(path, encoding="utf-8") as handle:
            key = handle.read().strip()
        if len(key) >= 20:
            return key
    except OSError:
        pass
    key = secrets.token_urlsafe(24)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as handle:
        handle.write(key + "\n")
    return key


def load_gesture_config(path):
    """gestures.json -> (settings dict, actions dict). Raises ValueError with a readable message."""
    with open(path, encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict) or not isinstance(data.get("actions"), dict):
        raise ValueError('gestures.json needs an "actions" object')
    settings = data.get("settings") or {}
    if not isinstance(settings, dict):
        raise ValueError('"settings" must be an object')
    actions = {}
    for gesture, action in data["actions"].items():
        if gesture.startswith("_"):
            continue
        if gesture not in KNOWN_GESTURES:
            raise ValueError("unknown gesture %r (known: %s)" % (gesture, ", ".join(sorted(KNOWN_GESTURES))))
        if not isinstance(action, dict) or ("tool" in action) == ("bot" in action):
            raise ValueError('gesture %r needs exactly one of "tool" or "bot"' % gesture)
        if "tool" in action and not isinstance(action["tool"], str):
            raise ValueError("gesture %r: tool must be a name" % gesture)
        if "bot" in action and not isinstance(action["bot"], dict):
            raise ValueError("gesture %r: bot must be an object" % gesture)
        args = action.get("args", {})
        if not isinstance(args, dict):
            raise ValueError("gesture %r: args must be an object" % gesture)
        actions[gesture] = action
    return settings, actions


class GestureBridge:
    def __init__(self, tools, ui_dir, key, actions, port=DEFAULT_PORT, bot_url="", max_per_minute=60, bot_key="",
                 clock=time.monotonic):
        self.tools = tools
        self.ui_dir = ui_dir
        self.key = key
        self.actions = actions
        self.port = port
        self.bot_url = (bot_url or "").strip()
        self.bot_key = (bot_key or "").strip()
        self._clock = clock
        self._hits = collections.deque()
        self._limit = max_per_minute
        self._server = None
        self.last_event = None

    # ---- actions ------------------------------------------------------------------
    def _rate_ok(self):
        now = self._clock()
        while self._hits and now - self._hits[0] >= 60:
            self._hits.popleft()
        if len(self._hits) >= self._limit:
            return False
        self._hits.append(now)
        return True

    def handle_gesture(self, event):
        """Runs the mapped action. Returns (http_status, {"ok": bool, "message": str})."""
        gesture = event.get("gesture")
        if not isinstance(gesture, str) or gesture not in KNOWN_GESTURES:
            return 400, {"ok": False, "message": "unknown gesture"}
        action = self.actions.get(gesture)
        if action is None:
            return 200, {"ok": False, "message": "no action set for %s" % gesture}
        if not self._rate_ok():
            return 429, {"ok": False, "message": "too many gestures, slow down"}
        self.last_event = (gesture, time.time())
        if "tool" in action:
            return 200, self._run_tool(action["tool"], action.get("args", {}))
        return 200, self._run_bot(action["bot"], gesture, bool(event.get("repeat")))

    def _run_tool(self, name, args):
        if name not in getattr(self.tools, "_handlers", {}):
            return {"ok": False, "message": "tool %s is not available on this server" % name}
        try:
            text = asyncio.run(self.tools.call(name, dict(args)))
        except Exception as exc:  # a gesture must never take the bridge down
            log.warning("gesture tool %s failed: %s", name, type(exc).__name__)
            return {"ok": False, "message": "that action failed"}
        log.info("gesture -> %s", name)
        return {"ok": True, "message": str(text)}

    def _run_bot(self, payload, gesture, repeat):
        if not self.bot_url:
            return {"ok": False, "message": "bot endpoint not set up yet (settings.bot_url in gestures.json)"}
        body = json.dumps({"gesture": gesture, "repeat": repeat, **payload}).encode("utf-8")
        request = urllib.request.Request(self.bot_url, data=body, method="POST",
                                         headers={"Content-Type": "application/json", "X-Gesture-Key": self.bot_key})
        try:
            with urllib.request.urlopen(request, timeout=2.5) as response:
                response.read(256)
        except (urllib.error.URLError, OSError) as exc:
            log.warning("bot endpoint unreachable: %s", type(exc).__name__)
            return {"ok": False, "message": "could not reach the bot"}
        log.info("gesture -> bot (%s)", gesture)
        return {"ok": True, "message": "sent to the bot"}

    # ---- http -----------------------------------------------------------------------
    def _handler_class(bridge):
        local_hosts = {"127.0.0.1:%d" % bridge.port, "localhost:%d" % bridge.port}
        allowed_origins = {"http://" + host for host in local_hosts}

        class Handler(http.server.BaseHTTPRequestHandler):
            server_version = "gesture-bridge"
            protocol_version = "HTTP/1.1"

            def log_message(self, fmt, *args):  # keep the console quiet
                pass

            def _send(self, status, body, content_type="application/json"):
                data = body if isinstance(body, bytes) else json.dumps(body).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                if status >= 400:  # the request body may be unread, so do not reuse this connection
                    self.send_header("Connection", "close")
                    self.close_connection = True
                self.end_headers()
                self.wfile.write(data)

            def _local_only(self):
                if self.headers.get("Host", "") not in local_hosts:
                    return False
                origin = self.headers.get("Origin")
                return origin is None or origin in allowed_origins

            def do_GET(self):
                if not self._local_only():
                    return self._send(403, {"ok": False, "message": "local access only"})
                path = self.path.split("?", 1)[0]
                if path in ("/", "/index.html"):
                    try:
                        with open(os.path.join(bridge.ui_dir, "index.html"), encoding="utf-8") as handle:
                            page = handle.read().replace(PLACEHOLDER, bridge.key)
                    except OSError:
                        return self._send(500, {"ok": False, "message": "gesture_ui/index.html is missing"})
                    return self._send(200, page.encode("utf-8"), "text/html; charset=utf-8")
                if path in STATIC:
                    filename, ctype = STATIC[path]
                    try:
                        with open(os.path.join(bridge.ui_dir, filename), "rb") as handle:
                            return self._send(200, handle.read(), ctype)
                    except OSError:
                        pass
                return self._send(404, {"ok": False, "message": "not found"})

            def do_POST(self):
                if not self._local_only():
                    return self._send(403, {"ok": False, "message": "local access only"})
                if self.path.split("?", 1)[0] != "/gesture":
                    return self._send(404, {"ok": False, "message": "not found"})
                supplied = self.headers.get("X-Gesture-Key", "")
                if not hmac.compare_digest(supplied.encode("utf-8"), bridge.key.encode("utf-8")):
                    return self._send(401, {"ok": False, "message": "wrong key, reload the page"})
                try:
                    length = int(self.headers.get("Content-Length", "0"))
                except ValueError:
                    length = -1
                if length < 0 or length > MAX_BODY:
                    return self._send(413, {"ok": False, "message": "bad request size"})
                try:
                    event = json.loads(self.rfile.read(length) or b"{}")
                except ValueError:
                    return self._send(400, {"ok": False, "message": "bad JSON"})
                if not isinstance(event, dict):
                    return self._send(400, {"ok": False, "message": "bad JSON"})
                status, reply = bridge.handle_gesture(event)
                return self._send(status, reply)

        return Handler

    def start(self):
        """Starts serving in a background thread. Returns True if it is listening."""
        try:
            self._server = http.server.ThreadingHTTPServer(("127.0.0.1", self.port), self._handler_class())
        except OSError as exc:
            log.warning("Gesture bridge could not start on port %d: %s", self.port, exc)
            return False
        self._server.daemon_threads = True
        threading.Thread(target=self._server.serve_forever, name="gesture-bridge", daemon=True).start()
        log.info("Gesture page: http://127.0.0.1:%d/  (open it in Chrome on this phone)", self.port)
        return True

    def stop(self):
        if self._server:
            self._server.shutdown()
            self._server.server_close()
            self._server = None


def start_bridge(tools, base_dir):
    """Called from server.py main(). Never raises: gestures are optional."""
    try:
        settings, actions = load_gesture_config(os.path.join(base_dir, "gestures.json"))
        port = int(settings.get("port", DEFAULT_PORT))
        key = load_key(os.path.join(os.path.expanduser("~"), ".jarvis", "gesture.key"))
        bridge = GestureBridge(tools, os.path.join(base_dir, "gesture_ui"), key, actions, port=port,
                               bot_url=str(settings.get("bot_url", "")), bot_key=str(settings.get("bot_key", "")))
        return bridge if bridge.start() else None
    except FileNotFoundError:
        log.info("No gestures.json, gesture bridge is off.")
    except (ValueError, OSError) as exc:
        log.warning("Gesture bridge is off: %s", exc)
    return None
