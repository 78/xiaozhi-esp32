# Display Rotation & UI Alignment Guide

This guide explains how to configure the default display rotation (specifically 270°) and ensures UI elements remain centered and properly aligned across different orientations.

## 1. Setting Default Rotation (Auto Default 270°)

The default rotation is configured in the board initialization file ensuring the display starts in landscape mode.

### Implementation Location
File: `main/boards/iotforce-xiaozhi-iot-vietnam-es3n28p-lcd-2.8/iotforce_xiaozhi_iot_vietnam_es3n28p_lcd_2.8.cc`

### How it works
In the `InitializeTouch()` method (or constructor), the LVGL display rotation is explicitly set to 270 degrees:

```cpp
// Store LVGL display handle for rotation
g_lvgl_display = lv_display_get_default();
if (g_lvgl_display != NULL)
{
  // Set default rotation to 270° (landscape mode)
  lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_270);
}
```

This ensures that upon boot, the screen coordinates are mapped to 270° (Landscape), where:
- **Width** becomes 320px
- **Height** becomes 240px

## 2. Ensuring Elements Remain Centered (Fix UI Position)

To guarantee that UI elements (like the boot logo, status bar, or chat bubbles) remain centered regardless of rotation (0°, 90°, 180°, 270°), follow these three rules:

### Rule A: Use Percentage-Based Sizing
Avoid hardcoding pixel widths (e.g., `320px`). Instead, use `LV_PCT(100)`. This allows the container to resize automatically when the screen dimensions flip between Portrait and Landscape.

**Example (`main/display/lcd_display.cc`):**
```cpp
// BAD: Fixed width
lv_obj_set_width(content_, 240); 

// GOOD: Responsive width
lv_obj_set_width(content_, LV_PCT(100)); // Adapts to 240px or 320px automatically
```

### Rule B: Use Flex Layouts for Automatic Alignment
Flex layouts allow children to center themselves dynamically without manual coordinate calculation.

**Example (Centering Status Bar Elements):**
```cpp
// Create container
lv_obj_t* status_bar = lv_obj_create(parent);

// 1. Set Flex Flow (Row or Column)
lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);

// 2. Set Alignment (Horizontal, Vertical, Row Gap)
// LV_FLEX_ALIGN_SPACE_BETWEEN: Spreads items to edges
// LV_FLEX_ALIGN_CENTER: Centers items vertically
lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
```

### Rule C: Use Center Constraints for Floating Objects
For single objects like a Loading Spinner or Logo that float above the content, use `lv_obj_center()` or alignment flags ensuring they stay relative to the parent's center.

**Example:**
```cpp
lv_obj_t* loader = lv_spinner_create(screen);
lv_obj_center(loader); // Always stays in the mathematical center (w/2, h/2)
```

## 3. Gestures for Rotation
The current firmware includes a built-in gesture to rotate the screen manually for testing:
- **Action**: Hold **two fingers** on the screen for > 0.2 seconds.
- **Result**: Cycles rotation 0° -> 90° -> 180° -> 270°.

This confirms that if your UI follows the rules above, it will look correct in all orientations instantly.
