#!/usr/bin/env python3
"""Hooks the gesture bridge into server.py. Run it in the folder that holds server.py:

    python3 install_gestures.py            install (a backup server.py.bak-gestures is kept)
    python3 install_gestures.py --undo     put the backup back

Running it twice changes nothing.
"""

import os
import shutil
import sys

MARKER = "gesture_bridge"
TARGET = "        asyncio.run(supervise(settings, tools))\n"
BLOCK = (
    "        # Hand gestures from the phone camera (gesture_bridge.py, gestures.json)\n"
    "        from gesture_bridge import start_bridge\n"
    "        start_bridge(tools, base_dir)\n"
)


def patch(text):
    """Returns (new_text, message). Raises ValueError if server.py has an unexpected shape."""
    if MARKER in text:
        return text, "already installed, nothing to change"
    if text.count(TARGET) != 1:
        raise ValueError("could not find the line 'asyncio.run(supervise(settings, tools))' exactly once in server.py")
    if "base_dir = " not in text:
        raise ValueError("server.py has no base_dir variable in main(); is this the right file?")
    return text.replace(TARGET, BLOCK + TARGET), "installed"


def main(argv):
    folder = os.path.dirname(os.path.abspath(__file__))
    server = os.path.join(folder, "server.py")
    backup = server + ".bak-gestures"
    if "--undo" in argv:
        if not os.path.exists(backup):
            print("No backup found, nothing to undo.")
            return 1
        shutil.copy2(backup, server)
        print("Restored server.py from the backup.")
        return 0
    if not os.path.exists(server):
        print("server.py not found next to this script.")
        return 1
    with open(server, encoding="utf-8") as handle:
        text = handle.read()
    try:
        new_text, message = patch(text)
    except ValueError as exc:
        print("Not changed: %s" % exc)
        return 1
    if new_text != text:
        shutil.copy2(server, backup)
        with open(server, "w", encoding="utf-8") as handle:
            handle.write(new_text)
    print("server.py: %s" % message)
    if not os.path.exists(os.path.join(folder, "gestures.json")):
        print("Note: gestures.json is missing, so the bridge will stay off.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
