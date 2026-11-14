"""Offline music for Jarvis, running on the phone (Termux).

Plays ONLY files from the music folder on the phone (default ~/storage/shared/SpotMusic,
files named "Song - Artist.flac" or similar). It never searches the web. If a song is not
in the library it says so and stops, so the cloud assistant has nothing to fall back on.

Uses only the Python standard library plus the `mpv` player (pkg install mpv).

How a server uses it:
    from local_music import LocalMusic, TOOLS
    music = LocalMusic()
    for name, description, parameters in TOOLS:   # register each one with your MCP server
        ...
    result_text = music.call(name, {"query": "tum hi ho"})
"""

import difflib
import hashlib
import http.server
import json
import os
import random
import re
import shutil
import socket
import subprocess
import tempfile
import threading
import time
import unicodedata
from pathlib import Path

AUDIO_EXTS = (".flac", ".mp3", ".m4a", ".opus", ".ogg", ".wav", ".aac", ".mp4")  # best first
FILLER_WORDS = {"play", "song", "songs", "music", "gaana", "gana"}
MIN_SCORE = 0.55
BOT_WORDS = {"bot", "here", "local", "speaker", "device", "jarvis"}

OFFLINE_NOTE = (
    "Offline only: this plays music saved on the phone and never searches the internet. "
    "Use it for EVERY request to play music, a song, an artist or a mood. Never use web search "
    "or any online music source."
)


def _normalize(text):
    text = unicodedata.normalize("NFKD", text.casefold())
    text = "".join(c for c in text if not unicodedata.combining(c))
    return " ".join(re.findall(r"\w+", text))


class LocalMusic:
    def __init__(self, music_dir=None, player=None, ipc_path=None, rescan_seconds=60):
        self.music_dir = self._find_dir(music_dir)
        self.player = player or os.environ.get("JARVIS_PLAYER", "mpv")
        self.extra_args = os.environ.get("JARVIS_MPV_ARGS", "").split()
        self.ipc_path = ipc_path or os.path.join(tempfile.gettempdir(), "jarvis-mpv.sock")
        self.rescan_seconds = rescan_seconds
        self.volume = 70
        self.stream_port = int(os.environ.get("JARVIS_STREAM_PORT", "8765"))
        self.ffmpeg = os.environ.get("JARVIS_FFMPEG", "ffmpeg")
        self._server = None
        self._ids = {}
        self._proc = None
        self._library = []
        self._scanned_at = 0.0

    # ---- library ----------------------------------------------------------------------
    @staticmethod
    def _find_dir(music_dir):
        candidates = [
            music_dir,
            os.environ.get("JARVIS_MUSIC_DIR"),
            "~/storage/shared/SpotMusic",
            "/storage/emulated/0/SpotMusic",
        ]
        for c in candidates:
            if c and os.path.isdir(os.path.expanduser(c)):
                return os.path.expanduser(c)
        return None

    def scan(self):
        found = {}
        if self.music_dir:
            for root, _dirs, files in os.walk(self.music_dir):
                for name in files:
                    ext = os.path.splitext(name)[1].lower()
                    if ext not in AUDIO_EXTS:
                        continue
                    stem = os.path.splitext(name)[0]
                    title, _, artist = stem.partition(" - ")
                    entry = {
                        "path": os.path.join(root, name),
                        "title": title.strip(),
                        "artist": artist.strip(),
                        "k_title": _normalize(title),
                        "k_artist": _normalize(artist),
                        "k_all": _normalize(stem),
                        "rank": AUDIO_EXTS.index(ext),
                    }
                    entry["tokens"] = set(entry["k_all"].split())
                    old = found.get(entry["k_all"])
                    if old is None or entry["rank"] < old["rank"]:  # same song in two formats
                        found[entry["k_all"]] = entry
        self._library = sorted(found.values(), key=lambda e: e["k_all"])
        self._scanned_at = time.monotonic()
        return len(self._library)

    def library(self):
        if not self._library or time.monotonic() - self._scanned_at > self.rescan_seconds:
            self.scan()
        return self._library

    # ---- search -----------------------------------------------------------------------
    @staticmethod
    def _tokens_of_query(query):
        tokens = _normalize(query).split()
        kept = [t for t in tokens if t not in FILLER_WORDS]
        return kept or tokens

    @staticmethod
    def _token_hit(token, entry_tokens):
        if token in entry_tokens:
            return True
        if len(token) < 4:
            return False
        return any(difflib.SequenceMatcher(None, token, t).ratio() >= 0.8 for t in entry_tokens)

    def search(self, query, limit=5):
        tokens = self._tokens_of_query(query)
        if not tokens:
            return []
        q = " ".join(tokens)
        results = []
        for e in self.library():
            coverage = sum(self._token_hit(t, e["tokens"]) for t in tokens) / len(tokens)
            ratio = max(
                difflib.SequenceMatcher(None, q, e["k_title"]).ratio(),
                difflib.SequenceMatcher(None, q, e["k_all"]).ratio(),
                0.9 * difflib.SequenceMatcher(None, q, e["k_artist"]).ratio() if e["k_artist"] else 0,
            )
            score = 0.5 * coverage + 0.5 * ratio
            if q in e["k_all"]:
                score = max(score, 0.9)
            if score >= MIN_SCORE:
                results.append((score, e))
        results.sort(key=lambda r: (-r[0], r[1]["k_all"]))
        return [e for _s, e in results[:limit]]

    @staticmethod
    def _label(e):
        return "%s by %s" % (e["title"], e["artist"]) if e["artist"] else e["title"]

    # ---- player -----------------------------------------------------------------------
    def _alive(self):
        return self._proc is not None and self._proc.poll() is None

    def _start(self, entries):
        self.stop()
        cmd = [self.player, "--no-video", "--no-terminal", "--really-quiet",
               "--input-ipc-server=%s" % self.ipc_path, "--volume=%d" % self.volume]
        cmd += self.extra_args + [e["path"] for e in entries]
        try:
            self._proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                          stderr=subprocess.DEVNULL, start_new_session=True)
        except FileNotFoundError:
            self._proc = None
            return "The player is not installed. Run: pkg install mpv"
        time.sleep(0.4)
        if self._proc.poll() is not None:
            self._proc = None
            return "The player stopped right away, so the song could not be played."
        return None

    def _ipc(self, *command):
        if not self._alive():
            return None
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.settimeout(1.5)
                s.connect(self.ipc_path)
                s.sendall((json.dumps({"command": list(command)}) + "\n").encode())
                buf = b""
                while b"\n" not in buf:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                return json.loads(buf.split(b"\n")[0].decode() or "{}")
        except (OSError, ValueError):
            return None

    # ---- streaming to the bot's own speaker --------------------------------------------
    # The bot cannot read the phone's files, so the phone serves the song as raw 16-bit mono
    # audio over HTTP and the bot pulls it (firmware tool: self.music.play_url).
    def _song_id(self, entry):
        sid = hashlib.sha1(entry["path"].encode("utf-8")).hexdigest()[:16]
        self._ids[sid] = entry["path"]
        return sid

    def _ensure_server(self):
        if self._server is not None:
            return None
        owner = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                try:
                    owner._serve_stream(self)
                except (BrokenPipeError, ConnectionResetError):
                    pass

        try:
            self._server = http.server.ThreadingHTTPServer(("0.0.0.0", self.stream_port), Handler)
        except OSError as exc:
            return "Could not open the music stream port %d: %s" % (self.stream_port, exc)
        threading.Thread(target=self._server.serve_forever, daemon=True).start()
        return None

    def _serve_stream(self, handler):
        from urllib.parse import parse_qs, urlparse

        parsed = urlparse(handler.path)
        if not parsed.path.startswith("/stream/"):
            handler.send_error(404)
            return
        path = self._ids.get(parsed.path.rsplit("/", 1)[-1].split(".")[0])
        if not path or not os.path.isfile(path):
            handler.send_error(404)
            return
        try:
            rate = int(parse_qs(parsed.query).get("rate", ["24000"])[0])
        except ValueError:
            rate = 24000
        rate = max(8000, min(48000, rate))
        cmd = [self.ffmpeg, "-v", "quiet", "-i", path, "-vn", "-f", "s16le", "-ac", "1",
               "-ar", str(rate), "-"]
        try:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                    stdin=subprocess.DEVNULL)
        except FileNotFoundError:
            handler.send_error(500, "ffmpeg is not installed")
            return
        handler.send_response(200)
        handler.send_header("Content-Type", "application/octet-stream")
        handler.end_headers()
        try:
            while True:
                chunk = proc.stdout.read(8192)
                if not chunk:
                    break
                handler.wfile.write(chunk)
        finally:
            proc.kill()
            proc.wait()
            proc.stdout.close()

    def phone_address(self):
        """Address the bot should use. "gateway" means the bot's Wi-Fi gateway, which is the phone
        when the bot is on the phone's hotspot. Set JARVIS_PHONE_IP when the bot is on another Wi-Fi."""
        return os.environ.get("JARVIS_PHONE_IP") or "gateway"

    def prepare_for_bot(self, entry):
        failed = self._ensure_server()
        if failed:
            return failed
        if shutil.which(self.ffmpeg) is None:
            return "ffmpeg is not installed on the phone. Run: pkg install ffmpeg"
        host = self.phone_address()
        url = "http://%s:%d/stream/%s.pcm" % (host, self.stream_port, self._song_id(entry))
        return ("Ready to play %s on the bot's speaker. Now call the device tool "
                "self.music.play_url with url=%s" % (self._label(entry), url))

    # ---- actions ----------------------------------------------------------------------
    def _no_library(self):
        if not self.music_dir:
            return ("The music folder was not found on the phone. Run termux-setup-storage, or "
                    "set JARVIS_MUSIC_DIR to the folder.")
        if not self.library():
            return "The music folder is empty."
        return None

    def play(self, query, target="phone"):
        problem = self._no_library()
        if problem:
            return problem
        if not query or not query.strip():
            return self.shuffle("")
        hits = self.search(query, limit=1)
        if not hits:
            return ("No song matching '%s' is saved on the phone. Nothing was searched online. "
                    "Ask the user for another song." % query)
        if target == "bot":
            self.stop()  # do not play on the phone as well
            return self.prepare_for_bot(hits[0])
        failed = self._start(hits)
        return failed or "Playing %s on the phone." % self._label(hits[0])

    def shuffle(self, artist=""):
        problem = self._no_library()
        if problem:
            return problem
        pool = self.search(artist, limit=500) if artist and artist.strip() else list(self.library())
        if not pool:
            return "No songs by '%s' are saved on the phone. Nothing was searched online." % artist
        random.shuffle(pool)
        failed = self._start(pool[:30])
        return failed or "Shuffling %d songs from the phone, starting with %s." % (
            min(len(pool), 30), self._label(pool[0]))

    def list_songs(self, limit=60, artist=""):
        problem = self._no_library()
        if problem:
            return problem
        songs = self.search(artist, limit=500) if artist and artist.strip() else self.library()
        if not songs:
            return "No matching songs on the phone."
        names = [self._label(e) for e in songs[: max(1, int(limit))]]
        return "%d songs on the phone (showing %d): %s" % (len(songs), len(names), "; ".join(names))

    def find(self, query):
        problem = self._no_library()
        if problem:
            return problem
        hits = self.search(query, limit=5)
        if not hits:
            return "No song matching '%s' is saved on the phone." % query
        return "Matches on the phone: " + "; ".join(self._label(e) for e in hits)

    def pause(self):
        return "Paused." if self._ipc("set_property", "pause", True) is not None else "Nothing is playing."

    def resume(self):
        return "Resumed." if self._ipc("set_property", "pause", False) is not None else "Nothing is playing."

    def stop(self):
        if self._alive():
            self._proc.terminate()
            try:
                self._proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._proc.kill()
            self._proc = None
            return "Stopped."
        self._proc = None
        return "Nothing is playing."

    def next(self):
        if self._ipc("playlist-next") is None:
            return "Nothing is playing."
        return "Skipped to the next song."

    def previous(self):
        if self._ipc("playlist-prev") is None:
            return "Nothing is playing."
        return "Went back to the previous song."

    def set_volume(self, level=None, change=None):
        if level is not None:
            self.volume = max(0, min(130, int(level)))
        elif change is not None:
            self.volume = max(0, min(130, self.volume + int(change)))
        self._ipc("set_property", "volume", self.volume)
        return "Music volume is %d." % self.volume

    def now_playing(self):
        reply = self._ipc("get_property", "path")
        if not reply or "data" not in reply:
            return "Nothing is playing."
        path = reply["data"]
        for e in self.library():
            if e["path"] == path:
                return "Now playing %s." % self._label(e)
        return "Now playing %s." % os.path.basename(path)

    # ---- tool layer -------------------------------------------------------------------
    def call(self, name, args=None):
        args = args or {}
        table = {
            "local_music_play": lambda: self.play(args.get("query", ""), _target(args)),
            "local_music_shuffle": lambda: self.shuffle(args.get("artist", "")),
            "local_music_find": lambda: self.find(args.get("query", "")),
            "local_music_list": lambda: self.list_songs(args.get("limit", 60), args.get("artist", "")),
            "local_music_pause": self.pause,
            "local_music_resume": self.resume,
            "local_music_stop": self.stop,
            "local_music_next": self.next,
            "local_music_previous": self.previous,
            "local_music_volume": lambda: self.set_volume(args.get("level"), args.get("change")),
            "local_music_now_playing": self.now_playing,
        }
        if name not in table:
            return "Unknown music tool: %s" % name
        return table[name]()


def _target(args):
    value = str(args.get("target", "phone")).strip().lower()
    return "bot" if value in BOT_WORDS else "phone"


def _str(description):
    return {"type": "string", "description": description}


def _int(description):
    return {"type": "integer", "description": description}


# (tool name, description, JSON-schema properties, required)
TOOLS = [
    ("local_music_play",
     "Play a song or artist from the music saved on the user's phone. " + OFFLINE_NOTE +
     " Pass only the song and/or artist name as the user said it, without words like 'play'. "
     "If the user names no song, call local_music_shuffle instead. "
     "Where to play: target 'phone' (default; also when the user says mobile, phone or PH) plays on "
     "the phone's speaker. Target 'bot' (when the user says bot, here, local, speaker or device) "
     "plays on the bot's own speaker: the result then names a device tool, self.music.play_url, "
     "and you MUST call it with the url from the result to start the music.",
     {"query": _str("Song title and/or artist, e.g. 'tum hi ho' or 'arijit singh'"),
      "target": _str("'phone' (default) or 'bot'")}, ["query"]),
    ("local_music_shuffle",
     "Play random songs from the phone, optionally only one artist. " + OFFLINE_NOTE +
     " For a mood request, call local_music_list first, pick fitting songs, then use local_music_play.",
     {"artist": _str("Optional artist name to limit the shuffle")}, []),
    ("local_music_find",
     "Check which songs saved on the phone match a name. " + OFFLINE_NOTE,
     {"query": _str("Song or artist to look for")}, ["query"]),
    ("local_music_list",
     "List song names saved on the phone, to choose by mood or artist. " + OFFLINE_NOTE,
     {"limit": _int("Maximum number of songs to list"), "artist": _str("Optional artist filter")}, []),
    ("local_music_pause", "Pause the music on the phone.", {}, []),
    ("local_music_resume", "Resume the paused music on the phone.", {}, []),
    ("local_music_stop", "Stop the music on the phone.", {}, []),
    ("local_music_next", "Skip to the next song on the phone.", {}, []),
    ("local_music_previous", "Go back to the previous song on the phone.", {}, []),
    ("local_music_volume",
     "Set or change the music volume on the phone (0 to 100). Use 'level' for an exact value or "
     "'change' for louder (+) and quieter (-).",
     {"level": _int("Exact volume 0-100"), "change": _int("Relative change, e.g. 10 or -10")}, []),
    ("local_music_now_playing", "Say which song is playing on the phone.", {}, []),
]
