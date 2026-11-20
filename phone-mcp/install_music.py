#!/usr/bin/env python3
"""Hooks the offline music tools into server.py. Run it in the folder that holds server.py:

    python3 install_music.py

It removes the old `music_tools` register calls (there can be two of them, which registers every
tool twice) and adds one call to jarvis_music_tools. A backup of server.py is kept.
Running it again changes nothing. Use --undo to put the backup back.
"""

import glob
import os
import re
import shutil
import sys
import time

MARKER = "jarvis_music_tools"
BLOCK = (
    "        # Jarvis offline music: plays only songs saved on the phone (jarvis_music_tools.py)\n"
    "        from jarvis_music_tools import register as _reg_jarvis_music\n"
    "        _reg_jarvis_music(self)\n"
)
OLD_LINES = re.compile(
    r"^[ \t]*(?:from[ \t]+music_tools[ \t]+import[ \t]+.*|_reg_music\(self\)[ \t]*)\r?\n", re.M
)
ADD_METHOD = re.compile(r"^    def _add\(self,", re.M)


def patch(text):
    """Returns (new_text, message). Raises ValueError if server.py has an unexpected shape."""
    removed = len(OLD_LINES.findall(text))
    cleaned = OLD_LINES.sub("", text)
    if MARKER in cleaned:
        if removed:
            return cleaned, "removed %d old music_tools line(s); the new music tools were already added" % removed
        return text, "already installed, nothing to change"
    found = ADD_METHOD.search(cleaned)
    if not found:
        raise ValueError("could not find 'def _add(self, ...' inside class Tools in server.py")
    new_text = cleaned[: found.start()].rstrip("\n") + "\n\n" + BLOCK + "\n" + cleaned[found.start():]
    return new_text, "removed %d old music_tools line(s) and added the offline music tools" % removed


def main(argv):
    path = "server.py"
    undo = "--undo" in argv
    skip_check = "--no-check" in argv  # for tests only
    for arg in argv:
        if arg.endswith(".py") and not arg.endswith("install_music.py"):
            path = arg
    if not os.path.isfile(path):
        print("server.py was not found here. Run this from the folder that contains it (cd ~/xiaozhi-phone-mcp).")
        return 2

    if undo:
        backups = sorted(glob.glob(path + ".bak-music*"))
        if not backups:
            print("No backup found.")
            return 1
        shutil.copy2(backups[0], path)
        print("Restored %s from %s" % (path, backups[0]))
        return 0

    with open(path, encoding="utf-8", newline="") as handle:
        text = handle.read()
    try:
        new_text, message = patch(text)
    except ValueError as exc:
        print("Nothing was changed: %s" % exc)
        return 1
    if new_text == text:
        print(message)
        return 0
    if not skip_check:
        try:
            compile(new_text, path, "exec")
        except SyntaxError as exc:
            print("Nothing was changed: the result would not run (%s)" % exc)
            return 1
    backup = path + ".bak-music"
    if os.path.exists(backup):
        backup += time.strftime("-%Y%m%d-%H%M%S")
    shutil.copy2(path, backup)
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(new_text)
    print("Done: %s." % message)
    print("Backup: %s" % backup)
    print("Next: python3 server.py --check, then start the server as usual.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
