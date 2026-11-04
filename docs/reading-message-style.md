# Reading message style

A display style for small LCD screens: the emotion moves into the status row and
the rest of the screen shows the whole conversation, so an answer can be read
while it is spoken instead of scrolling past in one line at the bottom.

- Enable: `CONFIG_USE_READING_MESSAGE_STYLE=y` (a display style next to the
  default and WeChat styles).
- Demo: [`reading-message-style/demo.html`](reading-message-style/demo.html)
  (one HTML file; open it in a browser to play the eight scenarios and switch
  between word-by-word and sentence highlighting).
- First board using it: FoloToy AI Passport (ESP32-C3, 240x320, no PSRAM).

## Layout

| Area | Content |
|---|---|
| Status row | Emotion, status text (or a short notification such as the volume), network and battery icons; the background follows the state |
| Transcript | One turn per question and answer: the question in grey, the answer in the text color; a new turn opens on a fresh page; older turns are faded |
| Trailing lines | "Listening", the "..." shown while waiting for the answer, and system messages (activation, errors) sit at the end of the newest turn |
| ▼ marker | Shown after paging back, while newer text is below the view |

Status row colors: idle / connecting grey, listening blue, waiting for the answer
amber, speaking green. They are mixed with the theme colors, so light and dark
themes both work.

## Behaviour

| Event | Transcript | Status row | Animation |
|---|---|---|---|
| New conversation (connecting) | Cleared | Connecting | [01-answer](reading-message-style/01-answer.gif) |
| Listening | A "listening" line at the end | Listening, blue | same |
| Question recognised (stt) | The question, then "..." | Please wait, with a seconds counter from 2 s, amber | [04-waiting](reading-message-style/04-waiting.gif) |
| Answer sentence (tts sentence_start) | Appended, not replaced; the part not spoken yet is grey and lights up with the voice | Speaking, green | [01-answer](reading-message-style/01-answer.gif) |
| Follow-up question | A new page with the question at the top; the previous turn is faded | — | [02-follow-up](reading-message-style/02-follow-up.gif) |
| Interrupted while speaking | What was not spoken stays grey; no further sentences | Back to listening | [03-interrupt](reading-message-style/03-interrupt.gif) |
| Conversation ends (idle) | The last turn stays, faded, until the next conversation starts | Clock | [05-end-and-idle](reading-message-style/05-end-and-idle.gif) |
| Network error | What arrived stays, followed by the system message | Error | [06-network](reading-message-style/06-network.gif) |
| Paging / volume (board keys) | Paging back stops following; back at the end it follows again; the volume only shows in the status row | Volume N | [07-paging-volume](reading-message-style/07-paging-volume.gif) |

Servers may send `tts start` before the first sentence (xiaozhi.me does); the
waiting line and counter stay until that sentence arrives.

## How the spoken text is followed

The text of a sentence arrives before its voice (the decode queue holds up to
about 1.2 s of audio), so the highlight follows the actual playback:

1. On `sentence_start` the sentence gets an id; the audio packets that follow are
   tagged with the id and their position in the sentence.
2. The audio service reports (id, position) for every played frame. The display
   only records the value, so the audio task never blocks; a 100 ms LVGL timer
   applies it while an answer is spoken.
3. The audio length of a sentence is not known while it plays, so the position is
   turned into characters with an estimate per character, calibrated with how
   long the earlier sentences really took. 300 ms without a frame finishes the
   sentence; the end of the speech finishes the answer unless it was interrupted.

`CONFIG_READING_MESSAGE_STYLE_FOLLOW_WORDS` (default y) selects word-by-word
highlighting; off lights a whole sentence when its audio starts. The device MCP
tool `self.screen.set_subtitle_mode` (`words` / `sentences`) switches at runtime
and stores the choice in NVS.

## Display API

| Method | Purpose | Default |
|---|---|---|
| `ScrollChat(direction)` | Page the transcript from board keys; returns whether it moved | returns false |
| `OnDeviceStateChanged(state)` | Called after every state change so a style can follow the conversation | no-op |
| `AddSpokenSentence(id, text)` | One numbered sentence of a spoken answer | shown as an assistant message |
| `SetSpeechProgress(id, position_ms)` | Playback progress; called from the audio output task, must not block | no-op |
| `OnSpeechInterrupted()` | The spoken answer was cut off | no-op |
| `SetSpeechFollowWords(words)` | Word-by-word or sentence highlighting | returns false |

## FoloToy AI Passport: key hints at start-up

The Passport board ([`main/boards/folotoy/ai-passport`](../main/boards/folotoy/ai-passport/README.md))
labels its keys next to them every time the firmware starts: the power key on
the left, UP / DOWN / OK on the right. The labels stay for 3 s, turn grey for
1 s and are removed; text appearing in the transcript removes them at once,
status row text does not. Until Wi-Fi connects, a tap on OK enters Wi-Fi setup,
so the OK label reads "Tap: Wi-Fi setup" and changes once the device is online.
See [08-key-hints](reading-message-style/08-key-hints.gif).

## Limits

- Only the two newest turns and at most 1800 bytes are kept (boards without
  PSRAM are short of memory); a longer single answer loses its oldest sentences
  and shows "..." at the start.
- The question uses the same text font as the answer and is told apart by its
  grey color only, with no prefix, so no new translations are needed.
- Rare glyphs pushed by the server for an older turn may have left the glyph
  cache when paging back to it, and then show as boxes.
