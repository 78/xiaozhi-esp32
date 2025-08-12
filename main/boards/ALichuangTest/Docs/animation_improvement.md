# Animation Display System Improvement Plan

## 1. Improvement Goals

### Current Issues
- Animation expressions are displayed as Canvas overlay on top of existing UI, consuming double display resources
- Cannot choose between traditional UI or animation expression system at compile time
- Bottom layer UI elements (status bar, chat interface, etc.) consume resources even when not visible
- Current implementation doesn't align with the animation.md design document

### Improvement Objectives
1. Provide configuration option in menuconfig to allow users to choose display mode
2. Pure animation mode: Display only animation expressions without bottom UI, saving memory and CPU resources
3. Traditional UI mode: Keep original status bar, chat interface and other UI elements
4. Align with animation.md design principles (but without complex LVGL layering for now)
5. Maintain code maintainability and extensibility

## 2. Implementation Steps

### Step 1: Add Kconfig Configuration Options
**File**: `main/boards/ALichuangTest/Kconfig`

Create Kconfig menu for ALichuangTest display configuration:
- ALICHUANG_USE_ANIMATION_DISPLAY: Enable/disable animation display mode
- ALICHUANG_ANIMATION_FRAME_RATE: Configure animation frame rate (default 30fps)
- ALICHUANG_ANIMATION_CACHE_SIZE: Configure animation frame cache size

### Step 2: Refactor Animation Display Implementation

#### 2.1 Simplify animation.h/cc for Current Phase
**Files**: `main/boards/ALichuangTest/skills/animation.h/cc`

The new AnimaDisplay class will:
- Inherit from Display base class directly (not LcdDisplay)
- Implement pure animation mode without traditional UI elements
- Support full-screen animation sequences based on emotions
- Use predefined sprite sequences (as per animation.md concept)
- NOT implement complex LVGL layering (eyes, mouth, etc.) in this phase

Key features:
```cpp
class AnimaDisplay : public Display {
private:
    // Animation state management
    std::string current_emotion_;
    int current_frame_index_;
    
    // Direct screen buffer for animation
    uint8_t* frame_buffer_;
    
    // Animation sequences mapping
    std::map<std::string, AnimationSequence> animations_;
    
public:
    // Core animation methods
    void PlayAnimation(animation_id_t id);
    void SetEmotion(const char* emotion) override;
    void UpdateFrame();  // Called by animation task
    
    // Simplified display without UI chrome
    void ShowMessage(const char* message) override;  // Optional subtitle overlay
};
```

#### 2.2 Keep Traditional UI Mode Separate
**Files**: `main/display/lcd_display.h/cc`

Keep the existing LcdDisplay class for traditional UI mode:
- Status bar, chat interface, etc.
- Used when ALICHUANG_USE_ANIMATION_DISPLAY is disabled

### Step 3: Animation Sequence Management

#### 3.1 Define Animation IDs (Simplified Version)
Based on animation.md, implement a subset of animations for initial version:
```cpp
typedef enum {
    // Idle animations
    ANIMATION_IDLE_BREATHING_BLINK,
    ANIMATION_IDLE_LOOK_AROUND,
    
    // Emotion reactions
    ANIMATION_HAPPY_WIGGLE,
    ANIMATION_ANNOYED_GLARE,
    ANIMATION_SURPRISED_WIDE_EYES,
    ANIMATION_SAD_EYES,
    
    // Physical reactions
    ANIMATION_DIZZY_SPIN_EYES,
    
    ANIMATION_COUNT
} animation_id_t;
```

#### 3.2 Animation Frame Data Structure
```cpp
struct AnimationFrame {
    const uint8_t* image_data;  // Pointer to image data
    uint16_t duration_ms;        // Frame duration
};

struct AnimationSequence {
    const AnimationFrame* frames;
    size_t frame_count;
    bool loop;  // Whether animation loops
};
```

### Step 4: Modify ALichuangTest Board Code

#### 4.1 Conditional Compilation for Display Type
**File**: `main/boards/ALichuangTest/ALichuangTest.cc`

```cpp
class ALichuangTest : public WifiBoard {
private:
#ifdef CONFIG_ALICHUANG_USE_ANIMATION_DISPLAY
    AnimaDisplay* display_ = nullptr;  // Pure animation display
#else
    LcdDisplay* display_ = nullptr;    // Traditional UI display
#endif

public:
    Display* GetDisplay() override {
#ifdef CONFIG_ALICHUANG_USE_ANIMATION_DISPLAY
        if (!display_) {
            display_ = new AnimaDisplay(...);
            // Setup emotion callback
            display_->SetEmotionCallback([this](const std::string& emotion) {
                HandleEmotionChange(emotion);
            });
        }
#else
        // Traditional UI mode with status bar, chat, etc.
        if (!display_) {
            display_ = new LcdDisplay(...);
        }
#endif
        return display_;
    }
};
```

#### 4.2 Simplify Animation Task
Replace ImageSlideshowTask with a cleaner animation update task:
```cpp
static void AnimationUpdateTask(void* arg) {
    ALichuangTest* board = static_cast<ALichuangTest*>(arg);
    AnimaDisplay* display = static_cast<AnimaDisplay*>(board->GetDisplay());
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frame_period = pdMS_TO_TICKS(1000 / CONFIG_ALICHUANG_ANIMATION_FRAME_RATE);
    
    while (true) {
        display->UpdateFrame();
        vTaskDelayUntil(&last_wake_time, frame_period);
    }
}
```

### Step 5: Optimize Resource Management

#### 5.1 Memory Optimization
- Use double buffering instead of Canvas in pure animation mode
- Prioritize PSRAM usage for frame buffers
- Implement memory pool for animation frame cache

#### 5.2 CPU Optimization
- Remove unnecessary UI update tasks
- Simplify LVGL refresh logic
- Reduce layer composition overhead

### Step 6: Update CMakeLists.txt
Conditionally compile source files based on configuration

## 3. Test Plan

### 3.1 Functional Testing
- Menuconfig option display and save correctly
- Pure animation mode: Only animation displayed, no UI elements
- Traditional UI mode: Complete UI interface displayed
- Normal compilation after mode switch

### 3.2 Performance Testing
- Memory usage comparison (monitor with heap_caps_get_free_size())
- CPU usage comparison (monitor with vTaskGetRunTimeStats())
- Frame rate stability test

### 3.3 Compatibility Testing
- Configuration persistence after OTA upgrade
- Smooth emotion state transitions
- Animation synchronization during audio playback

## 4. Implementation Timeline

| Phase | Task | Estimated Time |
|-------|------|----------------|
| Phase 1 | Kconfig configuration and basic framework | 2 hours |
| Phase 2 | PureAnimationDisplay implementation | 4 hours |
| Phase 3 | Board code adaptation | 2 hours |
| Phase 4 | Testing and optimization | 3 hours |
| Phase 5 | Documentation update | 1 hour |

## 5. Risks and Considerations

### Risk Points
1. **LVGL Version Compatibility**: Ensure new display mode is compatible with existing LVGL version
2. **Memory Fragmentation**: Frequent image switching may cause memory fragmentation
3. **Configuration Migration**: Compatibility for existing users after upgrade

### Solutions
1. Use stable LVGL APIs, avoid experimental features
2. Implement memory pool management for animation frame cache
3. Provide configuration migration script or default value handling

## 6. Expected Results

### Resource Savings Estimate
- **Memory Savings**: ~30-40% (removing UI elements and Canvas layer)
- **CPU Savings**: ~20-25% (reducing layer composition)
- **Code Size**: Reduce ~15KB through selective compilation

### User Experience Improvement
- Smoother animation playback
- Faster startup speed
- Longer battery life (for battery-powered devices)

## 7. Future Optimization Directions

1. **Animation Compression**: Implement simple inter-frame compression algorithm
2. **Dynamic Loading**: Load animation resources dynamically from Flash
3. **Custom Animations**: Support user-uploaded custom emoticons
4. **Hardware Acceleration**: Utilize ESP32's 2D acceleration features

## 8. File List

Files modified/created:
- [x] `main/Kconfig.projbuild` - Added ALichuangTest display configuration menu
- [x] `main/boards/ALichuangTest/skills/animation.h` - New pure animation display class with conditional compilation
- [x] `main/boards/ALichuangTest/skills/animation.cc` - Animation system implementation with conditional compilation
- [x] `main/boards/ALichuangTest/ALichuangTest.cc` - Added conditional compilation for display modes
- [x] `partitions/v1/16m_large_app.csv` - New partition table with larger app partitions

## 9. Menuconfig Configuration Steps

### Step 1: Enter ESP-IDF Configuration
```bash
idf.py menuconfig
```

### Step 2: Navigate to Board Configuration
1. Enter **"Xiaozhi Assistant"** menu
2. Select **"Board Type"**
3. Choose **"ALichuangTest"**

### Step 3: Configure Display Mode
After selecting ALichuangTest as board type, the **"ALichuangTest Display Configuration"** menu will appear:

1. **Use Animation Display Mode** (default: enabled)
   - When enabled: Pure animation mode without traditional UI
   - When disabled: Traditional UI with status bar and chat interface
   
2. **Animation Frame Rate** (if animation mode enabled)
   - Range: 10-60 FPS
   - Default: 30 FPS
   - Higher values = smoother animation but more CPU usage
   
3. **Animation Frame Cache Size** (if animation mode enabled)
   - Range: 1-5 frames
   - Default: 2 frames
   - Larger cache = smoother playback but more memory usage
   
4. **Use PSRAM for Animation Buffers** (if animation mode enabled and PSRAM available)
   - Default: enabled
   - Saves internal RAM but may be slightly slower

### Step 4: Configure Partition Table (if needed)
If you encounter "app partition too small" error:

1. Navigate to **"Partition Table"** menu
2. Select **"Custom partition table CSV"**
3. Change **"Custom partition CSV file"** to: `partitions/v1/16m_large_app.csv`
4. Save and exit menuconfig

### Step 5: Build and Flash
```bash
# Clean build (recommended when changing display modes)
idf.py fullclean

# Build for ALichuangTest
python scripts/release.py ALichuangTest

# Or standard build and flash
idf.py build
idf.py -p COM_PORT flash monitor
```

## 10. Troubleshooting

### Common Issues and Solutions

#### 1. "All app partitions are too small" Error
**Problem**: Binary size exceeds 6MB limit
**Solution**: 
- Option A: Disable animation mode in menuconfig to reduce binary size
- Option B: Use the larger partition table (`16m_large_app.csv`)
- Option C: Enable compiler optimizations (set to `-Os` for size)

#### 2. Watchdog Timer Triggered
**Problem**: LVGL operations cause watchdog timeout
**Solution**: Already fixed - all LVGL operations are now protected with locks

#### 3. Display Not Showing
**Problem**: Screen remains black in animation mode
**Solution**: 
- Check if LVGL is properly initialized
- Verify display hardware connections
- Ensure proper Kconfig settings are applied

#### 4. Compilation Errors with AnimaDisplay
**Problem**: AnimaDisplay class not found
**Solution**: 
- Ensure `CONFIG_ALICHUANG_USE_ANIMATION_DISPLAY` is enabled
- Run `idf.py fullclean` and rebuild

## 11. Migration Notes

### From Current Implementation to New System
1. The new `animation.h/cc` will completely replace the current Canvas-based implementation
2. Animation sequences will be based on the design principles from `animation.md` but simplified:
   - Full-screen sprite animations instead of LVGL layered components
   - Direct emotion-to-animation mapping
   - No complex facial feature composition in this phase
3. Future versions can add LVGL layering (eyes, mouth, etc.) as described in `animation.md`

### Backward Compatibility
- Traditional UI mode ensures existing users can maintain current experience
- Configuration option allows gradual migration
- OTA updates will default to traditional mode unless explicitly configured