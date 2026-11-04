# Hand gestures for your XiaoZhi phone server

Your phone's front camera watches your hand. A gesture runs one of your existing tools
(music, and later the bot). Video never leaves the phone; only the gesture name goes to the
Termux server over localhost.

## Install (about 2 minutes)

1. Unzip this into the **same folder as your `server.py`** (it adds 4 files and a `gesture_ui/` folder; nothing is overwritten).
2. In Termux, in that folder:
   ```
   python3 install_gestures.py
   ```
   It adds 3 lines to `server.py` and keeps a backup (`server.py.bak-gestures`). Undo any time with `python3 install_gestures.py --undo`.
3. Restart the server: `sh run.sh`. You should see the line
   `Gesture page: http://127.0.0.1:8770/`
4. On the same phone open **Chrome** and go to `http://127.0.0.1:8770/`, press **Start camera**, allow the camera.
   The first start downloads the hand model (needs internet once).
5. Keep that tab open with the screen on. Show your hand.

## Getting good tracking

- Bright light from in front of you (not a window behind you), a plain background, hand 30 to 60 cm away with every finger visible.
- Prop the phone up so it does not shake.
- Under the camera picture, a grey line shows what it sees, for example
  `Open_Palm 0.93 | fingers 4+thumb | 28 fps, 14 ms (GPU)`.
  Use it to tune things:
  - **Low fps (under about 15) or a jumpy skeleton:** switch the drop-down to **CPU** and press Start again. Some phones are steadier on CPU.
  - **Wrong pose name:** watch the `fingers` count. It is measured from your hand's shape and corrects mix-ups between palm and fist.
  - **"no hand" most of the time:** more light, or move the hand a little farther away.
- After changing the page, close the tab and reopen it so Chrome does not use an old copy.

## Default gestures (edit `gestures.json`)

| Gesture | Action |
|---|---|
| Open palm (hold 0.4 s) | Pause music |
| Peace sign (hold) | Resume music |
| Fist (hold **1.5 s**) | Mute music |
| Finger pointing up (hold) | Volume +10, repeats while held |
| Thumbs up / down | Next / previous song |
| I-love-you sign (thumb, index, pinky) | Bot shows the happy eyes |
| Wave | Wake the bot |

Swipes are switched off (`swipeEnabled: false` in `gesture_ui/gesture_logic.mjs`); they fired by accident and missed real ones. The fist hold time is `holdMsByGesture` in the same file.
Valid names: `Open_Palm, Closed_Fist, Pointing_Up, Thumb_Up, Thumb_Down, Victory, ILoveYou, wave` (and `swipe_left, swipe_right` if you turn swipes back on).
An action is either `{"tool": "<tool name>", "args": {...}}` or `{"bot": {"emotion": "happy"}}` / `{"bot": {"action": "wake"}}`.

## Bot actions

Build the firmware with the **gesture_endpoint** option on and a **gesture_key** (8 to 64 letters, digits, - or _). The bot then listens on port 8080. In `gestures.json` set:

```
"bot_url": "http://<bot ip>:8080/gesture",
"bot_key": "<the same gesture_key>"
```

Find the bot's IP in your phone hotspot's list of connected devices, and if you can, reserve that IP for the bot. Emotions: neutral, happy, sad, angry, surprised, laughing, loving, cool, relaxed, confident, crying, funny. A shown emotion goes back to neutral after 5 s, and only while the bot is idle.

## Safety

- The bridge listens on `127.0.0.1` only, so other devices on your Wi-Fi or hotspot cannot reach it.
- Requests need a secret key (stored in `~/.jarvis/gesture.key`, made automatically), and web pages from other sites are rejected.
- Only tools named in `gestures.json` can run, at most 60 gestures a minute.
- Tip: don't map gestures to `send_email`. A stray hand movement should not send mail.

## Tests

`node gesture_ui/gesture_logic.test.mjs` and `python3 test_gesture_bridge.py`
