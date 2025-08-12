# SD Card Animation System Design

## 1. Overview

Instead of embedding animation images in firmware, we can store them on an SD card to significantly reduce firmware size and enable dynamic content updates.

## 2. Advantages

### Memory Benefits
- **Firmware Size Reduction**: ~2-3MB saved by removing embedded images
- **Unlimited Storage**: SD cards can store thousands of animation frames
- **Dynamic Loading**: Load only needed frames into RAM

### Flexibility Benefits
- **Hot Swapping**: Change animations without reflashing
- **User Customization**: Users can add their own animations
- **Multiple Animation Sets**: Store different themes/seasons

## 3. Hardware Requirements

### SD Card Interface Options

#### Option A: SPI Mode (4 wires)
- **Pros**: Uses fewer pins, simpler implementation
- **Cons**: Slower speed (~10-20 MB/s)
- **Required Pins**:
  - MOSI (Master Out)
  - MISO (Master In) 
  - SCLK (Clock)
  - CS (Chip Select)

#### Option B: SDMMC Mode (4-bit, 6 wires)
- **Pros**: Faster speed (~40 MB/s)
- **Cons**: Uses more pins
- **Required Pins**:
  - CMD
  - CLK
  - D0-D3 (4 data lines)

### Available GPIO Analysis for ALichuangTest
Based on current pin usage, potential free GPIOs for SD card:
- GPIO 10, 11 (check if available)
- GPIO 19-22 (if not used by other peripherals)
- GPIO 35-37 (input only, can be used for MISO/detect)

## 4. Software Implementation

### 4.1 SD Card Driver Integration
```cpp
class SDCardManager {
private:
    sdmmc_card_t* card_;
    bool mounted_ = false;
    
public:
    bool Mount();
    bool Unmount();
    bool LoadImage(const char* path, uint8_t* buffer, size_t size);
    bool CheckCard();
    std::vector<std::string> ListAnimations();
};
```

### 4.2 Animation File Structure
```
/sd_card/
├── animations/
│   ├── happy/
│   │   ├── frame_001.rgb565
│   │   ├── frame_002.rgb565
│   │   └── frame_003.rgb565
│   ├── sad/
│   │   ├── frame_001.rgb565
│   │   └── frame_002.rgb565
│   └── angry/
│       └── ...
├── config.json  # Animation metadata
└── themes/      # Different animation sets
```

### 4.3 Modified AnimaDisplay Class
```cpp
class AnimaDisplay : public Display {
private:
    SDCardManager* sd_card_;
    std::map<std::string, std::vector<std::string>> animation_files_;
    uint8_t* frame_cache_[2];  // Double buffering
    
public:
    bool LoadAnimationFromSD(const std::string& emotion);
    void PreloadNextFrame();  // Background loading
    void CacheAnimationMetadata();
};
```

### 4.4 Lazy Loading Strategy
```cpp
void AnimaDisplay::UpdateFrame() {
    // Load frame from SD only when needed
    if (NeedNewFrame()) {
        // Load in background task
        xTaskCreate(LoadFrameTask, ...);
    }
    
    // Display cached frame
    DisplayCachedFrame();
}
```

## 5. File Format Options

### Option 1: Raw RGB565
- **Pros**: No decoding needed, fast display
- **Cons**: Large file size (112KB per 240x240 frame)
- **Usage**: Direct memory copy to display buffer

### Option 2: Compressed (JPEG/PNG)
- **Pros**: Smaller file size (10-30KB per frame)
- **Cons**: Decoding overhead, needs more RAM
- **Library**: ESP32 JPEG decoder or libpng

### Option 3: Custom RLE Compression
- **Pros**: Balance between size and speed
- **Cons**: Need custom encoder/decoder
- **Benefit**: Optimized for animation sequences

## 6. Implementation Steps

### Phase 1: Basic SD Card Support
1. Add SD card hardware to ALichuangTest board
2. Implement SDCardManager class
3. Test basic file reading

### Phase 2: Animation Loading
1. Convert existing animations to files
2. Implement LoadAnimationFromSD()
3. Add frame caching logic

### Phase 3: Optimization
1. Implement background loading
2. Add compression support
3. Optimize read performance

## 7. Kconfig Options

```kconfig
menu "SD Card Animation Configuration"
    depends on ALICHUANG_USE_ANIMATION_DISPLAY

    config ALICHUANG_USE_SD_CARD
        bool "Load Animations from SD Card"
        default n
        help
            Load animation frames from SD card instead of firmware.
            Requires SD card hardware connection.

    choice ALICHUANG_SD_INTERFACE
        prompt "SD Card Interface"
        depends on ALICHUANG_USE_SD_CARD
        default ALICHUANG_SD_SPI
        
        config ALICHUANG_SD_SPI
            bool "SPI Mode"
        config ALICHUANG_SD_SDMMC
            bool "SDMMC Mode (4-bit)"
    endchoice

    config ALICHUANG_SD_SPI_MOSI
        int "SD Card SPI MOSI GPIO"
        depends on ALICHUANG_SD_SPI
        default 11
        
    config ALICHUANG_SD_SPI_MISO
        int "SD Card SPI MISO GPIO"
        depends on ALICHUANG_SD_SPI
        default 10
        
    config ALICHUANG_SD_SPI_CLK
        int "SD Card SPI CLK GPIO"
        depends on ALICHUANG_SD_SPI
        default 21
        
    config ALICHUANG_SD_SPI_CS
        int "SD Card SPI CS GPIO"
        depends on ALICHUANG_SD_SPI
        default 22

    config ALICHUANG_SD_CACHE_FRAMES
        int "Number of Frames to Cache"
        depends on ALICHUANG_USE_SD_CARD
        default 3
        range 2 10
        help
            Cache multiple frames in RAM for smooth playback.
endmenu
```

## 8. Performance Considerations

### Read Speed Requirements
- 30 FPS animation needs: 30 * 112KB = 3.36 MB/s
- SPI mode is sufficient for smooth playback
- Pre-loading next frame during current frame display

### Optimization Techniques
1. **Double Buffering**: Load next frame while displaying current
2. **Predictive Loading**: Pre-load based on emotion patterns
3. **LRU Cache**: Keep frequently used frames in RAM
4. **Compression**: Reduce file size for faster loading

## 9. Example Code

### Loading Animation from SD Card
```cpp
bool AnimaDisplay::LoadAnimationFromSD(const std::string& emotion) {
    std::string path = "/sdcard/animations/" + emotion + "/";
    
    // List all frames in directory
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open animation directory: %s", path.c_str());
        return false;
    }
    
    // Clear previous animation
    animation_files_[emotion].clear();
    
    // Collect frame files
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".rgb565")) {
            animation_files_[emotion].push_back(path + entry->d_name);
        }
    }
    closedir(dir);
    
    // Sort files by name
    std::sort(animation_files_[emotion].begin(), 
              animation_files_[emotion].end());
    
    ESP_LOGI(TAG, "Loaded %d frames for emotion: %s", 
             animation_files_[emotion].size(), emotion.c_str());
    
    return !animation_files_[emotion].empty();
}
```

### Background Frame Loading
```cpp
void LoadFrameTask(void* param) {
    FrameLoadRequest* request = (FrameLoadRequest*)param;
    
    FILE* f = fopen(request->filepath, "rb");
    if (f) {
        fread(request->buffer, 1, request->size, f);
        fclose(f);
        request->ready = true;
    }
    
    vTaskDelete(NULL);
}
```

## 10. Migration Path

### Step 1: Hybrid Mode
- Keep critical animations in firmware
- Load extra animations from SD card

### Step 2: Full SD Mode  
- All animations on SD card
- Minimal firmware with fallback image

### Step 3: Cloud Integration
- Download new animations via WiFi
- Store on SD card for offline use

## 11. User Experience

### SD Card Preparation Tool
Create a Python script to prepare SD card:
```python
def prepare_sd_card(source_dir, sd_path):
    # Convert images to RGB565
    # Organize into emotion directories
    # Generate metadata file
    # Copy to SD card
```

### Benefits for Users
- Easy animation customization
- Share animation packs
- Seasonal/holiday themes
- Personal photo animations