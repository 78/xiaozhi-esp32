# 动态文字 Glyph Push 扩展

本文档定义 `glyph_push` 协议扩展的版本 1。服务器可以通过该扩展向设备下发本地文字
字库中缺失的位图 glyph。能力声明和 JSON 消息处理位于公共协议层，因此 WebSocket 与
MQTT/UDP 使用完全相同的扩展格式。

该扩展只补充文字渲染能力，不改变消息中的文本、TTS 音频或 STT 语义。

## 1. 能力声明

设备在客户端 `hello` 消息中声明能力：

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

`features.glyph_push` 表示设备支持该扩展。字段缺失或为 false 时，服务器必须视为设备不支持。
每次推送 payload 中的 `v` 字段负责表达扩展版本。

`text_font` 描述设备实际安装的文字字库：

| 字段 | 类型 | 含义 |
|---|---|---|
| `bundle` | string | 显式的字体 bundle 标识。glyph 度量、渲染方式、字符集或线格式变化时需要更换。 |
| `charset` | string | 已安装字符集。版本 1 设备报告 `basic` 或 `common`。 |
| `size` | number | 固件使用的文字字体像素规格。 |
| `bpp` | number | 字体位图的每像素位数，目前为 `1` 或 `4`。 |

`basic` 是链接进固件的字库。标准小智 assets 从分区加载 common 字库后会报告 `common`。
服务器必须使用每个连接在 hello 中报告的实际值，不能根据板型推测。

OTA assets 可以把文字字体替换为不同字号、bpp、字符集或字体家族的 CBIN 字体。固件仍会
加载任何结构有效的 CBIN 字体；如果 assets 同时提供完整的 `text_font_meta`，设备会声明当前
实际使用的运行时字体参数，并据此校验 glyph push。旧版或自定义 assets 缺少兼容的 glyph
metadata 时，自定义字体、表情、颜色和背景仍然正常使用，但设备会声明 `glyph_push: false`
并省略 `text_font`，从而只禁止不兼容的 fallback glyph，不限制主题定制能力。

## 2. 服务器下发格式

服务器可以在以下消息中附加 `glyph_push` 对象：

- `"type": "tts"`、`"state": "sentence_start"` 的 TTS 消息；
- `"type": "stt"` 的 STT 消息。

示例：

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
        "bitmap": "<base64 编码的位图>"
      }
    ]
  }
}
```

payload 头必须与设备能力完全匹配：

| 字段 | 要求 |
|---|---|
| `v` | 必须为 `1`。 |
| `bundle` | 必须等于 `text_font.bundle`。 |
| `size` | 必须等于 `text_font.size`。 |
| `bpp` | 必须等于 `text_font.bpp`。 |
| `glyphs` | 本次增量推送的 glyph 数组，最多 64 项。 |

每个 item 使用 LVGL 原生位图字体度量：

| 字段 | 含义 |
|---|---|
| `codepoint` | `1` 到 `0x10FFFF` 的 Unicode code point。 |
| `adv_w` | 带 4 位小数的 LVGL 定点水平 advance，16 个单位等于 1 像素。 |
| `box_w`、`box_h` | 位图宽高，单项范围为 0 到 64 像素。 |
| `ofs_x`、`ofs_y` | 相对于文字基线和光标位置的有符号 16 位偏移。 |
| `bitmap` | 未压缩 LVGL plain 位图的 Base64 编码。 |

解码后的位图长度必须严格等于：

```text
ceil(box_w * box_h * bpp / 8)
```

位图必须与对应 Noto full bundle CBIN 字体使用相同的 plain、零 stride 布局。服务器应直接
提取相同 profile 的 CBIN 位图和度量，不应临时使用其他字体重新光栅化。

单条 payload 的解码位图总长度不得超过 64 KiB。任意头字段、glyph 或位图无效时，设备会
拒绝整批 glyph，但仍使用本地字体显示消息文本；有 PSRAM 的设备还可能使用此前缓存的
fallback glyph。

## 3. 服务器选择流程

对于声明 `glyph_push: true` 的每个连接，服务器应：

1. 根据 `text_font.bundle` 找到对应 full 字体 bundle。
2. 根据 `text_font.size` 和 `text_font.bpp` 选择 CBIN profile。
3. 将消息文本解码为 Unicode code point。
4. 去掉控制字符、重复字符及 `text_font.charset` 已包含的字符。
5. 从 full bundle 提取剩余 glyph。
6. 执行单消息限制，并将一个 `glyph_push` 对象附加到文字消息。
7. 没有可用的缺失 glyph 时省略 `glyph_push`。

设备先查询本地文字字体，再查询动态 fallback。因此 glyph push 只补充缺字，不会覆盖
`basic` 或 `common` 中已有的 glyph。

同一服务器进程中的所有设备连接可以共享 full 字体 bundle 服务。每个连接只需根据
`(bundle, charset, size, bpp)` 能力组合判断缺字并选择 profile。

## 4. 设备缓存行为

同一消息中的全部 glyph 会先加入缓存，随后只执行一次 fallback 字体 rebuild，不会每加入
一个 glyph 就 rebuild 一次。

设备有已初始化的 PSRAM 时：

- bitmap、cmap、descriptor 和缓存条目存放在 PSRAM；
- glyph 跨消息保留；
- 缓存上限为 256 个 glyph 和 64 KiB 解码位图；
- 超限时淘汰最早插入或更新的条目。

设备没有 PSRAM 时：

- 数据使用内部 RAM；
- 只保留当前消息的 glyph 批次；
- 下一条文字消息会替换或清空上一批 glyph。

该差异不改变协议。服务器无需知道设备是否有 PSRAM，可以为每条消息发送它所需的 glyph。

## 5. 兼容与版本管理

以下任一条件成立时，服务器不得发送 glyph：

- `features.glyph_push` 缺失、为 false，或服务器不支持该扩展；
- 服务器没有设备声明的 bundle；
- 找不到匹配 size 和 bpp 的 full 字体 profile；
- glyph 数据不满足版本 1 的校验要求。

兼容降级是自动的：没有 `glyph_push`，或 glyph payload 被拒绝时，设备仍会正常处理消息。

字体生成器的度量、位图布局、源字体、字符集或渲染方式变化时，应发布新的显式 bundle
标识。即使 size 和 bpp 相同，也不能用一个 bundle 的标识下发另一个 bundle 的 glyph。

## 6. 安全要求

glyph payload 是不可信网络输入。实现必须先完整校验 payload，再修改正在使用的字体；限制
item 数量和解码总长度；验证 Base64 解码长度；拒绝非法 code point 或度量。服务器也应限制
单消息处理量，避免重复发送设备声明字符集已经包含的 glyph。
