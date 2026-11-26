# Jarvis offline music (phone side)

`local_music.py` plays songs saved on the phone through `mpv`. It never searches the internet:
if a song is not in the library it says so and stops.

## Install in Termux

```
pkg install mpv ffmpeg
termux-setup-storage
cd ~/xiaozhi-phone-mcp
B=https://raw.githubusercontent.com/mhatrehitaanshu09-hash/xiaozhi-esp32/oled-light-dark-theme/phone-mcp
curl -LO $B/local_music.py -O $B/jarvis_music_tools.py -O $B/install_music.py
python3 install_music.py
python3 server.py --check
```

`install_music.py` edits `server.py` for you: it removes the old `music_tools` register calls and
adds one call to `jarvis_music_tools`. It keeps a backup (`server.py.bak-music`), changes nothing
if run twice, and `python3 install_music.py --undo` puts the backup back. Then restart the server.

Music folder: `~/storage/shared/SpotMusic` by default. Files are named `Song - Artist.flac`
(`.mp3 .m4a .opus .ogg .wav .aac .mp4` also work). Change it with `JARVIS_MUSIC_DIR=/path`.
Extra player options go in `JARVIS_MPV_ARGS`, for example `--audio-device=...` for a Bluetooth speaker.

## Where it plays

Say it when you ask for a song:

| You say | It plays on |
| --- | --- |
| "... on phone" or "... on mobile" (default if you say nothing) | the phone's speaker (mpv) |
| "... on bot" or "... here" or "... on local" | the bot's own speaker (streamed from the phone) |

The bot speaker needs the experimental `bot_speaker_music` option in the firmware build and
`ffmpeg` on the phone. When the bot is on the phone's own hotspot, nothing else is needed: the
phone is the bot's Wi-Fi gateway, and the bot finds it by itself. If the bot is on another Wi-Fi,
set `JARVIS_PHONE_IP` to the phone's address on that Wi-Fi. The stream port is `8765`
(`JARVIS_STREAM_PORT` changes it).

## Tools it provides

`local_music_play`, `local_music_shuffle`, `local_music_find`, `local_music_list`,
`local_music_pause`, `local_music_resume`, `local_music_stop`, `local_music_next`,
`local_music_previous`, `local_music_volume`, `local_music_now_playing`.

`TOOLS` in the file holds each tool's name, description and parameters, and
`LocalMusic().call(name, args)` runs one. Register them in the phone MCP server's `Tools` class.

## Stop the cloud assistant from searching online

In the XiaoZhi console, add this to the agent's role prompt:

> For any request to play music, a song, an artist or a mood, always use the local_music tools
> on the phone. Never use web search or any online music source. If a song is not found, say so.

## Tests

```
cd phone-mcp && python3 -m unittest test_local_music -v
```
