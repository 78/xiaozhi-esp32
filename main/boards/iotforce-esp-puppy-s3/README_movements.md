# Puppy S3 Movement Documentation

## 1. Fix Explanation (Giải thích sửa lỗi)

The issue where servos stopped moving after the initial startup was caused by improper state management of the servos. Here is what was fixed:

1.  **Auto-Wakeup**: The robot now automatically "wakes up" (attaches servos) whenever a movement command is received. Previously, if the robot was in a "Rest" state (servos detached to save power), movement commands would update internal variables but not physically move the detached servos.
2.  **Proper Detach/Attach**:
    - `Oscillator::Detach()` now correctly stops the PWM signal (`ledc_stop`), preventing signal conflicts.
    - `Oscillator::Attach()` now immediately writes the last known position to the servo, preventing "limp" or undefined behavior when re-enabling.
3.  **State Synchronization**: `Puppy::SetRestState` now handles the physical attach/detach logic, ensuring the software state (`is_puppy_resting_`) matches the hardware state.
4.  **Non-Blocking Timing**: Replaced "busy-wait" loops with `vTaskDelay`. This allows the ESP32 to handle other tasks (WiFi, Audio, Web Server) smoothly while the robot is moving.

## 2. Control Flow (Luồng điều khiển)

The control flow moves from the high-level MCP tool (or button press) down to the hardware PWM signal.

1.  **Input Source**:
    - **MCP Tool**: `self.dog.basic_control` (e.g., "forward", "happy") or `self.dog.tail_control`.
    - **Startup**: `StartupAnimation()` task.
2.  **Queue**: Commands are sent to `puppy_queue_` (struct `OttoCommand`).
3.  **Task**: `PuppyTask` (running in `EspPuppyS3::EnablePuppy`) reads from the queue.
4.  **Puppy Class** (`Puppy`): High-level movement logic (e.g., `Walk`, `Turn`).
    - Calculates phases, amplitudes, and offsets for each leg.
    - Calls `OscillateServos` or `MoveServos`.
5.  **Oscillator Class** (`Oscillator`): Low-level servo control.
    - Generates sinusoidal waves or linear movements.
    - Converts angles to PWM duty cycles.
6.  **Hardware**: ESP32 LEDC (PWM) driver sends signals to the servos.

## 3. Control Functions (Các hàm điều khiển)

These functions are available in the `Puppy` class (`puppy_movements.h`).

### Basic Movements

- **`Walk(float steps, int period, int dir)`**:
  - `steps`: Number of cycles to walk (e.g., 4).
  - `period`: Duration of one step in ms (lower is faster, e.g., 1000).
  - `dir`: `FORWARD` (1) or `BACKWARD` (-1).
- **`Turn(float steps, int period, int dir)`**:
  - `dir`: `LEFT` (1) or `RIGHT` (-1).
- **`Home()`**:
  - Moves all servos to the center (0 degrees) and then enters "Rest" state (detaches servos).

### Actions

- **`Happy()`**: Crouches and jumps up 3 times, then wags tail.
- **`Shake()`**: Shakes the body left and right.
- **`WagTail(int period, int amplitude)`**: Wags the tail servo.
- **`Sit()`**: Moves legs to a sitting position.
- **`Jump(float steps, int period)`**: Makes the robot jump (experimental).

### Low-Level Control

- **`MoveSingle(int position, int servo_number)`**: Moves a specific servo to an angle (-90 to 90).
  - `servo_number`: `FL_LEG` (0), `FR_LEG` (1), `BL_LEG` (2), `BR_LEG` (3), `TAIL` (4).
- **`MoveServos(int time, int servo_target[])`**: Moves all servos to specific target positions over a duration `time`.

## 4. Servo Mapping

| Index | Name   | GPIO | Description     |
| ----- | ------ | ---- | --------------- |
| 0     | FL_LEG | 17   | Front Left Leg  |
| 1     | FR_LEG | 18   | Front Right Leg |
| 2     | BL_LEG | 39   | Back Left Leg   |
| 3     | BR_LEG | 38   | Back Right Leg  |
| 4     | TAIL   | 12   | Tail            |
