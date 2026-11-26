"""Tests for install_music.py and jarvis_music_tools.py.  Run: python3 -m unittest test_install_music -v"""

import asyncio
import os
import subprocess
import sys
import tempfile
import unittest

import install_music
import jarvis_music_tools
from local_music import TOOLS

HERE = os.path.dirname(os.path.abspath(__file__))

SERVER = '''class Tools:
    def __init__(self, settings, contacts):
        self.specs = []
        self._handlers = {}
        if settings:
            self._add("send_email", "d", {}, [], None)

        from music_tools import register as _reg_music
        _reg_music(self)

        from music_tools import register as _reg_music
        _reg_music(self)

    def _add(self, name, description, properties, required, handler):
        self.specs.append(name)
        self._handlers[name] = handler

    async def call(self, name, arguments):
        return await self._handlers[name](arguments)
'''


def run_installer(*args, cwd):
    return subprocess.run([sys.executable, os.path.join(HERE, "install_music.py"), *args],
                          cwd=cwd, capture_output=True, text=True)


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.server = os.path.join(self.tmp.name, "server.py")
        with open(self.server, "w") as f:
            f.write(SERVER)

    def tearDown(self):
        self.tmp.cleanup()

    def read(self):
        with open(self.server) as f:
            return f.read()

    def test_replaces_both_old_calls_with_one_new_call(self):
        result = run_installer(cwd=self.tmp.name)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        text = self.read()
        self.assertNotIn("music_tools import", text.replace("jarvis_music_tools", ""))
        self.assertNotIn("_reg_music(self)", text)
        self.assertEqual(text.count("_reg_jarvis_music(self)"), 1)
        self.assertLess(text.index("_reg_jarvis_music(self)"), text.index("def _add("))
        compile(text, "server.py", "exec")
        self.assertTrue(os.path.exists(self.server + ".bak-music"))

    def test_running_twice_changes_nothing(self):
        run_installer(cwd=self.tmp.name)
        once = self.read()
        result = run_installer(cwd=self.tmp.name)
        self.assertIn("already installed", result.stdout)
        self.assertEqual(self.read(), once)

    def test_undo_restores_the_backup(self):
        run_installer(cwd=self.tmp.name)
        run_installer("--undo", cwd=self.tmp.name)
        self.assertEqual(self.read(), SERVER)

    def test_unexpected_file_is_left_alone(self):
        with open(self.server, "w") as f:
            f.write("print('hello')\n")
        result = run_installer(cwd=self.tmp.name)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Nothing was changed", result.stdout)
        self.assertEqual(self.read(), "print('hello')\n")

    def test_missing_server(self):
        os.remove(self.server)
        self.assertEqual(run_installer(cwd=self.tmp.name).returncode, 2)


class RegisterTests(unittest.TestCase):
    def test_registers_every_tool_once_and_calls_through(self):
        ns = {}
        exec(SERVER.replace("from music_tools import register as _reg_music", "pass")
                   .replace("_reg_music(self)", "pass"), ns)
        tools = ns["Tools"](None, {})
        with tempfile.TemporaryDirectory() as music_dir:
            open(os.path.join(music_dir, "Kesariya - Arijit Singh.flac"), "wb").close()
            os.environ["JARVIS_MUSIC_DIR"] = music_dir
            try:
                jarvis_music_tools.register(tools)
                names = [t[0] for t in TOOLS]
                self.assertEqual(sorted(n for n in tools.specs if n.startswith("local_music_")), sorted(names))
                self.assertEqual(len(tools.specs), len(set(tools.specs)))
                reply = asyncio.run(tools.call("local_music_find", {"query": "kesariya"}))
                self.assertIn("Kesariya", reply)
                bad = asyncio.run(tools.call("local_music_volume", {"level": "loud"}))
                self.assertIn("did not understand", bad)
            finally:
                os.environ.pop("JARVIS_MUSIC_DIR", None)


if __name__ == "__main__":
    unittest.main()
