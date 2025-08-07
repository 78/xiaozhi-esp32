name: "Event-Driven Physical Interaction Framework for XiaoZhi ESP32"
description: |

## Purpose
Enhance the XiaoZhi ESP32 AI voice chatbot with an event-driven physical interaction framework that enables immediate local responses and intelligent cloud reporting. This adds "life-like" interactive behaviors through sensor-based event detection and multi-modal feedback.

## Core Principles
1. **Context is King**: Include ALL necessary ESP-IDF patterns, XiaoZhi architecture knowledge, and integration points
2. **Validation Loops**: Provide executable tests and build commands the AI can run and fix
3. **Information Dense**: Use keywords and patterns from the existing XiaoZhi codebase
4. **Progressive Success**: Start with LED verification, validate, then enhance with full feedback
5. **Minimal Intrusion**: New functionality as independent component, minimal changes to existing code

---

## Goal
Create an event-driven physical interaction system for the LiChuang S3 development board where the device can:
- Stably read data from the QMI8658 6-axis IMU sensor via I2C
- Detect 4 key physical interaction events: pickup, putdown_hard, flip, shake
- Provide immediate LED feedback using the built-in GPIO_NUM_48 LED
- Log detected event IDs to serial console for verification
- Serve as foundation for expanding to other boards and more complex interactions

## Why
- **User Experience**: Transform device from voice-only to multi-modal interaction
- **Responsiveness**: Local reactions eliminate network latency for physical interactions  
- **Context Awareness**: Physical events provide crucial context for LLM decision making
- **Extensibility**: Framework enables future sensor integrations and behavior expansions

## What
An ESP-IDF component (`event_engine`) that:
- Loads event configurations from SPIFFS/LittleFS with fallback defaults
- Continuously processes sensor data to detect configured events
- Manages prioritized action queue for local feedback execution
- Integrates with existing MQTT/WebSocket protocols for cloud reporting
- Works within XiaoZhi's single-threaded event loop architecture

### Success Criteria
- [ ] QMI8658 sensor successfully initialized and provides stable accelerometer/gyroscope readings
- [ ] Four motion events reliably detected: pickup, putdown_hard, flip, shake
- [ ] Built-in LED (GPIO_NUM_48) responds with distinct patterns for each event
- [ ] Serial console logs show clear event detection with timestamps
- [ ] Event detection works consistently across different motion speeds and orientations
- [ ] Memory usage remains under 100KB additional heap usage
- [ ] Build system integrates cleanly with existing lichuang-dev board configuration

## All Needed Context

### Documentation & References
```yaml
# MUST READ - Include these in your context window
- file: main/application.cc
  why: Core application singleton pattern and event loop
  
- file: main/boards/lichuang-dev/lichuang_dev_board.cc
  why: Target board implementation with I2C configuration
  
- file: main/boards/lichuang-dev/config.h
  why: GPIO pins, I2C configuration, LED setup for target hardware
  
- file: main/boards/common/i2c_device.h
  why: I2C device base class pattern used throughout project
  
- file: main/led/gpio_led.cc
  why: LED control pattern for GPIO_NUM_48 feedback
  
- file: main/audio/codecs/es8311_audio_codec.cc
  why: Example I2C device implementation pattern

- url: https://datasheet.lcsc.com/szlcsc/2109011930_QST-QMI8658_C2844981.pdf
  why: QMI8658 datasheet for register definitions and communication protocol
  
- url: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/i2c.html
  why: ESP-IDF I2C driver documentation
  
- url: https://github.com/QSTCorp/QMI8658-Driver-C/blob/main/qmi8658.c
  why: Official QMI8658 C driver reference for initialization sequence
```

### Current LiChuang Dev Board Structure
```bash
xiaozhi-esp32/
├── main/
│   ├── application.cc/h          # Core singleton application
│   ├── boards/lichuang-dev/      # Target board configuration
│   │   ├── config.h              # I2C pins: SDA=GPIO_1, SCL=GPIO_2, LED=GPIO_48
│   │   ├── config.json           # ESP32-S3 build configuration
│   │   └── lichuang_dev_board.cc # Board implementation with I2C bus setup
│   ├── boards/common/            # Shared hardware drivers
│   │   ├── i2c_device.h/cc       # I2C device base class (400kHz, 100ms timeout)
│   │   └── pca9557.h/cc          # GPIO expander (existing I2C device)
│   ├── led/                      # LED control system
│   │   ├── led.h                 # LED base interface
│   │   ├── gpio_led.cc           # GPIO LED implementation
│   │   └── single_led.cc         # Single LED wrapper
│   ├── audio/codecs/             # I2C device examples
│   │   └── es8311_audio_codec.cc # I2C pattern reference (0x18 address)
│   └── CMakeLists.txt           # Build configuration
└── scripts/
    └── release.py               # Build: python scripts/release.py lichuang-dev
```

### Target Implementation Structure (LiChuang Dev Board Focus)
```bash
xiaozhi-esp32/
├── main/
│   ├── sensors/                  # NEW: Sensor subsystem
│   │   ├── sensor.h             # Base sensor interface
│   │   ├── imu_sensor.h         # IMU sensor base class
│   │   └── motion_detector.cc/h # Motion event detection logic
│   │
│   ├── boards/common/           # ADD QMI8658 driver
│   │   └── qmi8658.cc/h         # QMI8658 6-axis IMU driver (I2C address 0x6B)
│   │
│   ├── boards/lichuang-dev/     # MODIFY target board
│   │   ├── config.h             # ADD: QMI8658_ADDR, motion detection thresholds
│   │   └── lichuang_dev_board.cc # ADD: GetImuSensor() method
│   │
│   ├── motion_engine.cc/h       # NEW: Core motion event system
│   ├── led_feedback.cc/h        # NEW: LED pattern controller for events
│   │
│   ├── application.cc           # MODIFIED: Add motion_engine initialization
│   └── CMakeLists.txt          # MODIFIED: Add sensor and motion sources
│
└── Test Validation:             # Testing approach
    ├── Serial Monitor           # Event logs: "Motion detected: pickup"
    ├── LED Patterns            # Visual feedback for each event type
    └── Physical Testing        # Manual motion tests
```

### Known Gotchas & LiChuang Dev Board Specifics
```cpp
// CRITICAL: XiaoZhi uses single-threaded event loop - no blocking in callbacks
// CRITICAL: Use Application::Schedule() for deferred execution
// CRITICAL: I2C bus shared with audio codec (ES8311), camera, PCA9557 GPIO expander
// CRITICAL: QMI8658 default I2C address is 0x6B (can be 0x6A with address pin)
// CRITICAL: Built-in LED on GPIO_NUM_48 - active HIGH logic
// CRITICAL: I2C bus runs at 400kHz - QMI8658 supports up to 400kHz
// CRITICAL: QMI8658 requires proper power-on sequence: 15ms delay after VDD
// CRITICAL: Motion detection algorithms must avoid false positives from audio vibrations
// CRITICAL: Use ESP_LOG macros with "motion" tag for debugging
// CRITICAL: Initialize IMU after I2C bus is configured by board initialization
```

## Implementation Blueprint

### Data Models and Structures

```cpp
// main/sensors/motion_detector.h - Motion event definitions
namespace motion {

enum class MotionEvent {
    PICKUP,        // Device lifted from surface
    PUTDOWN_HARD,  // Device placed down with impact > 2g
    FLIP,          // Device rotated 180 degrees quickly
    SHAKE          // Rapid back-and-forth motion detected
};

enum class LedPattern {
    SOLID_CYAN,     // Solid color for pickup
    RED_FLASH,      // Quick flash for putdown_hard  
    BLUE_SWEEP,     // Sweep pattern for flip
    YELLOW_BLINK    // Fast blink for shake
};

struct ImuReading {
    float accel_x, accel_y, accel_z;  // in g (±8g range)
    float gyro_x, gyro_y, gyro_z;     // in deg/s (±2000 dps range)
    uint64_t timestamp_us;            // microsecond timestamp
};

struct MotionTrigger {
    float accel_threshold;            // Acceleration threshold in g
    float gyro_threshold;             // Gyroscope threshold in deg/s
    uint32_t min_duration_ms;         // Minimum event duration
    uint32_t debounce_ms;            // Debounce time between events
};

// Event detection thresholds for QMI8658
struct MotionThresholds {
    static constexpr MotionTrigger PICKUP = {
        .accel_threshold = 1.5f,      // 1.5g upward acceleration
        .gyro_threshold = 50.0f,      // 50 deg/s rotation
        .min_duration_ms = 100,       // 100ms minimum
        .debounce_ms = 500            // 500ms between pickups
    };
    
    static constexpr MotionTrigger PUTDOWN_HARD = {
        .accel_threshold = 2.0f,      // 2g impact
        .gyro_threshold = 30.0f,      // 30 deg/s settling
        .min_duration_ms = 50,        // 50ms impact
        .debounce_ms = 300            // 300ms between impacts
    };
    
    static constexpr MotionTrigger FLIP = {
        .accel_threshold = 1.0f,      // 1g during rotation
        .gyro_threshold = 200.0f,     // 200 deg/s rotation
        .min_duration_ms = 200,       // 200ms rotation
        .debounce_ms = 1000           // 1s between flips
    };
    
    static constexpr MotionTrigger SHAKE = {
        .accel_threshold = 2.5f,      // 2.5g shake intensity
        .gyro_threshold = 100.0f,     // 100 deg/s shake
        .min_duration_ms = 300,       // 300ms shake duration
        .debounce_ms = 500            // 500ms between shakes
    };
};

} // namespace motion
```

### Task List (LiChuang Dev Board Focused)

```yaml
Task 1: Create QMI8658 IMU Driver
CREATE main/boards/common/qmi8658.cc/h:
  - PATTERN: Follow es8311_audio_codec.cc I2C device pattern
  - Inherit from I2cDevice base class (address 0x6B)
  - Implement QMI8658 initialization sequence (15ms power-on delay)
  - Methods: Initialize(), ReadAcceleration(), ReadGyroscope()
  - Handle ±8g accel range, ±2000 dps gyro range

Task 2: Create Sensor Base Classes
CREATE main/sensors/sensor.h:
  - PATTERN: Follow audio_codec.h abstraction
  - Virtual base class for all sensors
  - Methods: Initialize(), GetName(), IsPresent()

CREATE main/sensors/imu_sensor.h:
  - Inherit from Sensor base class
  - Define ImuReading struct for accel/gyro data
  - Virtual methods for IMU-specific operations

Task 3: Implement Motion Detection Logic
CREATE main/sensors/motion_detector.cc/h:
  - PATTERN: Process sensor data in Application main loop
  - Implement 4 motion algorithms: pickup, putdown_hard, flip, shake
  - Use MotionThresholds constants for detection parameters
  - Use Application::Schedule() for event callbacks
  - Add ESP_LOG statements for debugging

Task 4: Create LED Feedback System
CREATE main/led_feedback.cc/h:
  - PATTERN: Use board->GetLed() to access GPIO_NUM_48 LED
  - Implement 4 distinct LED patterns for each motion event
  - Non-blocking LED patterns using timers
  - Methods: ShowPickup(), ShowPutdownHard(), ShowFlip(), ShowShake()

Task 5: Core Motion Engine
CREATE main/motion_engine.cc/h:
  - PATTERN: Similar to audio_service.cc coordination
  - Initialize QMI8658 sensor via board I2C configuration
  - Create MotionDetector and LedFeedback instances
  - Coordinate sensor reading, detection, and LED feedback
  - Update() method called from Application main loop

Task 6: Board Integration
MODIFY main/boards/lichuang-dev/config.h:
  - ADD: #define QMI8658_I2C_ADDR 0x6B
  - ADD: Motion detection enable flag
  - ADD: Debug logging level for motion events

MODIFY main/boards/lichuang-dev/lichuang_dev_board.cc:
  - ADD: GetImuSensor() method returning QMI8658 instance
  - Ensure I2C bus initialization includes QMI8658
  - ADD: LED configuration for motion feedback

Task 7: Application Integration
MODIFY main/application.cc:
  - ADD: #include "motion_engine.h"
  - ADD: std::unique_ptr<MotionEngine> motion_engine_ member
  - Initialize motion_engine_ in InitSystem() after board initialization
  - Call motion_engine_->Update() in Run() main loop
  - Handle initialization failure gracefully

MODIFY main/CMakeLists.txt:
  - ADD sensor subsystem sources to build
  - ADD motion engine and LED feedback sources
  - ADD QMI8658 driver to board common sources

Task 8: Testing and Debugging
BUILD and FLASH:
  - Command: python scripts/release.py lichuang-dev
  - Flash to device and monitor serial output
  - Expected: "QMI8658 initialized successfully"

PHYSICAL TESTING:
  - Test pickup motion: expect cyan LED + "Motion: pickup" log
  - Test putdown_hard: expect red flash + "Motion: putdown_hard" log  
  - Test flip motion: expect blue sweep + "Motion: flip" log
  - Test shake motion: expect yellow blink + "Motion: shake" log

FINE-TUNING:
  - Adjust MotionThresholds values based on test results
  - Optimize detection algorithms to avoid false positives
  - Ensure LED patterns are visually distinct
```

### Per-Task Implementation Details

```cpp
// Task 1: QMI8658 Driver Implementation (following I2cDevice pattern)
// main/boards/common/qmi8658.h
class Qmi8658 : public I2cDevice {
private:
    static constexpr uint8_t DEFAULT_ADDR = 0x6B;
    static constexpr uint8_t WHO_AM_I = 0x00;
    static constexpr uint8_t RESET = 0x60;
    static constexpr uint8_t CTRL1 = 0x02;
    static constexpr uint8_t CTRL2 = 0x03;
    static constexpr uint8_t ACCEL_X_L = 0x35;
    static constexpr uint8_t GYRO_X_L = 0x3B;
    
public:
    Qmi8658(i2c_master_bus_handle_t bus_handle) 
        : I2cDevice(bus_handle, DEFAULT_ADDR) {}
    
    esp_err_t Initialize() override {
        // PATTERN: Similar to Es8311AudioCodec initialization
        ESP_RETURN_ON_ERROR(I2cDevice::Initialize(), TAG, "I2C init failed");
        
        // Check WHO_AM_I register (should return 0x05)
        uint8_t who_am_i;
        ESP_RETURN_ON_ERROR(ReadReg(WHO_AM_I, who_am_i), TAG, "Read WHO_AM_I failed");
        if (who_am_i != 0x05) {
            ESP_LOGE(TAG, "QMI8658 not found, WHO_AM_I=0x%02X", who_am_i);
            return ESP_ERR_NOT_FOUND;
        }
        
        // Software reset and wait 15ms
        ESP_RETURN_ON_ERROR(WriteReg(RESET, 0xB0), TAG, "Reset failed");
        vTaskDelay(pdMS_TO_TICKS(15));
        
        // Configure accelerometer: ±8g, 125Hz
        ESP_RETURN_ON_ERROR(WriteReg(CTRL1, 0x40), TAG, "Accel config failed");
        
        // Configure gyroscope: ±2000dps, 125Hz  
        ESP_RETURN_ON_ERROR(WriteReg(CTRL2, 0x60), TAG, "Gyro config failed");
        
        ESP_LOGI(TAG, "QMI8658 initialized successfully");
        return ESP_OK;
    }
    
    esp_err_t ReadAcceleration(float& x, float& y, float& z) {
        uint8_t data[6];
        ESP_RETURN_ON_ERROR(ReadRegs(ACCEL_X_L, data, 6), TAG, "Read accel failed");
        
        // Convert to g (±8g range, 16-bit)
        int16_t raw_x = (data[1] << 8) | data[0];
        int16_t raw_y = (data[3] << 8) | data[2];
        int16_t raw_z = (data[5] << 8) | data[4];
        
        x = raw_x * 8.0f / 32768.0f;
        y = raw_y * 8.0f / 32768.0f;
        z = raw_z * 8.0f / 32768.0f;
        
        return ESP_OK;
    }
    
    esp_err_t ReadGyroscope(float& x, float& y, float& z) {
        uint8_t data[6];
        ESP_RETURN_ON_ERROR(ReadRegs(GYRO_X_L, data, 6), TAG, "Read gyro failed");
        
        // Convert to deg/s (±2000dps range, 16-bit)
        int16_t raw_x = (data[1] << 8) | data[0];
        int16_t raw_y = (data[3] << 8) | data[2];
        int16_t raw_z = (data[5] << 8) | data[4];
        
        x = raw_x * 2000.0f / 32768.0f;
        y = raw_y * 2000.0f / 32768.0f;
        z = raw_z * 2000.0f / 32768.0f;
        
        return ESP_OK;
    }
};

// Task 3: Motion Detection Algorithm
// main/sensors/motion_detector.cc
class MotionDetector {
private:
    Qmi8658* imu_;
    uint64_t last_pickup_time_ = 0;
    uint64_t last_putdown_time_ = 0;
    uint64_t last_flip_time_ = 0;
    uint64_t last_shake_time_ = 0;
    
public:
    MotionDetector(Qmi8658* imu) : imu_(imu) {}
    
    void Update() {
        // CRITICAL: Non-blocking, called from Application main loop
        float ax, ay, az, gx, gy, gz;
        
        if (imu_->ReadAcceleration(ax, ay, az) != ESP_OK ||
            imu_->ReadGyroscope(gx, gy, gz) != ESP_OK) {
            return;  // Skip this cycle if read fails
        }
        
        uint64_t now = esp_timer_get_time();
        
        // Check pickup: upward acceleration + rotation
        if (az > motion::MotionThresholds::PICKUP.accel_threshold &&
            (now - last_pickup_time_) > (motion::MotionThresholds::PICKUP.debounce_ms * 1000)) {
            
            Application::GetInstance().Schedule([this]() {
                OnMotionDetected(motion::MotionEvent::PICKUP);
            });
            last_pickup_time_ = now;
        }
        
        // Check putdown_hard: downward impact
        float magnitude = sqrt(ax*ax + ay*ay + az*az);
        if (magnitude > motion::MotionThresholds::PUTDOWN_HARD.accel_threshold &&
            (now - last_putdown_time_) > (motion::MotionThresholds::PUTDOWN_HARD.debounce_ms * 1000)) {
            
            Application::GetInstance().Schedule([this]() {
                OnMotionDetected(motion::MotionEvent::PUTDOWN_HARD);
            });
            last_putdown_time_ = now;
        }
        
        // Check flip: high rotation rate
        float gyro_magnitude = sqrt(gx*gx + gy*gy + gz*gz);
        if (gyro_magnitude > motion::MotionThresholds::FLIP.gyro_threshold &&
            (now - last_flip_time_) > (motion::MotionThresholds::FLIP.debounce_ms * 1000)) {
            
            Application::GetInstance().Schedule([this]() {
                OnMotionDetected(motion::MotionEvent::FLIP);
            });
            last_flip_time_ = now;
        }
        
        // Check shake: alternating acceleration
        if (magnitude > motion::MotionThresholds::SHAKE.accel_threshold &&
            gyro_magnitude > motion::MotionThresholds::SHAKE.gyro_threshold &&
            (now - last_shake_time_) > (motion::MotionThresholds::SHAKE.debounce_ms * 1000)) {
            
            Application::GetInstance().Schedule([this]() {
                OnMotionDetected(motion::MotionEvent::SHAKE);
            });
            last_shake_time_ = now;
        }
    }
    
private:
    void OnMotionDetected(motion::MotionEvent event) {
        const char* event_names[] = {"pickup", "putdown_hard", "flip", "shake"};
        ESP_LOGI("motion", "Motion detected: %s", event_names[static_cast<int>(event)]);
        
        // Trigger LED feedback
        // This will be implemented in LedFeedback class
    }
};

// Task 7: Application Integration Example
// main/application.cc modifications
class Application {
private:
    std::unique_ptr<MotionEngine> motion_engine_;  // ADD this member
    
public:
    void InitSystem() {
        // ... existing board, display, audio initialization ...
        
        // Initialize motion engine after board setup
        #ifdef HAS_MOTION_DETECTION
        motion_engine_ = std::make_unique<MotionEngine>();
        esp_err_t ret = motion_engine_->Initialize(board_.get());
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Motion engine disabled: %s", esp_err_to_name(ret));
            motion_engine_.reset();
        } else {
            ESP_LOGI(TAG, "Motion detection enabled");
        }
        #endif
    }
    
    void Run() {
        while (running_) {
            // ... existing system updates ...
            
            // Update motion detection
            if (motion_engine_) {
                motion_engine_->Update();
            }
            
            // ... handle scheduled callbacks, events, etc. ...
            vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz update rate
        }
    }
};

// Task 6: Board Integration Pattern
// main/boards/lichuang-dev/lichuang_dev_board.cc
class LichuangDevBoard : public WifiBoard {
private:
    std::unique_ptr<Qmi8658> qmi8658_;
    
public:
    LichuangDevBoard() : WifiBoard() {
        // Initialize QMI8658 with board's I2C bus
        qmi8658_ = std::make_unique<Qmi8658>(i2c_bus_);
    }
    
    Qmi8658* GetImuSensor() {
        return qmi8658_.get();
    }
    
    // Override LED to enable motion feedback
    Led* GetLed() override {
        if (!led_) {
            // Create GPIO LED for motion feedback
            led_ = std::make_unique<GpioLed>(static_cast<gpio_num_t>(BUILTIN_LED_GPIO));
        }
        return led_.get();
    }
};
```

### Integration Points (LiChuang Dev Board Specific)
```yaml
BUILD SYSTEM:
  - modify: main/CMakeLists.txt
  - changes: |
      # Add motion detection sources
      list(APPEND SRCS
          "motion_engine.cc"
          "led_feedback.cc"
          "sensors/motion_detector.cc"
      )
      
      # Add QMI8658 driver to board common
      list(APPEND BOARD_COMMON_SRCS
          "boards/common/qmi8658.cc"
      )
      
      # Conditional compilation for lichuang-dev board
      if(CONFIG_BOARD_TYPE_LICHUANG_DEV)
          add_compile_definitions(HAS_MOTION_DETECTION)
      endif()

BOARD CONFIGURATION:
  - file: main/boards/lichuang-dev/config.h
  - additions: |
      // QMI8658 IMU Configuration
      #define QMI8658_I2C_ADDR 0x6B
      #define HAS_MOTION_DETECTION
      
      // Motion Detection Parameters
      #define MOTION_UPDATE_RATE_HZ 100
      #define MOTION_DEBUG_ENABLED 1
      
      // LED Configuration for motion feedback
      #define MOTION_LED_GPIO BUILTIN_LED_GPIO  // GPIO_NUM_48

I2C BUS SHARING:
  - Current devices on I2C_NUM_1 (SDA=GPIO_1, SCL=GPIO_2):
      * PCA9557 GPIO expander (0x19)
      * ES8311 audio codec (0x18) 
      * ES7210 audio ADC (0x82)
      * FT5x06 touch controller (varies)
      * QMI8658 IMU (0x6B) <- NEW
  - Bus capacity: Good, no address conflicts
  - Bus speed: 400kHz (supported by all devices)

SERIAL DEBUG OUTPUT:
  - Motion events logged to console:
      I (12345) motion: QMI8658 initialized successfully
      I (23456) motion: Motion detected: pickup
      I (34567) motion: Motion detected: shake
  - Enable with: #define MOTION_DEBUG_ENABLED 1
  - Log level: ESP_LOG_INFO for motion events

LED FEEDBACK PATTERNS:
  - PICKUP: Solid cyan LED for 2 seconds
  - PUTDOWN_HARD: Red flash 3 times (200ms on/off)
  - FLIP: Blue sweep pattern (fade in/out)
  - SHAKE: Fast yellow blink (100ms on/off, 5 cycles)
  - Controlled via GPIO_NUM_48 (active HIGH)

TESTING COMMANDS:
  - Build: python scripts/release.py lichuang-dev
  - Flash: idf.py -p COMx flash monitor
  - Expected boot sequence:
      I (xxx) lichuang-dev: Board initialized
      I (xxx) motion: QMI8658 initialized successfully  
      I (xxx) application: Motion detection enabled
```

## Validation Loop (LiChuang Dev Board)

### Level 1: Build Verification
```bash
# Clean build for lichuang-dev board
python scripts/release.py lichuang-dev

# Expected: Successful compilation
# Check: QMI8658 and motion detection sources compiled
# Check: No I2C address conflicts or GPIO conflicts
# If errors: Verify CMakeLists.txt changes and header inclusions
```

### Level 2: Hardware Initialization
```bash
# Flash firmware to LiChuang S3 board
idf.py -p COMx flash monitor

# Expected boot log sequence:
I (123) lichuang-dev: Board initialization complete
I (234) motion: QMI8658 WHO_AM_I: 0x05
I (245) motion: QMI8658 initialized successfully
I (256) application: Motion detection enabled
I (267) application: System ready

# If IMU init fails:
# - Check I2C connections (SDA=GPIO_1, SCL=GPIO_2)
# - Verify QMI8658 is present on board
# - Check I2C address (should be 0x6B)
```

### Level 3: Sensor Data Validation
```bash
# Monitor IMU readings (if debug enabled)
# Expected continuous data stream:
D (1000) motion: Accel: x=0.12, y=-0.05, z=0.98 g
D (1100) motion: Gyro: x=1.2, y=-0.8, z=0.3 deg/s

# Test basic orientation:
# - Flat on table: az ≈ 1.0g, ax,ay ≈ 0g
# - Vertical: one axis ≈ ±1.0g
# - Gentle rotation: gyro values change

# If no data or incorrect readings:
# - Check QMI8658 register configuration
# - Verify I2C communication is working
# - Check sensor orientation and mounting
```

### Level 4: Motion Event Testing
```bash
# Physical motion tests (perform slowly and deliberately):

# Test 1: PICKUP Event
# Action: Lift device smoothly from table
# Expected: 
#   - LED turns solid cyan for 2 seconds
#   - Log: "I (xxxx) motion: Motion detected: pickup"

# Test 2: PUTDOWN_HARD Event  
# Action: Place device on table with firm impact
# Expected:
#   - LED flashes red 3 times
#   - Log: "I (xxxx) motion: Motion detected: putdown_hard"

# Test 3: FLIP Event
# Action: Rotate device 180 degrees quickly
# Expected:
#   - LED shows blue sweep pattern
#   - Log: "I (xxxx) motion: Motion detected: flip"

# Test 4: SHAKE Event
# Action: Shake device back and forth rapidly
# Expected:
#   - LED blinks yellow rapidly (5 cycles)
#   - Log: "I (xxxx) motion: Motion detected: shake"

# If events don't trigger:
# - Check MotionThresholds values in code
# - Increase debug logging to see sensor values
# - Adjust thresholds based on actual sensor readings
```

### Level 5: Performance and Stability
```bash
# Long-term stability test (run for 30+ minutes):
# - No memory leaks (check heap usage)
# - Consistent motion detection
# - No false positives from audio or vibrations
# - LED patterns remain distinct and reliable

# Performance metrics to monitor:
# - CPU usage: <5% additional load
# - Memory usage: <50KB additional heap
# - I2C bus utilization: No timeouts or errors
# - Motion detection latency: <100ms from motion to LED

# Expected system behavior:
# - Normal voice interaction unaffected
# - Audio quality not degraded by motion detection
# - No interference with other I2C devices
# - Reliable detection across different motion speeds
```

## Final Validation Checklist (LiChuang Dev Board)
- [ ] LiChuang-dev board firmware builds without errors
- [ ] QMI8658 WHO_AM_I register reads correctly (0x05)
- [ ] QMI8658 accelerometer provides stable readings (±8g range)
- [ ] QMI8658 gyroscope provides stable readings (±2000 dps range)
- [ ] All 4 motion events detected reliably: pickup, putdown_hard, flip, shake
- [ ] LED patterns are visually distinct for each motion event
- [ ] Serial console shows motion event logs with correct event names
- [ ] Motion detection latency is <100ms from physical motion to LED response
- [ ] No false positives from audio playback or vibrations
- [ ] I2C bus sharing works correctly with existing devices (no conflicts)
- [ ] Motion detection adds <50KB heap usage and <5% CPU load
- [ ] System remains stable during extended operation (30+ minutes)
- [ ] Normal XiaoZhi voice functionality unaffected by motion detection

---

## Anti-Patterns to Avoid (LiChuang Dev Board Specific)
- ❌ Don't block in motion detection Update() - use non-blocking I2C reads
- ❌ Don't ignore QMI8658 initialization errors - handle gracefully 
- ❌ Don't assume I2C address 0x6B - check WHO_AM_I register first
- ❌ Don't set motion thresholds too low - will cause false positives from audio
- ❌ Don't poll IMU faster than 100Hz - unnecessary CPU overhead
- ❌ Don't forget the 15ms power-on delay for QMI8658
- ❌ Don't use blocking vTaskDelay() in motion detection callbacks
- ❌ Don't hardcode GPIO_NUM_48 - use board config BUILTIN_LED_GPIO
- ❌ Don't interfere with existing I2C devices (ES8311, PCA9557)
- ❌ Don't create motion events without proper debouncing
- ❌ Don't ignore LED pattern timing - ensure patterns are visually distinct

## Confidence Score: 9/10

High confidence due to:
- Specific hardware target (LiChuang S3 + QMI8658)
- Deep analysis of existing board architecture
- Clear integration patterns from audio/display subsystems
- Well-defined, testable success criteria
- Focused scope (4 motion events + LED feedback)

Minor uncertainty on:
- Optimal motion detection thresholds (will need testing/tuning)
- QMI8658 sensor orientation on actual board (may affect axis mapping)