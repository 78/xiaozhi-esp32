# Dynamic Text Glyph Push Extension

This document defines version 1 of the `glyph_push` protocol extension. The extension lets a server
send bitmap glyphs that are missing from a device's installed text font. It applies equally to the
WebSocket and MQTT/UDP transports because capability advertisement and incoming JSON handling are
implemented in the shared protocol layer.

The extension supplements text rendering only. It does not change the text, TTS audio, or STT
semantics of the containing message.

## 1. Capability advertisement

The device advertises support in its client `hello` message:

```json
{
  "type": "hello",
  "version": 1,
  "features": {
    "mcp": true,
    "glyph_push": true
  },
  "text_font": {
    "bundle": "noto-v1",
    "charset": "common",
    "size": 20,
    "bpp": 4
  }
}
```

`features.glyph_push` indicates support for this extension. A server must treat a missing or false
value as unsupported. The `v` field in each pushed payload carries the extension version.

The `text_font` object describes the exact font data installed on the device:

| Field | Type | Meaning |
|---|---|---|
| `bundle` | string | Explicit font bundle identifier. It changes when glyph metrics, rendering behavior, character sets, or wire compatibility change. |
| `charset` | string | Installed character set. Version 1 devices report `basic` or `common`. |
| `size` | number | Text font pixel profile used by the firmware. |
| `bpp` | number | Bits per pixel of the text font bitmap, currently `1` or `4`. |

`basic` is the font linked into the firmware. The standard XiaoZhi assets report `common` after
loading their common font from the assets partition. The server must use the values from each
device's hello message rather than inferring them from the board model.

An OTA assets package may replace the text font with a different size, bpp, character set, or font
family. The firmware still loads any structurally valid CBIN font. When the package also provides
complete `text_font_meta` fields, the device advertises those active runtime values and validates
glyph pushes against them. A legacy or custom package without compatible glyph metadata continues
to use its custom font, emoji, colors, and background, but advertises `glyph_push: false` and omits
`text_font`. This prevents incompatible fallback glyphs without restricting theme customization.

## 2. Server glyph payload

The server may attach a `glyph_push` object to either of these server-to-device messages:

- a TTS message with `"type": "tts"` and `"state": "sentence_start"`;
- an STT message with `"type": "stt"`.

Example:

```json
{
  "type": "tts",
  "state": "sentence_start",
  "text": "𠮷野家",
  "glyph_push": {
    "v": 1,
    "bundle": "noto-v1",
    "size": 20,
    "bpp": 4,
    "glyphs": [
      {
        "codepoint": 134071,
        "adv_w": 320,
        "box_w": 20,
        "box_h": 20,
        "ofs_x": 0,
        "ofs_y": 0,
        "bitmap": "<base64-encoded bitmap>"
      }
    ]
  }
}
```

The payload header must match the device capability exactly:

| Field | Requirement |
|---|---|
| `v` | Must be `1`. |
| `bundle` | Must equal `text_font.bundle`. |
| `size` | Must equal `text_font.size`. |
| `bpp` | Must equal `text_font.bpp`. |
| `glyphs` | The partial glyph batch being pushed, containing at most 64 entries. |

Each item uses the LVGL native bitmap-font metrics:

| Field | Meaning |
|---|---|
| `codepoint` | Unicode code point from `1` through `0x10FFFF`. |
| `adv_w` | Horizontal advance in LVGL fixed-point units with four fractional bits (one pixel is 16 units). |
| `box_w`, `box_h` | Bitmap dimensions in pixels. Each dimension must be from 0 through 64. |
| `ofs_x`, `ofs_y` | Signed 16-bit glyph offsets relative to the text baseline and cursor position. |
| `bitmap` | Base64 encoding of the uncompressed LVGL plain bitmap. |

The decoded bitmap length must be exactly:

```text
ceil(box_w * box_h * bpp / 8)
```

The bitmap must use the same plain, zero-stride layout as the matching Noto full-bundle CBIN font.
Servers should extract and forward the bitmap and metrics directly from that CBIN profile instead of
rasterizing an unrelated font at request time.

The sum of decoded bitmap lengths in one payload must not exceed 64 KiB. If any header, glyph, or
bitmap is invalid, the device rejects the entire glyph payload but still displays the message text
using its installed fonts. A PSRAM device may also use fallback glyphs cached by earlier messages.

## 3. Server selection algorithm

For every connection that advertises `glyph_push: true`, the server should:

1. Resolve the full font bundle identified by `text_font.bundle`.
2. Select the CBIN profile matching `text_font.size` and `text_font.bpp`.
3. Decode the message text into Unicode code points.
4. Remove control characters, duplicates, and code points already present in
   `text_font.charset`.
5. Extract the remaining glyphs from the full bundle.
6. Apply the per-message limits and attach one `glyph_push` object to the text message.
7. Omit `glyph_push` when no missing glyph is available.

The installed text font is searched before the dynamic fallback font. Pushed glyphs therefore fill
missing code points; they do not override glyphs in `basic` or `common`.

The full font bundle may be shared by all device connections in a server process. Per-connection
work is limited to using the capability tuple `(bundle, charset, size, bpp)` to select which glyphs
are missing and which profile to read.

## 4. Device cache behavior

All glyphs in one message are inserted first, followed by a single fallback-font rebuild. The device
never rebuilds the font once per glyph.

On a device with initialized PSRAM:

- bitmap, cmap, descriptor, and cache-entry storage is allocated in PSRAM;
- glyphs are retained across messages;
- the cache holds at most 256 glyphs and 64 KiB of decoded bitmap data;
- the least recently inserted or updated entries are evicted when a limit is exceeded.

On a device without PSRAM:

- storage uses internal RAM;
- only the current message's glyph batch is retained;
- the next text message replaces or clears the previous batch.

This distinction does not affect the protocol. A server can send the glyphs needed by each message
without knowing whether the device has PSRAM.

## 5. Compatibility and versioning

The server must not send glyphs when any of these conditions is true:

- `features.glyph_push` is absent or not supported by the server;
- the server does not have the advertised bundle;
- no full-font profile matches the advertised size and bpp;
- the glyph data cannot satisfy the version 1 validation rules.

Fallback is automatic: messages without `glyph_push`, and messages whose glyph payload is rejected,
are still processed normally.

When the font generator changes metrics, bitmap layout, source fonts, character sets, or rendering
behavior, publish a new explicit bundle identifier. Do not serve glyphs from one bundle under
another bundle's identifier even if their size and bpp happen to match.

## 6. Security requirements

Glyph payloads are untrusted network input. Implementations must validate the complete payload before
mutating a live font, bound both item count and decoded size, verify base64 decoded length, and reject
invalid code points or metrics. Servers should also bound their own per-message work and avoid
sending glyphs already covered by the advertised charset.
