# Persistent WebSocket: Техническое задание для voice_gateway

## 1. Что изменилось

ESP32 теперь поддерживает **постоянное WebSocket-соединение** с сервером. Соединение открывается сразу после активации устройства (WiFi connected + OTA check) и держится постоянно, независимо от наличия голосовой сессии.

### Старая модель (до изменений)

```
Wake word → WS connect → hello → аудиосессия → CloseAudioChannel → WS disconnect
```

### Новая модель

```
WiFi connected → WS connect → hello → [WS всегда открыт]
                                        ↕ ping/pong каждые 30с
                                        ↕ reconnect при обрыве
                             
   Wake word → audio_channel_open = true → разговор → audio_channel_open = false
   Server TTS → те же JSON-команды через тот же WS
```

## 2. Протокол — что должен делать сервер

### 2.1. Handshake (без изменений)

Клиент подключается, шлёт `hello`:
```json
{
  "type": "hello",
  "version": 3,
  "transport": "websocket",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  },
  "features": {}
}
```

Сервер отвечает `hello` (как и раньше):
```json
{
  "type": "hello",
  "transport": "websocket",
  "audio_params": {
    "sample_rate": 24000,
    "frame_duration": 60
  },
  "session_id": "uuid-xxx"
}
```

**Важно:** `hello` шлётся **один раз** при подключении. Пересылать его при каждой голосовой сессии **не нужно**.

### 2.2. Ping/Pong — **критично для работы**

Клиент шлёт WebSocket Ping-фрейм (0x9, opcode 9) каждые 30 секунд.

Сервер **обязан** отвечать Pong-фреймом (0xA, opcode 10) **в течение разумного времени**. Если сервер не отвечает на Ping — соединение не рвётся (TCP keepalive может держать его), но клиент не переподключается без необходимости.

**Рекомендация:** настройте ваш WS-сервер (FastAPI/websockets/uvicorn) обрабатывать Ping автоматически. Большинство библиотек делают это по умолчанию. Проверьте, что:
- `ping_interval` **не установлен** (или больше 60с) — клиент сам шлёт Ping
- `ping_timeout` не сбрасывает соединение при задержке Pong

### 2.3. TTS (alert) через существующий WS — **основная фича**

Для доставки TTS-алерта сервер отправляет в любой момент времени (даже когда устройство в Idle):
```json
{
  "type": "tts",
  "state": "start"
}
```

Устройство:
1. Переходит в состояние `Speaking`
2. Включает экран
3. Готово принимать аудиоданные (OPUS binary frames)

После окончания TTS:
```json
{
  "type": "tts",
  "state": "stop"
}
```

Устройство:
1. Переходит в `Idle` (если TTS начат сервером, а не голосовой командой)
2. Микрофон выключен, экран показывает standby
3. Wake word detection включён

### 2.4. Сервер-инициированный TTS vs обычный разговор — **одинаковые JSON**

JSON-команды для TTS (start/stop/sentence_start) **абсолютно те же**, что и при обычном разговоре. Разница только в том, что они приходят без предшествующего wake word.

Устройство само определяет, была ли сессия инициирована сервером (было ли устройство в Idle до `tts start`). Если да — после `tts stop` возвращается в Idle. Если нет (пользователь сказал wake word) — поведение не изменилось (Listening после stop, если режим AutoStop).

### 2.5. Другие типы сообщений — без изменений

- `{"type":"stt", "text":"..."}` — от сервера к устройству (отображать на экране)
- `{"type":"llm", "emotion":"..."}` — эмоции на экране
- `{"type":"mcp", "payload":{...}}` — MCP-команды
- `{"type":"alert", "status":"...", "message":"...", "emotion":"..."}` — алерты
- `{"type":"system", "command":"reboot"}` — системные команды

Всё это работает через тот же persistent WS.

## 3. Работа с несколькими устройствами

Так как каждое устройство держит **свой** persistent WS, сервер может в любой момент отправить TTS любому устройству. Идентификация — по session_id (или Device-Id в http headers при handshake).

**Сценарий:**
1. Устройство A подключается → WS открыт, session_id="a-xxx"
2. Устройство B подключается → WS открыт, session_id="b-yyy"
3. Сервер хочет отправить TTS на A → шлёт JSON через WS A
4. Сервер хочет отправить TTS на B → шлёт JSON через WS B

Никакой дополнительной маршрутизации не требуется — WS сам по себе является каналом к конкретному устройству.

## 4. reconnect — что видит сервер

При обрыве связи (WiFi падает, ESP32 перезагружается и т.п.):
1. Старое WS-соединение закрывается (TCP RST или timeout)
2. ESP32 переподключается через 1с, 2с, 4с... до 60с (экспоненциальный backoff)
3. Новый `hello` с новым session_id
4. Старый session_id можно считать умершим

Серверу **не нужно** ничего делать для reconnect — клиент сам переподключается.

Если сервер заметил, что устройство отключилось (TCP close), можно очистить внутреннее состояние. При переподключении устройство всегда шлёт новый hello.

## 5. Рекомендации по реализации на voice_gateway

### FastAPI + websockets

```python
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

app = FastAPI()

class ConnectionManager:
    def __init__(self):
        self.active_connections: dict[str, WebSocket] = {}
    
    async def connect(self, websocket: WebSocket, device_id: str):
        await websocket.accept()
        self.active_connections[device_id] = websocket
    
    def disconnect(self, device_id: str):
        self.active_connections.pop(device_id, None)
    
    async def send_tts_alert(self, device_id: str, text: str):
        if device_id not in self.active_connections:
            return
        ws = self.active_connections[device_id]
        await ws.send_json({"type": "tts", "state": "start"})
        # ... send OPUS audio frames over the same WS ...
        await ws.send_json({"type": "tts", "state": "stop"})

manager = ConnectionManager()

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    # Первое сообщение — hello от клиента
    data = await websocket.receive_json()
    device_id = data.get("device_id", websocket.headers.get("Device-Id", "unknown"))
    
    await manager.connect(websocket, device_id)
    try:
        while True:
            data = await websocket.receive()
            # Обработка данных (JSON или binary)
    except WebSocketDisconnect:
        manager.disconnect(device_id)
```

### Важные моменты

| Что | Детали |
|-----|--------|
| **Ping** | Не отключайте клиента за отсутствие Pong — клиент сам шлёт Ping. Сервер отвечает автоматически. |
| **hello** | Один раз при подключении. Не ждите hello перед каждым TTS. |
| **Binary** | OPUS-аудио шлётся как binary WS frames. JSON — как text frames. |
| **TCP keepalive** | Не полагайтесь только на Ping — настройте TCP keepalive на уровне ОС или библиотеки. |
| **Session timeout** | Если устройство не подавало признаков жизни >5 минут, можно считать его отключённым. |

## 6. Проверка работоспособности

1. Устройство загружается → видим в логах `"Persistent websocket established"`
2. Сервер шлёт `{"type":"tts","state":"start"}` → устройство переходит в Speaking
3. Сервер шлёт OPUS-аудио → динамик воспроизводит
4. Сервер шлёт `{"type":"tts","state":"stop"}` → устройство возвращается в Idle
5. Отключаем WiFi → видим reconnect через 1-2-4-8... секунд
6. Говорим wake word → обычный разговор работает через тот же WS
7. После разговора → WS всё ещё открыт (можно снова отправить TTS)
