# reTerminal D1001 HTTP 推送面板

设备联网后在 **8080 端口**提供一个 HTTP 服务，局域网内任何客户端都可以把内容推到屏幕上：Markdown 渲染、让用户在屏上做选择并取回结果。面板以底部悬浮卡片显示，不遮挡状态栏、表情和屏底对话文本。

## 远程唤醒（可选）

`GET /api/wake/status` 查询能力，`POST /api/wake` 提交唤醒请求。远程唤醒默认关闭，通过 user-only MCP 工具 `self.remote_wake.configure` 配置 `enabled`（布尔值）和 `token`（16–128 个安全字符）；token 只写入设备本地设置，不会从状态接口或日志回显。启用后两个接口都使用 `Authorization: Bearer <token>`。请求体为 `{"request_id":"...","mode":"context|speak","text":"...","ttl_ms":15000}`，`ttl_ms` 必须是 1–15000 的整数。`context` 最多 25 字节，`speak` 最多 512 字节；HTTP 202 仅表示已排队，设备忙时返回 409，重复且内容相同的 `request_id` 在短窗口内幂等。`speak` 依赖启用 `CONFIG_SEND_WAKE_WORD_DATA` 的本地服务端 `[device_call]` TTS 路径。

`context` 适合云端或本地服务生成简短唤醒提示；`speak` 只承诺本地服务端的 `[device_call]` TTS，不代表远端云端口可直接播放。

设备 IP 可在设置 → Wi-Fi 页查看，或从路由器 / 串口日志（`sta ip:`）获取。

## 接口

### `GET /`

纯文本用法速查。

### `POST /panel/markdown` — 推送内容

Body 为 Markdown 文本（UTF-8，≤32KB）。支持的子集：

| 语法 | 渲染 |
|---|---|
| `# 标题` / `## 标题` | 30px 蓝色标题 |
| `- 项目` / `* 项目` | 圆点列表 |
| `\| 单元格 \|` 表格 | `lv_table`，表头行蓝底白字 |
| 普通段落 | 20px 正文，自动换行（连续行合并为一段） |
| `**加粗**` / `` `代码` `` | 标记剥离，按普通文本渲染 |

可选 query 参数：

| 参数 | 作用 |
|---|---|
| `ttl_s=30` | 30 秒后自动消失（1–3600；不带则常驻） |
| `size=large` | 正文也用 30px 大字（默认 20px） |

```bash
curl -X POST --data-binary @notes.md "http://<IP>:8080/panel/markdown"
curl -X POST --data-binary $'# 提醒\n- 三点开会' "http://<IP>:8080/panel/markdown?ttl_s=60&size=large"
```

返回 `200 OK`。再次推送会原位替换内容。有 choice 挂起时返回 400。

### `POST /panel/choice` — 屏上选择（阻塞式）

Body 为 JSON：

```json
{"title": "选一个模组", "options": ["SC202CS", "OV5647"], "timeout_s": 60}
```

- `options`：1–16 个字符串，渲染为全宽按钮
- `timeout_s`：等待时长，默认 60，上限 600

HTTP 请求**阻塞**直到用户点选或超时：

| 结果 | 响应 |
|---|---|
| 用户点了第 i 项 | `200` `{"selected": i, "option": "..."}` |
| 超时无人点 | `408` `{"error":"timeout"}` |
| 被关闭（✕ / 点外部 / 状态切换 / close 接口） | `410` `{"error":"dismissed"}` |

```bash
curl -X POST -d '{"title":"部署到哪台","options":["spark","orin-nano"],"timeout_s":120}' \
     "http://<IP>:8080/panel/choice"
```

同一时刻只允许一个 choice 挂起（并发返回 400）。

### `POST /panel/close` — 远程关闭

关闭当前面板；有 choice 挂起时等价于用户拒选（调用方收到 410）。

## 面板消失的所有途径

1. 右上角 ✕
2. **点击卡片外的任意区域**
3. **设备对话状态切换**（打开面板后设备状态一变即消失，500ms 内生效——例如唤醒开始对话、回答开始/结束）
4. `ttl_s` 到期
5. `POST /panel/close`
6. 新的 markdown 推送（原位替换，不算消失）

## 组合用法：对比 + 选择

```bash
IP=192.168.10.168
# 1. 推对比表格（30 秒自动消失，防止忘关）
curl -X POST --data-binary $'# 模组对比\n\n| 型号 | 分辨率 | 接口 |\n|---|---|---|\n| SC202CS | 2MP | MIPI |\n| OV5647 | 5MP | MIPI |' \
     "http://$IP:8080/panel/markdown?ttl_s=30"
sleep 8
# 2. 让人选择，取回结果
CHOICE=$(curl -s -X POST -d '{"title":"用哪个","options":["SC202CS","OV5647"]}' "http://$IP:8080/panel/choice")
echo "$CHOICE"   # {"selected":0,"option":"SC202CS"}
```

Python：

```python
import requests
IP = "192.168.10.168"
requests.post(f"http://{IP}:8080/panel/markdown", data="# 部署确认\n- 目标: spark".encode(),
              params={"ttl_s": 30})
r = requests.post(f"http://{IP}:8080/panel/choice",
                  json={"title": "继续部署？", "options": ["继续", "取消"], "timeout_s": 120},
                  timeout=130)
if r.status_code == 200 and r.json()["selected"] == 0:
    print("用户确认，开始部署")
```

## 实现说明

- 代码：本目录 `push_panel.{h,cc}`，随网络栈启动（`StartNetwork`）
- 卡片几何：宽 96%，高度贴合内容（上限为旋转后屏高的 55%，超出卡内滚动），底部为屏底聊天条让位（`BottomInsetProvider` 实测 `bottom_bar_` 高度）
- 30px 字体为 `font_noto_sans_basic_30_4`（常用字子集，约 250KB flash），生僻字可能缺字形
- 所有 UI 操作持 display lock，与语音对话、设置页共存；旋转后按新分辨率自适应
- 无鉴权，仅监听局域网；不要暴露到公网

## 人脸识别（外部识别端点）

设备无 NPU，识别走可配置的 HTTP 端点。你的识别服务需实现：

```
POST <endpoint>   Content-Type: image/jpeg   Body: JPEG 帧
200 → {"faces":[{"name":"harvest","score":92}]}
```

`score` 接受 0-1 浮点或 0-100；检出但库中无匹配用 `"unknown"`；无人时 `faces` 为空。

### `POST /face/config` / `GET /face/status`

```bash
curl -X POST -d '{"mode":1,"endpoint":"http://192.168.3.73:8399/recognize","interval_s":5}' \
     "http://<IP>:8080/face/config"
curl "http://<IP>:8080/face/status"
```

字段：`mode`(0 关/1 人脸唤醒)、`endpoint`、`interval_s`、`threshold`(0-100)、`duration_s` 持续确认、`cooldown_s` 冷却、`known_only`(1=陌生脸不唤醒)。NVS 持久化。

### 两个用途

1. **人脸唤醒**：mode=1 时空闲态按 interval 采集，合格人脸持续 duration_s 秒即以 `<detect>face detected: 名字</detect>` 触发对话（watcher 同款状态机：确认→唤醒→冷却等人离开）
2. **MCP 鉴权**：助手可调用 `self.face.verify` 识别当前操作者身份，在敏感操作前做权限判断；`self.face.param_get/param_set` 支持语音调参
