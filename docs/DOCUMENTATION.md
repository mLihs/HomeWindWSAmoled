---
title: HomeWindWSAmoled Library Documentation
version: 1.5.9
library_name: HomeWindWSAmoled
platform: ESP32
framework: Arduino
language: C/C++
dependencies:
  - name: LVGL
    version: ">=8.0.0"
    required: true
hardware:
  - microcontroller: ESP32
  - display: SH8601 AMOLED (280×456 pixels, QSPI)
  - touch: FT3168 (I2C)
features:
  - HR Widget
  - CSC Widget
  - Fan Widget (4 fans)
  - Power Save Modes
  - Touch Screen Support
  - QR Code Settings
  - Customizable Breathing Animation
api_version: 1.5.9
license: MIT
author: HomeWind Project
---

# HomeWindWSAmoled Library Documentation

## Table of Contents
1. [Library Overview](#library-overview)
2. [Installation](#installation)
3. [Architecture](#architecture)
4. [API Reference](#api-reference)
5. [Widget States](#widget-states)
6. [Power Save System](#power-save-system)
7. [Design Tokens](#design-tokens)
8. [File Structure](#file-structure)
9. [Usage Examples](#usage-examples)
10. [Troubleshooting](#troubleshooting)
11. [For AI Assistants](#for-ai-assistants)

---

## Library Overview

HomeWindWSAmoled is an Arduino library for ESP32-based fitness/cycling devices featuring AMOLED display support, LVGL graphics, and intelligent power management.

### Key Features
- **HR Widget**: Displays heart rate value and sensor name with connected/disconnected states
- **CSC Widget**: Shows cadence sensor state and RPM data with three states (Inactive, Error, Active)
- **Fan Widget**: 2x2 grid layout displaying up to 4 fans, each with three states and toggle on/off functionality
- **Settings Modal**: QR code-based device setup with animated overlay
- **Power Save Modes**: Three power states (Active, Dimmed, Soft Powersave) with automatic transitions
- **Touch Screen Support**: Full touch interaction with FT3168 controller
- **Customizable Breathing Animation**: Configurable easing curves for power save breathing effect

### Hardware Requirements
- **Microcontroller**: ESP32
- **Display**: SH8601 AMOLED panel (280×456 pixels, QSPI interface)
- **Touch Controller**: FT3168 (I2C interface)

---

## Installation

### Arduino IDE

1. **Install Dependencies**:
   - Open Arduino IDE
   - Go to `Tools > Manage Libraries`
   - Search for "LVGL" and install version 8.x or later

2. **Install Library**:
   - Download or clone this library
   - Place the `HomeWindWSAmoled` folder in your Arduino `libraries` directory
   - Restart Arduino IDE

3. **Verify Installation**:
   - Open `File > Examples > HomeWindWSAmoled > BasicExample`
   - Verify it compiles without errors

### PlatformIO

Add to your `platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
lib_deps = 
    lvgl/lvgl@^8.0.0
lib_extra_dirs = path/to/HomeWindWSAmoled
```

---

## Architecture

### Component Hierarchy
```
scr_boot (Boot Screen) - "Warming Up" during init
scr_ap (AP Screen) - "Wifisetup Mode" when captive portal active
scr_main (Main Screen)
├── stack (Flex Column Container)
│   ├── card_hr (HR Widget)
│   ├── card_csc (CSC Widget)
│   └── fan_container (Fan Widget - 2x2 Grid)
├── btn_settings (Settings Button)
└── settings_overlay (Modal Overlay)
    ├── settings_card (QR Code Container)
    │   ├── qr_code_widget
    │   └── lbl_scan (Text Label)
    └── btn_close (Close Button)
```

### Threading Model
- LVGL runs in a dedicated FreeRTOS task
- Mutex protection for thread-safe LVGL operations
- Double buffering for smooth rendering
- Touch input handled via callback

### Initialization Flow
1. `Touch_Init()` - Initialize FT3168 touch controller
2. `lcd_lvgl_Init()` - Initialize display, LVGL, and create FreeRTOS task
3. `homewind_create_screens()` - Creates Boot, AP, and Main screens; loads Boot screen initially
4. `powersave_init()` - Initialize power save system
5. App switches to Main or AP screen via `homewind_show_main_screen()` / `homewind_show_ap_screen()`

### Boot and AP Screens (v1.5.9+)
- **Boot Screen**: Shown during device init ("Warming Up", heart icon). Use `powersave_lock()` before init, `powersave_unlock()` when ready.
- **AP Screen**: Shown when WiFi captive portal is active ("Wifisetup Mode"). Call `homewind_show_ap_screen()` when entering AP mode, `homewind_show_main_screen()` when switching back to STA.

**Convenience Function:**
```cpp
homewind_init();  // Calls all initialization functions
```

---

## API Reference

### Initialization

#### `void homewind_init(void)`
Initialize the library. Must be called in `setup()` before using any other functions.

**Usage:**
```cpp
void setup() {
  Serial.begin(115200);
  homewind_init();
}
```

**What it does:**
- Initializes touch controller (`Touch_Init()`)
- Initializes display and LVGL (`lcd_lvgl_Init()`)
- Creates UI screens (`homewind_create_screens()`)
- Initializes power save system (`powersave_init()`)

#### `void homewind_create_screens(void)`
Creates and loads the main screen with all widgets. Called automatically by `homewind_init()`.

### HR Widget Functions

#### `void homewind_set_hr_state(hr_state_t state, [const char* sensor_name], [uint16_t value])`
Sets the HR widget state, sensor name, and heart rate value. The `sensor_name` and `value` parameters are optional.

**Parameters:**
- `state` (required): One of `HR_NOT_CONFIGURATED`, `HR_STATE_INACTIVE`, or `HR_STATE_ACTIVE` (since v1.5.8)
- `sensor_name` (optional): Sensor name string (max 31 characters, null-terminated). Omit to keep current name unchanged.
- `value` (optional): Heart rate value (0-65535). Omit to keep current value unchanged.

**Function Signature:**
```c
void homewind_set_hr_state(hr_state_t state, ...);
```

**Examples:**
```cpp
// Set state only (name and value unchanged)
homewind_set_hr_state(HR_NOT_CONFIGURATED);

// Set state with sensor name (value unchanged)
homewind_set_hr_state(HR_STATE_INACTIVE, "TickrFit");

// Set all parameters
homewind_set_hr_state(HR_STATE_ACTIVE, "TickrFit", 129);
```

#### `void homewind_set_hr_state_simple(hr_state_t state)`
Convenience function to set only the HR widget state without changing value or sensor name.

**Function Signature:**
```c
void homewind_set_hr_state_simple(hr_state_t state);
```

### CSC Widget Functions

#### `void homewind_set_csc_state(csc_state_t state, const char* sensor_name, uint16_t cadence)`
Sets the CSC widget state, sensor name, and cadence value.

**Function Signature:**
```c
void homewind_set_csc_state(csc_state_t state, const char* sensor_name, uint16_t cadence);
```

**Parameters:**
- `state`: One of `CSC_NOT_CONFIGURATED`, `CSC_STATE_INACTIVE`, or `CSC_STATE_ACTIVE` (since v1.5.8)
- `sensor_name`: Sensor name string (max 31 characters). Pass `NULL` to keep current name unchanged.
- `cadence`: Cadence value in RPM. Pass `0` to keep current value unchanged.

**Examples:**
```cpp
// Set to active state with cadence and sensor name
homewind_set_csc_state(CSC_STATE_ACTIVE, "Ridesense", 100);

// Set to error state (keep current sensor name and cadence)
homewind_set_csc_state(CSC_STATE_ERROR, NULL, 0);

// Update only cadence, keep state and sensor name
homewind_set_csc_state(CSC_STATE_ACTIVE, NULL, 120);
```

#### `void homewind_set_csc_state_simple(csc_state_t state)`
Convenience function to set only the CSC widget state.

**Function Signature:**
```c
void homewind_set_csc_state_simple(csc_state_t state);
```

### Fan Widget Functions

#### `void homewind_set_fan_state(uint8_t fan_index, fan_state_t state, bool is_on)`
Sets the state and toggle status of a specific fan in the fan widget. Fans with ACTIVE or ERROR states can be toggled on/off by touching them.

**Function Signature:**
```c
void homewind_set_fan_state(uint8_t fan_index, fan_state_t state);
void homewind_set_fan_state(uint8_t fan_index, fan_state_t state, bool is_on);
```

**Parameters:**
- `fan_index`: Fan index (0-3, where 0 = Fan 1, 1 = Fan 2, etc.)
- `state`: One of `FAN_STATE_NOT_CONFIGURATED`, `FAN_STATE_INACTIVE`, `FAN_STATE_ERROR`, or `FAN_STATE_ACTIVE`
- `is_on`: Toggle state (optional, defaults to `false`). Only applies to INACTIVE, ACTIVE and ERROR states.

**Examples:**
```cpp
// Set Fan 1 (index 0) to active, OFF (default)
homewind_set_fan_state(0, FAN_STATE_ACTIVE);

// Set Fan 2 (index 1) to active, ON
homewind_set_fan_state(1, FAN_STATE_ACTIVE, true);

// Set Fan 3 (index 2) to error, OFF
homewind_set_fan_state(2, FAN_STATE_ERROR, false);

// Set Fan 4 (index 3) to error, ON
homewind_set_fan_state(3, FAN_STATE_ERROR, true);

// Set Fan 1 to not configured (not toggleable, is_on parameter ignored)
homewind_set_fan_state(0, FAN_STATE_NOT_CONFIGURATED);
```

**Note:** The function uses a convenience macro that allows calling with 2 or 3 arguments. If `is_on` is omitted, it defaults to `false`.

#### `void homewind_set_fan_toggle_callback(fan_toggle_callback_t callback)`
Registers a callback function that is called when a user toggles a fan item by touching it.

**Function Signature:**
```c
typedef void (*fan_toggle_callback_t)(uint8_t fan_index, bool is_on);
void homewind_set_fan_toggle_callback(fan_toggle_callback_t callback);
```

**Parameters:**
- `callback`: Function pointer to callback that receives `fan_index` and `is_on` (true/false)

**Callback Parameters:**
- `fan_index`: Index of the fan that was toggled (0-3)
- `is_on`: New toggle state (true = on, false = off)

**Example:**
```cpp
// Register callback to handle fan toggles
homewind_set_fan_toggle_callback([](uint8_t fan_index, bool is_on) {
    Serial.print("Fan ");
    Serial.print(fan_index + 1);
    Serial.print(" toggled to: ");
    Serial.println(is_on ? "ON" : "OFF");
    
    // Control actual fan hardware here
    control_fan_hardware(fan_index, is_on);
});
```

**Note:** Only ACTIVE and ERROR state fans can be toggled. INACTIVE fans ignore touch events.

### Settings Modal Functions

#### `void homewind_show_ap_screen(void)` / `void homewind_show_main_screen(void)`
Switch between AP screen ("Wifisetup Mode") and main screen. Call from DisplayManager or app when WiFi mode changes (captive portal vs normal).

#### `void homewind_set_qr_code_url(const char* url)`
Updates the QR code URL in the settings modal.

**Function Signature:**
```c
void homewind_set_qr_code_url(const char* url);
```

**Parameters:**
- `url`: URL string (max 127 characters, null-terminated)

**Example:**
```cpp
homewind_set_qr_code_url("https://example.com/setup");
```

### Power Save Functions

#### `void powersave_init(void)`
Initialize the power save system. Called automatically by `homewind_init()`.

#### `void powersave_lock(void)` / `void powersave_unlock(void)`
Lock/unlock powersave. When locked (Boot or AP screen active), dimming and soft powersave are disabled. Call `powersave_lock()` before display init, `powersave_unlock()` when showing main screen.

#### `void on_user_activity(void)`
Reset inactivity timer and return to active state. Called automatically on touch.

#### `void set_state_active(void)`
Manually set power state to active.

#### `void set_state_dimmed(void)`
Manually set power state to dimmed.

#### `void set_state_soft_powersave(void)`
Manually set power state to soft powersave.

#### `void set_breathing_inhale_curve(breathing_ease_type_t ease_type)`
Set the easing curve for breathing animation inhale phase.

**Function Signature:**
```c
void set_breathing_inhale_curve(breathing_ease_type_t ease_type);
```

#### `void set_breathing_exhale_curve(breathing_ease_type_t ease_type)`
Set the easing curve for breathing animation exhale phase.

**Function Signature:**
```c
void set_breathing_exhale_curve(breathing_ease_type_t ease_type);
```

**Available Easing Types:**
- `BREATHING_EASE_LINEAR` - Linear interpolation
- `BREATHING_EASE_QUADRATIC_IN` - Slow start, fast end
- `BREATHING_EASE_QUADRATIC_OUT` - Fast start, slow end
- `BREATHING_EASE_QUADRATIC_IN_OUT` - Slow start and end
- `BREATHING_EASE_CUBIC_IN` - Very slow start
- `BREATHING_EASE_CUBIC_OUT` - Very slow end
- `BREATHING_EASE_CUBIC_IN_OUT` - Very slow start and end
- `BREATHING_EASE_SINE_IN` - Smooth acceleration
- `BREATHING_EASE_SINE_OUT` - Smooth deceleration
- `BREATHING_EASE_SINE_IN_OUT` - Smooth start and end

**Example:**
```cpp
set_breathing_inhale_curve(BREATHING_EASE_CUBIC_IN);
set_breathing_exhale_curve(BREATHING_EASE_SINE_OUT);
```

---

## Widget States

### HR Widget States (since v1.5.8)

#### `HR_STATE_ACTIVE`
- **Visual**: Pink/red card background, white heart icon (U+31), HR value displayed
- **Labels**: HR value (large number) / Sensor name (e.g., "TickrFit")
- **Use Case**: HR sensor connected and providing data

#### `HR_STATE_INACTIVE`
- **Visual**: Pink/red card background, white disconnected icon (U+34), no HR value
- **Labels**: Disconnected icon / "Not Connected"
- **Use Case**: HR sensor configured but not connected

#### `HR_NOT_CONFIGURATED`
- **Visual**: Pink/red card background, white disconnected icon (U+34), no HR value
- **Labels**: Disconnected icon / "Not Configurated"
- **Use Case**: HR sensor not configured

### CSC Widget States (since v1.5.8)

#### `CSC_NOT_CONFIGURATED`
- **Visual**: Light blue background (50% opacity), white circle with blue "!" icon
- **Labels**: "CSC Sensor" / "Not Configurated"
- **Use Case**: Sensor not configured

#### `CSC_STATE_INACTIVE`
- **Visual**: Blue background, white circle with blue "!" icon
- **Labels**: Sensor name / "Not Connected"
- **Use Case**: Sensor configured but not connected

#### `CSC_STATE_ACTIVE`
- **Visual**: Blue background, checkmark icon (no circle)
- **Labels**: Sensor name / "XXX RPM"
- **Use Case**: Sensor connected and providing data

### Fan Widget States

The Fan widget supports toggle functionality. Fans in INACTIVE, ACTIVE or ERROR states can be toggled on/off by touching them. NOT_CONFIGURATED fans cannot be toggled.

#### `FAN_STATE_NOT_CONFIGURATED`
- **Visual**: Gray card (50% opacity)
- **Content**: No icon, no label (completely empty card)
- **Toggle**: Not toggleable (touch events ignored)
- **Use Case**: Fan slot not configured

#### `FAN_STATE_INACTIVE` (off, clickable)
- **Visual**: White card (80% opacity), green circle (no checkmark), dark text
- **Layout**: Icon left, text right
- **Content**: Fan name visible (e.g., "Fan 1")
- **Toggle**: Can be toggled by touching
- **Use Case**: Fan active but currently off

#### `FAN_STATE_ERROR` (no icon)
- **Visual**: Pink/red when on; white card when off. No icon in either case.
- **Content**: Fan name visible
- **Toggle**: Can be toggled by touching
- **Use Case**: Fan error (e.g. not connected)

#### `FAN_STATE_ACTIVE` (on, no checkmark)
- **Visual**: Green background (#06BB60), no icon/checkmark, white text
- **Content**: Fan name visible (e.g., "Fan 1")
- **Toggle**: Can be toggled by touching
- **Use Case**: Fan active and currently on

**Toggle Animation**: When toggling between on/off states, a smooth 300ms color animation (ease-in-out) transitions the background color.

---

## Power Save System

The library automatically manages power states based on user inactivity:

### Power States

1. **ACTIVE** (default)
   - Full brightness (255)
   - All features enabled
   - Normal operation

2. **DIMMED**
   - Reduced brightness (120, ~47%)
   - Activated after 10 seconds of inactivity
   - Transparent overlay for touch detection

3. **SOFT_POWERSAVE**
   - Minimal brightness with breathing animation (80-120 range)
   - Activated after 12 seconds of inactivity (2s after dimmed)
   - Pure black background (AMOLED safe)
   - Animated status area with sensor icons
   - Breathing animation: 6-second cycle

### Automatic Transitions

```
ACTIVE → (10s inactivity) → DIMMED → (2s more inactivity) → SOFT_POWERSAVE
  ↑                                                              ↓
  └────────────────── (touch detected) ────────────────────────┘
```

### Customization

**Change Breathing Curves:**
```cpp
set_breathing_inhale_curve(BREATHING_EASE_CUBIC_IN);
set_breathing_exhale_curve(BREATHING_EASE_SINE_OUT);
```

**Manual State Control:**
```cpp
set_state_dimmed();              // Force dimmed state
set_state_soft_powersave();      // Force soft powersave
set_state_active();               // Force active state
```

**Touch Detection:**
- Touch events automatically call `on_user_activity()`
- Returns to ACTIVE state immediately
- Hides settings overlay if open

---

## Design Tokens

### Colors
```c
#define COLOR_BG_APP           0xEEF2FA  // Background: Light gray-blue
#define COLOR_HR_CARD          0xF00F66  // HR Card: Pink/Red
#define COLOR_CSC_ACTIVE       0x25ADD3  // CSC Active/Error: Blue
#define COLOR_CSC_INACTIVE      0x25ADD3  // CSC Inactive: Blue (50% opacity)
#define COLOR_FAN_CARD         0xFFFFFF  // Fan Card: White (80% opacity)
#define COLOR_FAN_INACTIVE     0xFFFFFF  // Fan Inactive: White (50% opacity)
#define COLOR_FAN_UNCONFIGURED 0xAFBDD4  // Fan Unconfigured: Light blue-gray
#define COLOR_FAN_ACTIVE_ON    0x06BB60  // Fan Active On: Green background
#define COLOR_FAN_ERROR_ON     0xF00F66  // Fan Error On: Red/pink background
#define COLOR_MODAL_BG         0x000000  // Modal Background: Black (90% opacity)
#define COLOR_MODAL_CARD       0x1E1E2A  // Modal Card: Dark blue-gray
#define COLOR_BUTTON           0xF00F66  // Button: Pink/Red
#define COLOR_TEXT_DARK        0x516682  // Text Dark: Medium blue-gray (updated for Fan Active Off)
#define COLOR_ERROR_ICON       0xF00F66  // Error Icon: Pink/Red
#define COLOR_SUCCESS_ICON     0x25C46B  // Success Icon: Green
```

### Border Radius
```c
#define RADIUS_CARD    24  // Main cards (HR, Settings)
#define RADIUS_WIDGET  28  // Widget cards (CSC, Fan)
#define RADIUS_BUTTON  32  // Buttons
#define RADIUS_ICON    18  // Icon circles
```

### Animation
```c
#define MODAL_ANIMATION_TIME_MS       300  // Modal overlay animation duration
#define FAN_TOGGLE_ANIMATION_TIME_MS  300  // Fan toggle color transition duration
#define FAN_ANIM_RESOLUTION           1024 // Fan animation interpolation resolution
#define BREATHING_ANIM_TIME           6000  // Breathing animation cycle (6 seconds)
```

### Fonts
- **Inter Black 64px**: HR value numbers
- **Inter Bold 24px**: HR sensor name, button labels, QR code text
- **Inter Bold 20px**: Fan names, CSC status
- **Inter Bold 14px**: CSC sensor name
- **Icon Font 56px**: Heart icon (U+31), Disconnected icon (U+34) - Powersave screen
- **Icon Font 36px**: Checkmark (U+32), Error (U+33) - Main screen

### Icon Font Mapping
- **U+31 (character "1")**: Heart icon (HR widget - connected state, Powersave screen)
- **U+32 (character "2")**: Checkmark icon (CSC/Fan active state)
- **U+33 (character "3")**: Error icon (CSC/Fan error/inactive state)
- **U+34 (character "4")**: Disconnected icon (HR widget - disconnected state)
- **U+35 (character "5")**: CSC icon (Powersave screen)
- **U+36 (character "6")**: Fan icon (Powersave screen)

---

## File Structure

```
HomeWindWSAmoled/
├── src/
│   ├── HomeWindWSAmoled.h      # Main library header
│   ├── homewind_ui.h            # UI header with API declarations
│   ├── homewind_ui.c             # UI implementation
│   ├── powersave.h               # Power save header
│   ├── powersave.c               # Power save implementation
│   ├── lcd_bsp.h                 # LCD board support package header
│   ├── lcd_bsp.c                 # LCD initialization and LVGL setup
│   ├── lcd_config.h              # Hardware pin configuration
│   ├── esp_lcd_sh8601.h          # SH8601 display driver header
│   ├── esp_lcd_sh8601.c          # SH8601 display driver implementation
│   ├── FT3168.h                  # FT3168 touch controller header
│   ├── FT3168.cpp                # FT3168 touch controller implementation
│   └── fonts/                    # Font files directory
│       ├── lv_font_icon_36.c
│       ├── lv_font_icon_56.c
│       ├── lv_font_inter_black_64.c
│       ├── lv_font_inter_bold_14.c
│       ├── lv_font_inter_bold_20.c
│       └── lv_font_inter_bold_24.c
├── examples/
│   ├── BasicExample/
│   │   └── BasicExample.ino
│   ├── FullFeatures/
│   │   └── FullFeatures.ino
│   └── PowersaveCustomization/
│       └── PowersaveCustomization.ino
├── library.properties
├── keywords.txt
├── README.md
└── DOCUMENTATION.md
```

---

## Usage Examples

### Complete Initialization Example
```cpp
#include <HomeWindWSAmoled.h>

void setup() {
  Serial.begin(115200);
  
  // Initialize the library (does everything)
  homewind_init();
  
  // Set initial states
  homewind_set_hr_state(HR_STATE_ACTIVE, "TickrFit", 75);
  homewind_set_csc_state(CSC_STATE_ACTIVE, "Wahoo", 90);
  
  // Set fan states with toggle support
  homewind_set_fan(0, FAN_STATE_INACTIVE, false);  // Fan 1: Active green, OFF
  homewind_set_fan(1, FAN_STATE_ACTIVE, true);     // Fan 2: Active green, ON
  
  // Register fan toggle callback
  homewind_set_fan_toggle_callback([](uint8_t fan_index, bool is_on) {
    Serial.print("Fan ");
    Serial.print(fan_index + 1);
    Serial.print(" toggled to: ");
    Serial.println(is_on ? "ON" : "OFF");
  });
}

void loop() {
  // Update values
  static uint16_t hrValue = 75;
  hrValue += random(-2, 3);
  homewind_set_hr_state(HR_STATE_ACTIVE, "TickrFit", hrValue);
  
  delay(1000);
}
```

### Dynamic State Updates
```cpp
void update_sensors() {
  // HR sensor management
  if (hr_sensor_connected()) {
    homewind_set_hr_state(HR_STATE_ACTIVE, "TickrFit", get_hr_value());
  } else if (hr_sensor_configured()) {
    homewind_set_hr_state_simple(HR_STATE_ERROR);
  } else {
    homewind_set_hr_state_simple(HR_STATE_INACTIVE);
  }
  
  // CSC sensor management
  if (csc_sensor_connected()) {
    homewind_set_csc_state(CSC_STATE_ACTIVE, "Ridesense", get_cadence());
  } else if (csc_sensor_configured()) {
    homewind_set_csc_state_simple(CSC_STATE_ERROR);
  } else {
    homewind_set_csc_state_simple(CSC_STATE_INACTIVE);
  }
  
  // Fan management
  for (int i = 0; i < 4; i++) {
    if (fan_connected(i)) {
      // Fan is connected - set to ACTIVE state
      // Preserve current toggle state, or set to OFF by default
      bool current_state = get_fan_toggle_state(i);  // Your function to get current state
      homewind_set_fan_state(i, FAN_STATE_ACTIVE, current_state);
    } else if (fan_configured(i)) {
      // Fan is configured but not connected - set to ERROR state
      bool current_state = get_fan_toggle_state(i);
      homewind_set_fan_state(i, FAN_STATE_ERROR, current_state);
    } else {
      // Fan not configured - set to NOT_CONFIGURATED (toggle state ignored)
      homewind_set_fan_state(i, FAN_STATE_NOT_CONFIGURATED);
    }
  }
}

// Register callback to handle user toggle actions
void setup() {
  homewind_init();
  
  homewind_set_fan_toggle_callback([](uint8_t fan_index, bool is_on) {
    // Update your hardware/firmware state
    set_fan_toggle_state(fan_index, is_on);  // Your function to save state
    control_fan_hardware(fan_index, is_on);  // Your function to control hardware
  });
}
```

### Power Save Customization
```cpp
void setup() {
  homewind_init();
  
  // Customize breathing animation
  set_breathing_inhale_curve(BREATHING_EASE_CUBIC_IN);
  set_breathing_exhale_curve(BREATHING_EASE_SINE_OUT);
}
```

---

## Troubleshooting

### Display not working
- **Check wiring**: Verify all display connections match `lcd_config.h`
- **Check pins**: Ensure pin definitions in `lcd_config.h` are correct
- **Check LVGL**: Verify LVGL library is installed and version 8.x or later
- **Serial output**: Check for initialization errors in Serial Monitor

### Touch not responding
- **Check I2C**: Verify FT3168 I2C connections (SDA, SCL)
- **Check address**: Verify I2C address in `FT3168.h` matches hardware
- **Check initialization**: Ensure `Touch_Init()` is called before `homewind_init()`
- **Test touch**: Use I2C scanner to verify touch controller is detected

### Compilation errors
- **LVGL missing**: Install LVGL library via Arduino Library Manager
- **ESP32 BSP**: Ensure ESP32 board support package is installed
- **Font files**: Verify all font `.c` files are in `src/fonts/` directory
- **Include paths**: Check that all includes use relative paths

### Power save not working
- **Check initialization**: Ensure `powersave_init()` is called (via `homewind_init()`)
- **Check touch**: Verify touch events are being detected
- **Check timers**: Verify FreeRTOS timers are working
- **Manual test**: Try calling `set_state_dimmed()` manually

### Widgets not updating
- **Check state**: Verify correct state enum values are used
- **Check parameters**: Ensure function parameters are correct types
- **Thread safety**: If called from ISR, ensure proper mutex protection
- **Serial debug**: Add Serial.println() to verify function calls

---

## For AI Assistants

### Code Patterns and Conventions

#### Widget Creation Pattern
All widgets follow this pattern:
1. Create container object with `lv_obj_create()`
2. Set size, style, and layout properties
3. Create child elements (labels, icons, etc.)
4. Set initial state via update function
5. Add to parent container

#### State Management Pattern
- Each widget maintains its own state variable (static)
- State changes trigger `update_*_widget()` functions
- Update functions refresh all visual elements based on state
- State enums defined in `homewind_ui.h`

#### Animation Pattern
- Animations use LVGL animation API (`lv_anim_t`)
- Timing controlled by constants (`MODAL_ANIMATION_TIME_MS`, `BREATHING_ANIM_TIME`)
- Easing functions: `lv_anim_path_ease_in_out`, custom breathing curves
- Animation callbacks handle cleanup (hiding objects, stopping animations)

#### Font Usage
- Inter fonts for text content (multiple sizes)
- Icon fonts for graphical elements (heart, checkmark, error)
- Fonts included via `#include "fonts/lv_font_*.c"` in `homewind_ui.c`
- Font files located in `src/fonts/` directory

#### Color and Opacity
- Colors defined as hex values in design tokens
- Opacity set separately using `lv_obj_set_style_bg_opa()` with `LV_OPA_*` constants
- Semi-transparent elements use opacity values (10%, 50%, 80%, 90%)
- AMOLED optimization: pure black (`0x000000`) for power save background

#### Layout System
- Flexbox layout for widget containers
- `LV_FLEX_FLOW_COLUMN` for vertical stacking
- `LV_FLEX_FLOW_ROW_WRAP` for grid layouts (Fan widget)
- Alignment using `LV_ALIGN_*` constants
- Padding and gaps controlled via style properties

### Key Implementation Details

#### Modal Overlay Positioning
- Overlay positioned absolutely at `(-2, -6)` to cover screen edges
- Size is `w + 4, h + 6` to extend beyond screen bounds
- Animation must account for negative offset (animates to `-4` not `0`)
- Hidden by default with `LV_OBJ_FLAG_HIDDEN`

#### Icon Font Implementation
- Icons use Unicode characters mapped to icon font
- Heart: character "1" (U+31) with `lv_font_icon_56` (HR connected, Powersave)
- Disconnected: character "4" (U+34) with `lv_font_icon_56` (HR disconnected)
- Checkmark: character "2" (U+32) with `lv_font_icon_36` (CSC/Fan active)
- Error: character "3" (U+33) with `lv_font_icon_36` (CSC/Fan error/inactive)
- CSC: character "5" (U+35) with `lv_font_icon_56` (Powersave screen)
- Fan: character "6" (U+36) with `lv_font_icon_56` (Powersave screen)

#### Thread Safety
- All UI updates should be called from LVGL task context
- If called from other threads, use mutex protection (see `lcd_bsp.c`)
- Mutex functions: `example_lvgl_lock()`, `example_lvgl_unlock()`
- LVGL task runs continuously in background

#### Memory Management
- All objects are static (created once, persist for lifetime)
- No dynamic allocation in UI code
- String buffers have fixed sizes (32 chars for names, 128 for URLs)
- Font data stored in program memory (Flash)
- **PSRAM for LVGL:** To move LVGL’s internal heap to PSRAM (saves internal RAM; may slightly slow UI), use the snippet in `extras/lv_conf_psram.h` and include it from your project’s `lv_conf.h`. See [PSRAM.md](PSRAM.md) for step-by-step instructions.

#### Power Save Implementation
- Uses `lv_timer` for inactivity checking (500ms interval)
- Transparent overlay for touch detection in dimmed state
- Breathing animation uses custom path callback
- Brightness controlled via `set_amoled_backlight()` function
- State transitions: ACTIVE → DIMMED (10s) → SOFT_POWERSAVE (12s total)

### Common Modifications

#### Adding New Widget
1. Add widget objects to global objects section in `homewind_ui.c`
2. Create `create_*_widget()` function
3. Create `update_*_widget()` function with state switch
4. Add to `create_main_screen()` function
5. Add setter function to `homewind_ui.h` and implement in `homewind_ui.c`
6. Add to `update_powersave_display()` if needed for powersave screen

#### Changing Colors
- Modify design token constants at top of `homewind_ui.c`
- All color references use these constants via `lv_color_hex()`
- Opacity set separately with `lv_obj_set_style_bg_opa()`

#### Adjusting Animation
- Change timing constants:
  - `MODAL_ANIMATION_TIME_MS` (default: 300ms) - Settings modal slide animation
  - `FAN_TOGGLE_ANIMATION_TIME_MS` (default: 300ms) - Fan toggle color transition
  - `BREATHING_ANIM_TIME` - Power save breathing animation
- Modify easing function in animation setup
- Available easing: `ease_in`, `ease_out`, `ease_in_out`, `linear`, `overshoot`
- Fan toggle uses `lv_anim_path_ease_in_out` (sine ease-in-out equivalent)
- Custom breathing curves via `set_breathing_*_curve()` functions

#### Adding New State
1. Add enum value to state enum in `homewind_ui.h`
2. Add case in `update_*_widget()` switch statement
3. Define visual appearance for new state (colors, icons, labels)
4. Update powersave screen if widget appears there

#### Modifying Power Save Timing
- Edit constants in `powersave.c`:
  - `DIM_TIMEOUT_MS` (default: 10000ms = 10s)
  - `SOFT_TIMEOUT_MS` (default: 12000ms = 12s total)
- Timer checks every 500ms (defined in `powersave_init()`)

### Debugging Tips
- **Check LVGL task**: Verify task is running (see `lcd_bsp.c` task creation)
- **Verify fonts**: Check for linker errors related to font files
- **Touch input**: Test touch controller with I2C scanner
- **Serial output**: Use Serial.println() for state debugging
- **Mutex issues**: Check mutex acquisition if updates fail
- **Memory**: Monitor free heap if experiencing crashes

### Performance Considerations
- UI updates are synchronous (block until complete)
- Animation runs in LVGL task (non-blocking)
- Double buffering prevents flicker
- Font rendering optimized (only required glyphs included)
- Power save reduces CPU usage in soft powersave mode
- Touch debouncing prevents excessive activity calls

### API Function Signatures Summary

```c
// Initialization
void homewind_init(void);
void homewind_create_screens(void);

// HR Widget
void homewind_set_hr_state(hr_state_t state, ...);
void homewind_set_hr_state_simple(hr_state_t state);

// CSC Widget
void homewind_set_csc_state(csc_state_t state, const char* sensor_name, uint16_t cadence);
void homewind_set_csc_state_simple(csc_state_t state);

// Fan Widget
void homewind_set_fan_state(uint8_t fan_index, fan_state_t state, bool is_on);
void homewind_set_fan_toggle_callback(fan_toggle_callback_t callback);
typedef void (*fan_toggle_callback_t)(uint8_t fan_index, bool is_on);

// Settings
void homewind_set_qr_code_url(const char* url);
void homewind_hide_settings_overlay(void);

// Power Save
void powersave_init(void);
void on_user_activity(void);
void set_state_active(void);
void set_state_dimmed(void);
void set_state_soft_powersave(void);
void set_breathing_inhale_curve(breathing_ease_type_t ease_type);
void set_breathing_exhale_curve(breathing_ease_type_t ease_type);
```

### State Enum Definitions

```c
// HR States
typedef enum {
    HR_STATE_INACTIVE = 0,
    HR_STATE_ERROR,
    HR_STATE_ACTIVE
} hr_state_t;

// CSC States
typedef enum {
    CSC_STATE_INACTIVE = 0,
    CSC_STATE_ERROR,
    CSC_STATE_ACTIVE
} csc_state_t;

// Fan States
typedef enum {
    FAN_STATE_NOT_CONFIGURATED = 0,
    FAN_STATE_INACTIVE,
    FAN_STATE_ERROR,
    FAN_STATE_ACTIVE
} fan_state_t;

// Power States
typedef enum {
    UI_POWER_STATE_ACTIVE = 0,
    UI_POWER_STATE_DIMMED,
    UI_POWER_STATE_SOFT_POWERSAVE
} ui_power_state_t;

// Breathing Easing Types
typedef enum {
    BREATHING_EASE_LINEAR = 0,
    BREATHING_EASE_QUADRATIC_IN,
    BREATHING_EASE_QUADRATIC_OUT,
    BREATHING_EASE_QUADRATIC_IN_OUT,
    BREATHING_EASE_CUBIC_IN,
    BREATHING_EASE_CUBIC_OUT,
    BREATHING_EASE_CUBIC_IN_OUT,
    BREATHING_EASE_SINE_IN,
    BREATHING_EASE_SINE_OUT,
    BREATHING_EASE_SINE_IN_OUT
} breathing_ease_type_t;
```

---

## Version History

- **1.5.4** - PSRAM for LVGL & Heap Debug Fix
  - **PSRAM:** Optional LVGL heap in PSRAM via `extras/lv_conf_psram.h`; lv_conf.h snippet and PSRAM.md steps
  - **FullFeaturesHeapDebug:** Internal RAM only for free/largest/frag (correct frag when PSRAM active); optional PSRAM line
  - **Docs:** ISSUES.md with measured configs (1/4, 1/8, 1/8+PSRAM); README/DOCUMENTATION reference to PSRAM option

- **1.5.3** - RAM & Serial Output
  - **Display buffer:** Reduced from 1/4 to 1/8 screen height (~64 KB less internal RAM, lower fragmentation)
  - **qmi8658_log:** IMU debug output off by default; define `QMI8658_DEBUG` to enable
  - **Docs:** PSRAM.md, Homewind_Implementation.md; example FullFeaturesHeapDebug

- **1.5.2** - Bug Fixes & Performance Optimization
  - **Fixed:** Powersave timer not reset on UI interaction (fan toggles, settings button)
  - **Fixed:** UI updates while modal open (unnecessary CPU usage)
  - **Fixed:** Dimming overlay visibility check before state changes
  - **No heap allocation** - All fixes use static flags/callbacks

- **1.5.1** - Code Size Optimization
  - **Removed:** Unused `I2C_master_write_read_device()` in FT3168 (never called)
  - **Removed:** Unused `qmi8658c_example()` (IMU uses `lcd_start_imu_rotation_task()`)
  - **Disabled:** `QMI8658_USE_FIFO` (FIFO code was compiled but never used)
  - **Result:** ~2.6 KB smaller flash footprint, no API or behavior changes

- **1.5.0** - CPU Performance Optimization (O1/O3)
  - **O1 - Change-Guards:** All `homewind_set_*` APIs skip LVGL updates if values unchanged
  - **O3 - Style Reuse:** Pre-defined `lv_style_t` objects for CSC states (fewer setter calls)
  - **O3 - UI State Cache:** Fan widget caches previous state to skip redundant updates
  - **Optimized:** Breathing animation only writes backlight when value changes
  - **Performance:** ~80% fewer LVGL calls when sensor values stable
  - **Responsiveness:** Direct updates for instant touch feedback

- **1.4.0** - API Cleanup & Code Quality
  - **New API:** Clean, explicit functions replace unsafe macro hacks
    - `homewind_set_hr(state, name, value)` - full HR control
    - `homewind_set_hr_state(state)` - state only
    - `homewind_set_hr_value(value)` - value only
    - `homewind_set_csc(state, name, cadence)` - full CSC control
    - `homewind_set_csc_cadence(cadence)` - cadence only
    - `homewind_set_fan(index, state, is_on)` - full fan control
    - `homewind_set_fan_toggle(index, is_on)` - toggle only
  - **Optimized:** Touch polling with rate limiting (50Hz instead of 200Hz)
  - **Optimized:** String formatting with caching (avoid repeated snprintf)
  - **Improved:** Legacy macros kept for backwards compatibility
  - **Technical:** Removed unsafe sentinel value macros `(bool)-1`

- **1.3.0** - Heap Stability & Memory Optimization
  - **Fixed:** I2C write functions now use stack buffer instead of malloc/free
  - **Fixed:** Fan toggle animation uses static allocation (no more lv_mem_alloc/free)
  - **Improved:** Eliminated heap fragmentation from hot-path operations
  - **Improved:** Touch and IMU drivers no longer allocate memory per transaction
  - **Technical:** Proper animation cleanup with lv_anim_del() before starting new

- **1.2.0** - CPU & Performance Optimization
  - **Optimized:** LVGL tick period from 2ms to 5ms (60% fewer timer interrupts)
  - **Optimized:** LVGL task min delay matched to tick period (5ms)
  - **Optimized:** IMU rotation task with hysteresis and debouncing (prevents flicker)
  - **Optimized:** IMU polling reduced from 100ms to 200ms (50% fewer I2C reads)
  - **Optimized:** Powersave icon updates with dirty-checking (skip if unchanged)
  - **Optimized:** Powersave updates skipped when screen not active
  - **Improved:** More stable rotation detection with consecutive reading requirement

- **1.1.0** - Architecture & Stability Improvements
  - **Breaking Change:** `homewind_init()` is now a proper function (was macro)
  - **Fixed:** Double initialization of UI screens and powersave system
  - **Fixed:** IMU delay functions using wrong time units (ms/µs vs ticks)
  - **Improved:** Clean separation of BSP layer and application layer
  - **Improved:** `homewind_init()` is now idempotent (safe to call multiple times)
  - **Added:** `lcd_start_imu_rotation_task()` public API for IMU control
  - **Technical:** Proper `extern "C"` linkage for C++ compatibility

- **1.0.0** - Initial library release
  - HR, CSC, and Fan widgets
  - Power save functionality with three states
  - Touch screen support
  - QR code settings modal
  - Customizable breathing animation
  - Comprehensive examples and documentation

---

## License

MIT License - see LICENSE file for details

---

## Support

For issues, questions, or contributions, please visit the [GitHub repository](https://github.com/homewind/HomeWindWSAmoled).

