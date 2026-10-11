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

The planned `main/robot/` modules are `motors`, `safety`, `memory`, `intent`,
`skills`, `parser`, `context`, and `base64`. Sensor drivers are deferred from
the MVP.

Motor commands should be queued for a dedicated motor task. Safety stop must be
able to interrupt motion independently of normal command processing. MCP tools
should enqueue requests rather than execute blocking motion in their callbacks.

### MVP motion control

The MVP is open-loop: it has no wheel encoder or range sensor. Movement actions
still specify `distance_m` and `speed`; firmware converts them to motor runtime
using a measured speed calibration table, then stops the motors. For example,
if calibration shows that a particular speed setting produces about 0.22 m/s,
the runtime for a requested distance is derived from that measured rate. The
LLM chooses the action parameters, but deterministic firmware should perform
the distance-to-runtime calculation rather than asking the model to calculate
timing. Rotation also requires empirical calibration.

Open-loop travel is only an estimate: battery voltage, floor surface, load,
motor variation, and turning can change the actual distance. The prompt and
responses must not imply measured odometry or obstacle detection in the MVP.
The initial sensorless design is chosen to reduce wiring, components, and
failure modes and to improve reproducibility. Adding the AS5600 encoder,
VL53L0X range sensor, or a second MCU for display expansion is deferred to a
later hardware iteration.

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
| Display | 1.69-inch ST7789V3, 240 x 280, SPI |
| Audio output | MAX98357A I2S mono amplifier and 8-ohm speaker |

The MVP uses the XIAO Sense camera and PDM microphone. The initially considered
AS5600 encoder and VL53L0X range sensor are deferred. A second MCU is also
deferred; the MVP display remains connected to the XIAO. The proposed GPIO
assignments are based on JonnyBot's selected modules, not copied from Huy
Vector's Wall-E AI. Huy's [materials page](https://www.huyvector.org/robots-kinetic/wall-e-ai)
confirms the XIAO ESP32-S3 Sense, but lists a 1.54-inch display, MAX98357BGA
amplifier, Mini L298N driver, and 17-68 RPM gear motors. JonnyBot uses a
1.69-inch ST7789V3, MAX98357A breakout, DRV8833, and GA12-N20 motors. Huy's
[public installer repository](https://github.com/huyvector/wall-e-ai-installer)
contains a prebuilt firmware image, not its source or a verified GPIO wiring
table. Therefore Wall-E is a useful build reference, but its exact peripheral
pinout cannot be assumed to apply here.

The proposed JonnyBot GPIO assignments are:

| XIAO pin | ESP32-S3 GPIO | Signal |
| --- | ---: | --- |
| D0 | 1 | DRV8833 AIN1 (left motor) |
| D1 | 2 | DRV8833 AIN2 (left motor) |
| D2 | 3 | Reserved; leave unconnected (JTAG-source strap) |
| D3 | 4 | DRV8833 BIN1 (right motor) |
| D4 | 5 | DRV8833 BIN2 (right motor) |
| D5 | 6 | MAX98357A BCLK |
| D6 | 43 | MAX98357A L/RCLK (WS) |
| D7 | 44 | MAX98357A DIN |
| D8 | 7 | ST7789 SCLK (module may label this SCL) |
| D9 | 8 | ST7789 MOSI (module may label this SDA or DIN) |
| D10 | 9 | ST7789 DC |

To fit the display without consuming another GPIO, connect its active-low CS
to GND; it is the only SPI device on this bus. Connect display RESET to the
XIAO reset line and backlight to 3V3, subject to the display module's onboard
current limiting. Tie DRV8833 nSLEEP high to 3V3. Add pulldowns to all four
motor inputs so the outputs remain inactive during boot and reset, and set
those GPIO outputs low before enabling motor control in firmware. Use a
separate motor supply and connect its ground to XIAO ground.

This pin plan reserves GPIO3 rather than risking a motor-control output on a
strap pin. GPIO43/44 are also UART0 pins, so use native USB Serial/JTAG for
logs and do not route the console through UART0. Sense expansion-board microSD
uses GPIO7/8/9; disconnect it using the Seeed board's J3 SD-enable pads before
sharing these pins with the display. The four camera and two PDM-microphone
GPIOs remain reserved for the built-in Sense peripherals.

The signal names follow common breakouts. Confirm the arriving display board's
labels and onboard pullups/resistors before soldering; if CS cannot be strapped
low or RESET/backlight need independent GPIO control, this 11-pin allocation
must be revisited rather than silently reusing a pin.

## Agent interaction contract (draft)

The agent receives a camera frame and the user's recognized text, then returns
one strict JSON action per interaction. The proposed fields are `action`,
`params`, `reasoning`, `speak_human`, `imotion`, `intent`, and `write_skill`.
The action set is `move_forward`, `move_backward`, `turn_left`, `turn_right`,
`stop`, `idle`, and `speak`.

Translation actions use `distance_m` and `speed` (proposed ranges 0.05-2.0 m
and 0.1-0.4). Turns use `angle_deg` (5-180 degrees, default 90). Since the MVP
has no motion sensors, firmware approximates translation and rotation from
calibrated motor runtime. The system prompt should describe this as an estimate,
not sensor-confirmed travel.

For multi-step tasks, `intent` is an LLM-authored plan and working context.
The LLM is the authority that decides which step is current, marks steps
complete, and updates or clears the plan; firmware must not advance intent
steps algorithmically. The firmware carries the latest intent and previous
action result into the next agent request so the LLM can make that decision.
The JSON schema, validation limits, safety behavior, and prompt wording remain
to be finalized before implementing the response parser.

### Working-prompt context example

The request can combine elapsed time, conversation history, the LLM-maintained
intent, previous action outcome, a camera image, and the latest STT text.
Timestamped sensor readings are omitted from the sensorless MVP. This is an
illustrative layout, not a finalized prompt template:

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

ОБРАТНАЯ СВЯЗЬ:
  датчики движения: отсутствуют
  предыдущее действие выполнено по калиброванному времени; фактическое расстояние не измерено

ПРЕДЫДУЩЕЕ ДЕЙСТВИЕ:
turn_right(angle_deg=30) [возраст: 1.5с]

[КАДР С КАМЕРЫ]
<base64 JPEG, QVGA>

ГОЛОС ЧЕЛОВЕКА (STT):
<пусто>
```

The prompt must not invent sensor readings. The history window, action outcome
format, image transport, and empty-STT encoding remain to be specified. The
firmware reports only what it actually observes; the LLM interprets that
context and owns intent progression.

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

### Display emotion engine contract

The display uses a procedural, time-driven face renderer, not GIF playback or
an image/frame stream from the LLM. The LLM selects an emotion identifier;
firmware sends that state to the renderer, which generates eye geometry,
transitions, blinking, gaze motion, and applicable overlays locally.

The current HTML prototype defines these 16 renderer emotion keys:
`neutral`, `happy`, `sad`, `surprised`, `angry`, `sleep`, `scared`,
`confused`, `wink`, `curious`, `warm`, `alert`, `awe`, `proud`, `monocle`,
and `idea`. These are renderer states, not GIF filenames or image assets.
The prototype also demonstrates animated behaviors such as blink-mediated
emotion changes, immediate wink, pupil saccades, breathing, jitter, tears,
sparkles, sleep Zs, monocle, idea bulb, and text overlay.

The current agent-prompt draft returns `imotion` as one emoji from a smaller
10-emoji set. Define an explicit, validated mapping from those response values
to renderer keys before implementing the parser and display interface.
Unknown values should fall back to a documented neutral state; they must not
be treated as filenames, URLs, or animation payloads. The HTML is a visual
prototype/reference for a later native C renderer port, not firmware runtime
code.

## Decisions pending before implementation

- Define configuration and persistent-data formats and size limits.
- Choose storage for mutable robot data and verify the required ESP-IDF
  filesystem component.
- Confirm flash size, OTA requirements, and a non-overlapping partition layout.
  The existing `partitions/v2/8m.csv` must not be changed until these are known.
- Calibrate forward/reverse speed settings and turn rates on the target robot,
  and define how calibration is performed and stored.
- Decide where robot MCP tools are registered in the application lifecycle.

This is a planning scaffold only. The new board and modules are not yet wired
into `main/CMakeLists.txt`, Kconfig, or the build script.
