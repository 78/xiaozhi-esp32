# JonnyBot Architecture

This document records the initial firmware layout for the JonnyBot fork. The
repository root is the firmware root; there is no additional `firmware/`
directory.

## Planned modules

```text
main/
├── application.cc / .h             # Application lifecycle and agent FSM
├── device_state_machine.cc / .h    # Legal runtime state transitions
├── boards/
│   ├── common/                     # Shared board interfaces and helpers
│   └── seeed-xiao-esp32s3-sense/   # JonnyBot board implementation
├── robot/                          # Robot control, sensors, memory, intent, skills
├── net/                            # HTTP, DeepSeek, Yandex STT/TTS clients
├── audio/                          # Reuse/adapt XiaoZhi audio components
├── display/                        # Reuse/adapt display components; robot face
└── mcp_server.cc / .h              # MCP tool registry and dispatch

scripts/                            # Build and asset tooling
partitions/                         # Flash partition tables
docs/                               # Architecture and protocol documentation
```

## Robot modules

The planned `main/robot/` modules are `motors`, `safety`, `sensors`, `memory`,
`intent`, `skills`, `parser`, `context`, and `base64`.

Motor commands should be queued for a dedicated motor task. Safety stop must be
able to interrupt motion independently of normal command processing. MCP tools
should enqueue requests rather than execute blocking motion in their callbacks.

The single wheel encoder is intended to estimate rough travel distance, not to
provide SLAM or precise odometry. A movement action specifies a target
`distance` in its JSON; the motor controller uses encoder feedback to stop
after approximately that travel distance, rather than running for a specified
`delay`. Wheel slip, turns, and uneven surfaces make the estimate approximate.
After each action, report estimated travel and the range sensor's before/after
readings so the agent can use them as feedback on the next iteration. The
VL53L0X measures distance to an obstacle; it does not by itself identify which
object was approached.

## Network modules

The planned `main/net/` modules are a shared HTTP client and DeepSeek, Yandex
STT, and Yandex TTS clients. REST requests must not block the application event
loop or audio tasks.

## Confirmed hardware target

| Component | Specification |
| --- | --- |
| Board | Seeed XIAO ESP32S3 Sense, 8 MB flash, 8 MB PSRAM |
| Motors | CNMAWAY GA12-N20, 6 V, 150 RPM, approximately 1:100 gearbox |
| Motor driver | DRV8833, dual channel |
| Encoder | One AS5600 with a 6 x 2.5 mm diametric magnet, I2C |
| Range sensor | VL53L0X GY-530, I2C, default address 0x29 |
| Display | 1.69-inch ST7789V3, 240 x 280, SPI |
| Audio output | MAX98357A I2S mono amplifier and 8-ohm speaker |

Board pin assignments and peripheral bus topology still need to be recorded
before the board implementation is wired into the build.

## Agent interaction contract (draft)

The agent receives a camera frame and the user's recognized text, then returns
one strict JSON action per interaction. The proposed fields are `action`,
`params`, `reasoning`, `speak_human`, `imotion`, `intent`, and `write_skill`.
The action set is `move_forward`, `move_backward`, `turn_left`, `turn_right`,
`stop`, `idle`, and `speak`.

Translation actions use `distance_m` and `speed` (proposed ranges 0.05-2.0 m
and 0.1-0.4 m/s). Turns use `angle_deg` (5-180 degrees, default 90). Movement
is distance-based: the motor controller uses the AS5600 feedback to stop at
the requested approximate travel distance, not a duration-based delay. The
single encoder does not provide precise odometry; calibration and limitations
from slip, turns, and uneven surfaces must be reflected in feedback to the
agent.

The range sensor is a forward-facing VL53L0X time-of-flight sensor, not an
infrared proximity switch. Its valid measurement range and the obstacle
threshold for stopping motion must be established from the selected module and
physical tests; do not assume it can reliably detect at 1 cm.

For multi-step tasks, `intent` is an LLM-authored plan and working context.
The LLM is the authority that decides which step is current, marks steps
complete, and updates or clears the plan; firmware must not advance intent
steps algorithmically. The firmware carries the latest intent and previous
action result into the next agent request so the LLM can make that decision.
The JSON schema, validation limits, safety behavior, and prompt wording remain
to be finalized before implementing the response parser.

### Working-prompt context example

The request can combine elapsed time, conversation history, the LLM-maintained
intent, timestamped sensor readings, the previous action, a camera image, and
the latest STT text. This is an illustrative layout, not a finalized prompt
template or sensor schema:

```text
СИТУАЦИЯ:
Время: 1.5

ИСТОРИЯ ДИАЛОГА:
Человек: Подъедь к баллончику
Робот: Поворачиваюсь к баллончику. Потом подъеду ближе.

ТЕКУЩЕЕ НАМЕРЕНИЕ:
Step 1: [x] повернуть корпус, чтобы баллончик был в центре кадра
Step 2: [>] определить расстояние и подъехать
Step 3: убедиться, что баллончик вблизи, сообщить человеку

ДАННЫЕ СЕНСОРОВ:
  энкодер: пройдено 0.00 м [возраст: 0.01с]
  дальномер: свободно (>10 см) [возраст: 0.02с]

ПРЕДЫДУЩЕЕ ДЕЙСТВИЕ:
turn_right(angle_deg=30) [возраст: 1.5с]

[КАДР С КАМЕРЫ]
<base64 JPEG, QVGA>

ГОЛОС ЧЕЛОВЕКА (STT):
<пусто>
```

Sensor values and their ages should be measured when captured, so the model
can distinguish recent readings from stale context. The exact distance
thresholds, units, age representation, history window, image transport, and
empty-STT encoding remain to be specified. The firmware reports observations
and action outcomes; the LLM interprets them and owns intent progression.

### Context and memory lifetimes

The agent needs three distinct context stores:

- **Conversation history:** a bounded chronological log of user utterances and
  robot replies, trimmed to the context budget when building a request. It may
  be persisted as a file if history must survive reboot; retention and size
  limits remain to be decided.
- **Current intent:** the latest plan returned by the LLM, replaced only when
  the LLM returns an updated value. It is temporary task state and can live in
  RAM; persist it only if continuing an in-progress task after reboot is a
  requirement.
- **Sensor memory:** a bounded in-memory ring buffer of timestamped readings
  and action outcomes. The prompt builder selects the newest valid entries
  and includes their age; stale or overwritten samples are not treated as
  current state. Avoid writing every sample to flash.

These are logical stores, not a commitment to particular files or filesystem.
Their schemas, capacities, timestamp source, stale-data policy, and reboot
behavior must be specified before implementation. A separate long-term memory
for learned skills or user-approved facts is a different feature from the
short-lived working context above.

## Decisions pending before implementation

- Define configuration and persistent-data formats and size limits.
- Choose storage for mutable robot data and verify the required ESP-IDF
  filesystem component.
- Confirm flash size, OTA requirements, and a non-overlapping partition layout.
  The existing `partitions/v2/8m.csv` must not be changed until these are known.
- Decide where robot MCP tools are registered in the application lifecycle.

This is a planning scaffold only. The new board and modules are not yet wired
into `main/CMakeLists.txt`, Kconfig, or the build script.
