"""Tests for local_music.py. Run: python3 -m unittest phone-mcp/test_local_music.py -v

Uses a temporary music folder and a fake "mpv" script, so no audio is played.
"""

import json
import os
import socket
import stat
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

import local_music
from local_music import LocalMusic, TOOLS

SONGS = [
    "Tum Hi Ho - Arijit Singh.flac",
    "Tum Hi Ho - Arijit Singh.mp4",
    "Kesariya - Arijit Singh.flac",
    "Apna Time Aayega - Ranveer Singh.flac",
    "Blinding Lights - The Weeknd.mp4",
    "Shape of You - Ed Sheeran.flac",
    "Channa Mereya - Arijit Singh.flac",
    "notes.txt",
]


class Fixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.music = os.path.join(self.tmp.name, "SpotMusic")
        os.makedirs(self.music)
        for name in SONGS:
            open(os.path.join(self.music, name), "wb").close()
        self.log = os.path.join(self.tmp.name, "player.log")
        self.fake = os.path.join(self.tmp.name, "fakempv")
        with open(self.fake, "w") as f:
            f.write("#!/bin/sh\necho \"$@\" >> %s\nexec sleep 30\n" % self.log)
        os.chmod(self.fake, os.stat(self.fake).st_mode | stat.S_IEXEC)
        self.m = LocalMusic(music_dir=self.music, player=self.fake,
                            ipc_path=os.path.join(self.tmp.name, "mpv.sock"))

    def tearDown(self):
        self.m.stop()
        self.tmp.cleanup()


class SearchTests(Fixture):
    def test_library_ignores_non_audio_and_duplicates(self):
        self.assertEqual(self.m.scan(), 6)  # 7 audio files, Tum Hi Ho counted once

    def test_prefers_flac_over_mp4(self):
        best = self.m.search("tum hi ho", 1)[0]
        self.assertTrue(best["path"].endswith(".flac"))

    def test_exact_and_filler_words(self):
        self.assertEqual(self.m.search("play kesariya", 1)[0]["title"], "Kesariya")

    def test_misspelling_from_speech_to_text(self):
        self.assertEqual(self.m.search("kesaria", 1)[0]["title"], "Kesariya")
        self.assertEqual(self.m.search("blinding light", 1)[0]["title"], "Blinding Lights")

    def test_artist_query(self):
        titles = {e["title"] for e in self.m.search("arijit singh", 10)}
        self.assertTrue({"Tum Hi Ho", "Kesariya", "Channa Mereya"} <= titles)

    def test_unknown_song_returns_nothing(self):
        self.assertEqual(self.m.search("quantum chemistry lecture", 5), [])


class PlayTests(Fixture):
    def test_play_starts_player_with_local_file_only(self):
        reply = self.m.play("kesariya")
        self.assertIn("Playing Kesariya by Arijit Singh", reply)
        args = open(self.log).read()
        self.assertIn(os.path.join(self.music, "Kesariya - Arijit Singh.flac"), args)
        self.assertNotIn("http", args)

    def test_unknown_song_says_offline_and_does_not_start_player(self):
        reply = self.m.play("some random chinese pop")
        self.assertIn("Nothing was searched online", reply)
        self.assertFalse(os.path.exists(self.log))

    def test_empty_query_shuffles(self):
        self.assertIn("Shuffling", self.m.play(""))

    def test_shuffle_one_artist(self):
        self.assertIn("Shuffling 3 songs", self.m.shuffle("arijit singh"))

    def test_stop(self):
        self.m.play("kesariya")
        self.assertEqual(self.m.stop(), "Stopped.")
        self.assertEqual(self.m.stop(), "Nothing is playing.")

    def test_missing_player(self):
        m = LocalMusic(music_dir=self.music, player="/no/such/player")
        self.assertIn("pkg install mpv", m.play("kesariya"))

    def test_missing_folder(self):
        m = LocalMusic(music_dir="/no/such/folder")
        m.music_dir = None
        self.assertIn("termux-setup-storage", m.play("kesariya"))

    def test_list_and_find(self):
        self.assertIn("6 songs", self.m.list_songs())
        self.assertIn("Kesariya", self.m.find("kesariya"))


class IpcTests(Fixture):
    def test_controls_send_mpv_commands(self):
        received = []
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(self.m.ipc_path)
        server.listen(5)
        server.settimeout(5)

        def serve():
            try:
                while True:
                    conn, _ = server.accept()
                    data = conn.recv(4096).decode().strip()
                    cmd = json.loads(data)["command"]
                    received.append(cmd)
                    reply = {"error": "success", "data": os.path.join(
                        self.music, "Kesariya - Arijit Singh.flac") if cmd[0] == "get_property" and cmd[1] == "path" else None}
                    conn.sendall((json.dumps(reply) + "\n").encode())
                    conn.close()
            except OSError:
                pass

        threading.Thread(target=serve, daemon=True).start()
        self.m.play("kesariya")
        self.assertEqual(self.m.pause(), "Paused.")
        self.assertEqual(self.m.resume(), "Resumed.")
        self.m.next()
        self.m.previous()
        self.assertEqual(self.m.set_volume(level=40), "Music volume is 40.")
        self.assertIn("Kesariya", self.m.now_playing())
        server.close()
        self.assertIn(["set_property", "pause", True], received)
        self.assertIn(["set_property", "pause", False], received)
        self.assertIn(["playlist-next"], received)
        self.assertIn(["playlist-prev"], received)
        self.assertIn(["set_property", "volume", 40], received)

    def test_controls_when_nothing_plays(self):
        self.assertEqual(self.m.pause(), "Nothing is playing.")
        self.assertEqual(self.m.next(), "Nothing is playing.")


class BotSpeakerTests(Fixture):
    def setUp(self):
        super().setUp()
        self.ffmpeg = os.path.join(self.tmp.name, "fakeffmpeg")
        with open(self.ffmpeg, "w") as f:
            f.write("#!/bin/sh\necho \"$@\" > %s.args\nprintf 'ABCDEFGHIJKLMNOP'\n" % self.ffmpeg)
        os.chmod(self.ffmpeg, os.stat(self.ffmpeg).st_mode | stat.S_IEXEC)
        self.m.ffmpeg = self.ffmpeg
        self.m.stream_port = 0  # any free port
        os.environ["JARVIS_PHONE_IP"] = "127.0.0.1"

    def tearDown(self):
        os.environ.pop("JARVIS_PHONE_IP", None)
        if self.m._server:
            self.m._server.shutdown()
            self.m._server.server_close()
        super().tearDown()

    def _url(self, reply):
        self.assertIn("self.music.play_url", reply)
        url = reply.split("url=")[1].strip()
        port = self.m._server.server_address[1]
        return url.replace(":0/", ":%d/" % port)

    def test_bot_target_gives_stream_url_and_does_not_play_on_phone(self):
        reply = self.m.play("kesariya", "bot")
        self.assertIn("Kesariya by Arijit Singh", reply)
        self.assertFalse(os.path.exists(self.log))  # the phone player was not started

    def test_stream_serves_raw_audio_at_requested_rate(self):
        url = self._url(self.m.play("kesariya", "bot"))
        with urllib.request.urlopen(url + "?rate=16000", timeout=5) as r:
            self.assertEqual(r.read(), b"ABCDEFGHIJKLMNOP")
        args = open(self.ffmpeg + ".args").read()
        self.assertIn("Kesariya - Arijit Singh.flac", args)
        self.assertIn("-ar 16000", args)
        self.assertIn("s16le", args)

    def test_default_host_is_the_wifi_gateway_for_hotspot(self):
        os.environ.pop("JARVIS_PHONE_IP", None)
        reply = self.m.play("kesariya", "bot")
        self.assertIn("url=http://gateway:", reply)

    def test_stream_refuses_unknown_ids_and_paths(self):
        url = self._url(self.m.play("kesariya", "bot"))
        base = url.rsplit("/", 2)[0]
        for bad in ("/stream/deadbeef.pcm", "/stream/..%2f..%2fetc%2fpasswd.pcm", "/other"):
            with self.assertRaises(urllib.error.HTTPError):
                urllib.request.urlopen(base + bad, timeout=5)

    def test_missing_ffmpeg_is_reported(self):
        self.m.ffmpeg = "/no/such/ffmpeg"
        self.assertIn("pkg install ffmpeg", self.m.play("kesariya", "bot"))

    def test_target_words(self):
        for word in ("bot", "here", "Local", "speaker"):
            self.assertIn("self.music.play_url", self.m.call("local_music_play", {"query": "kesariya", "target": word}))
        for word in ("phone", "mobile", "PH", "", "anything"):
            self.assertIn("on the phone", self.m.call("local_music_play", {"query": "kesariya", "target": word}))


class ToolSpecTests(Fixture):
    def test_every_tool_is_callable_and_offline(self):
        names = [t[0] for t in TOOLS]
        self.assertEqual(len(names), len(set(names)))
        for name, description, props, required in TOOLS:
            self.assertTrue(set(required) <= set(props))
            if name in ("local_music_play", "local_music_shuffle", "local_music_find", "local_music_list"):
                self.assertIn("never searches the internet", description)
        self.assertIn("Unknown music tool", self.m.call("nope"))
        self.assertIn("Kesariya", self.m.call("local_music_find", {"query": "kesariya"}))


if __name__ == "__main__":
    unittest.main()
