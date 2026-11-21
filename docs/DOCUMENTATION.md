---
title: HomeWindWSAmoled Library Documentation
version: 1.7.1
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
  - Status Badges (APP, Trainer)
  - HR Card
  - Power and Cadence Rows
  - Fan Control (plus/minus, mode, speed, 4 fan dots)
  - Power Save Modes (screensaver, panel off)
  - Touch Screen Support
  - QR Code Settings
  - Customizable Breathing Animation
api_version: 1.7.0
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
- **Status Bar**: pink, one block with the HR card; "APP" and "Trainer" badges (grey with white text, orange, green) and the settings gear
- **HR Card**: Heart rate value and sensor name, or sensor name and "Disconnected", or "Add Sensors"; grows when rows are hidden
- **Power and Cadence Rows**: Source name and "210 W" / "85 RPM", or "Disconnected"; hidden without a source
- **Fan Control**: Minus | mode, fan speed and four fan dots | plus. Plus/minus shift the HR range, a long press on the mode field changes the mode (the application decides)
- **Settings Modal**: QR code-based device setup with animated overlay
- **Power Save Modes**: Active, Dimmed, Screensaver (soft powersave) and Panel Off with automatic transitions
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
scr_ap (AP Screen) - "Wifisetup" when captive portal active
scr_main (Main Screen)
├── stack (Flex Column, gap 8, 2 px margin all round)
│   ├── top (pink block, flex grow, rounded at the bottom; a tap opens the modal)
│   │   ├── status bar (44 px: APP and Trainer badges 28 px high, settings gear)
│   │   └── card_hr (HR card, flex grow: 140, 204 or 268 px)
│   ├── power row (56 px, hidden without a source)
│   ├── cadence row (56 px, hidden without a source)
│   └── fan control (132 px: minus | mode, speed, dots | plus)
└── settings_overlay (full-screen modal)
    ├── QR area (244×244) with qr_code_widget
    ├── "Scan to Setup Homewind"
    └── Close button (pressed #BD0C50)
scr_powersave (Screensaver)
├── badges
└── grid: HR | power, cadence (side by side) | fans
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
- **Layout**: heart, title (Inter Bold 24) and text (Inter Bold 14, 190 px wide) centred in the top 369 px.
- **Boot Screen**: Shown during device init ("Warming Up", heart icon). Use `powersave_lock()` before init, `powersave_unlock()` when ready.
- **AP Screen**: Shown when WiFi captive portal is active ("Wifisetup"). Call `homewind_show_ap_screen()` when entering AP mode, `homewind_show_main_screen()` when switching back to STA.

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

#### `void homewind_set_hr(hr_state_t state, const char* sensor_name, uint16_t value)`
Sets the HR widget state, sensor name and heart rate value.

**Parameters:**
- `state`: one of `HR_NOT_CONFIGURATED`, `HR_STATE_INACTIVE` or `HR_STATE_ACTIVE`
- `sensor_name`: up to 63 characters, `NULL` keeps the current name. Longer names are cut off on screen.
- `value`: heart rate (0-65535)

**Examples:**
```cpp
homewind_set_hr(HR_STATE_INACTIVE, "TickrFit", 0);
homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", 129);
```

#### `void homewind_set_hr_state(hr_state_t state)`
Sets only the HR widget state; value and sensor name stay. `homewind_set_hr_state_simple(state)` is the same.

**Function Signature:**
```c
void homewind_set_hr_state_simple(hr_state_t state);
```

### Power and Cadence Rows (since v1.7.0)

```c
void homewind_set_power_row(row_state_t state, const char* name, uint16_t watts);
void homewind_set_cadence_row(row_state_t state, const char* name, uint16_t rpm);
```

- `state`: `ROW_HIDDEN` (no source; the HR card takes the height), `ROW_DISCONNECTED` (dark row, "Disconnected"), `ROW_ACTIVE` (coloured row with the value).
- `name`: source name, `NULL` keeps the current one. Names wider than the row are cut off at the right edge.
- `watts` / `rpm`: shown as "210 W" / "85 RPM" while `ROW_ACTIVE`.

Only changed parts are redrawn (state, name, value).

```cpp
homewind_set_power_row(ROW_ACTIVE, "Assioma Duo", 210);
homewind_set_cadence_row(ROW_DISCONNECTED, "Ridesense", 0);
homewind_set_power_row(ROW_HIDDEN, NULL, 0);
```

**CSC API:** `homewind_set_csc(state, name, cadence)`, `homewind_set_csc_state(state)` and `homewind_set_csc_cadence(cadence)` drive the cadence row (`CSC_NOT_CONFIGURATED` = hidden, `CSC_STATE_INACTIVE` = disconnected). `homewind_set_csc_power(watts)` is deprecated: `watts >= 0` shows the power row with the cadence row's name, `< 0` hides it.

### Status Badges (since v1.7.0)

```c
void homewind_set_badges(badge_state_t app, badge_state_t trainer);
```

`BADGE_HIDDEN`, `BADGE_GREY` (set up, not connected yet), `BADGE_ORANGE` (was connected, now lost), `BADGE_GREEN` (connected). The screensaver shows the same badges.

### Fan Control (since v1.7.0)

```c
void homewind_set_fan_control(fan_mode_t mode, bool target_on, uint8_t target_pct);
void homewind_set_fan(uint8_t fan_index, fan_state_t state, bool is_on);
void homewind_set_fan_state(uint8_t fan_index, fan_state_t state);
void homewind_set_fan_toggle(uint8_t fan_index, bool is_on);
void homewind_set_fan_control_callbacks(hr_shift_callback_t shift, fan_mode_callback_t mode);
```

- `homewind_set_fan_control()`: centre field. `mode` is the mode in effect (`FAN_MODE_PURE`, `FAN_MODE_SMART`, `FAN_MODE_HYBRID`), the speed in percent reads "63" (no "%", green while `target_on` and the speed is above 0) and "0" while off.
- `homewind_set_fan*()`: one of the four dots below the speed (see [Fan Dots](#fan-dots)).
- `homewind_set_fan_control_callbacks()`:
  - `shift(int8_t steps)`: a tap on plus calls `shift(-1)` (lower the HR range, more wind), minus calls `shift(+1)`.
  - `mode(void)`: a 700 ms long press on the mode field. The application picks the next mode and reports it back with `homewind_set_fan_control()`.
  - Both run in the LVGL task with the LVGL lock held. Only queue the work there (for example an atomic flag) and apply it in your own task.
  - A press that wakes the display (dimmed or screensaver) does not reach the controls.

```cpp
static std::atomic<int8_t> s_shift{0};
static std::atomic<bool> s_mode{false};

homewind_set_fan_control_callbacks(
    [](int8_t steps) { s_shift += steps; },
    []() { s_mode = true; });
```

`homewind_set_fan_toggle_callback()` is deprecated since 1.7.0: the main screen has no fan toggles, the callback is stored and never called.

### Settings Modal Functions

#### `void homewind_show_ap_screen(void)` / `void homewind_show_main_screen(void)`
Switch between AP screen ("Wifisetup") and main screen. Call from DisplayManager or app when WiFi mode changes (captive portal vs normal).

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

#### `void homewind_powersave_wake(void)` (since v1.7.0)
Activity that is not a touch: a sensor connects or sends data, an app connects, a web UI change. Restarts the 10 min panel-off timeout; a switched-off panel comes back on with the screensaver. The main screen and the dim timeout stay as they are. Call under the LVGL lock; cheap enough for every data update (Homewind calls it at most once per second).

#### `void set_state_active(void)`
Wakes the display as a touch would: panel on, main screen with fresh values, full brightness, dim timeout restarted.

#### `void set_state_dimmed(void)`
Dims the main screen; from the screensaver or panel off it loads the main screen with fresh values first.

#### `void set_state_soft_powersave(void)`
Shows the screensaver; a switched-off panel comes back on.

Call the three under the LVGL lock.

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
- **Visual**: Pink/red card background, HR value (Inter Black 64) and heart icon (U+31)
- **Labels**: HR value / sensor name below in Inter Bold 24, one line: centred if it fits, otherwise left-aligned and cut off
- **Use Case**: HR sensor connected and providing data

#### `HR_STATE_INACTIVE`
- **Visual**: Pink/red card, disconnected heart icon with "!" (U+34)
- **Labels**: sensor name (as above) above "Disconnected", both Inter Bold 24, 8 px below the Figma position
- **Use Case**: HR sensor configured but not connected, or no value yet (value 0)

#### `HR_NOT_CONFIGURATED`
- **Visual**: Pink/red card, heart icon (U+31)
- **Labels**: "Add Sensors" (Inter Bold 24)
- **Use Case**: HR sensor not configured

### Row States (since v1.7.0)

| State | Power row | Cadence row |
|---|---|---|
| `ROW_ACTIVE` | violet `COLOR_POWER`, "210 W" | blue `COLOR_CADENCE`, "85 RPM" |
| `ROW_DISCONNECTED` | dark `COLOR_POWER_DARK`, "Disconnected" | dark `COLOR_CADENCE_DARK`, "Disconnected" |
| `ROW_HIDDEN` | hidden | hidden |

Both rows show the source name (Inter Bold 14, cut off at the right edge) above the value; the icon (bolt, crank) sits in a white circle.

### Fan Dots

| State | Dot |
|---|---|
| `FAN_STATE_NOT_CONFIGURATED` | grey (slot empty) |
| `FAN_STATE_INACTIVE` | white (configured, no info yet) |
| `FAN_STATE_ERROR` | orange `COLOR_WARNING` |
| `FAN_STATE_ACTIVE` | green `COLOR_GREEN` while `is_on`, else white |

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
   - Activated after 8 seconds without touch

3. **SOFT_POWERSAVE** (screensaver)
   - Breathing brightness (80-120, 6-second cycle)
   - Activated 20 seconds after dimming
   - Black background: badges at the top, then a grid of HR alone, power and cadence side by side (108 px slots), fans; rows 24 px apart, the block centred below the badges (Figma 1251:6912, 6946, 6978). Icon 56 above the value in Inter Bold 24 with unit ("170 BPM", "295 W", "85 RPM", "60 %"). A disconnected source shows its grey icon with "--"; unconfigured sources are hidden, empty rows close up.

4. **PANEL_OFF** (since v1.7.0)
   - Screensaver with the panel switched off (DISPOFF, brightness 0) against burn-in
   - Activated after 10 minutes in the screensaver without activity. Touch and `homewind_powersave_wake()` count as activity.
   - `homewind_powersave_wake()` switches the panel back on with the screensaver; a touch returns to ACTIVE.

### Automatic Transitions

```
ACTIVE → (8 s) → DIMMED → (20 s) → SOFT_POWERSAVE → (10 min without activity) → PANEL_OFF
  ↑                                     ↑                                           │
  │                                     └──── homewind_powersave_wake() ────────────┤
  └──────────────────────────── touch (from any state) ────────────────────────────┘
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
- The touch that wakes the display only wakes it; controls ignore that press (since v1.7.0)
- Hides settings overlay if open

---

## Design Tokens

### Colors
```c
#define COLOR_BG_APP            0x000000
#define COLOR_ACCENT            0xF00F66   /* heart rate; QR close button */
#define COLOR_POWER             0x7A5AF8
#define COLOR_POWER_DARK        0x322566   /* power row disconnected */
#define COLOR_CADENCE           0x0E7FA3
#define COLOR_CADENCE_DARK      0x0A2933   /* cadence row disconnected */
#define COLOR_CADENCE_LIGHT     0x25ADD3   /* cadence icon in the screensaver */
#define COLOR_CONTROL_PRESSED   0x2F2F42   /* plus, minus, mode field pressed */
#define COLOR_ACCENT_PRESSED    0xBD0C50   /* QR close button pressed */
#define COLOR_WHITE             0xFFFFFF
#define COLOR_FAN_UNCONFIGURED  0x303132
#define COLOR_MODAL_BG          0x000000
#define COLOR_MODAL_CARD        0x1E1E2A   /* also the dark control fields */
#define COLOR_QR_AREA           0x1E2733
#define COLOR_GREEN             0x06BB60   /* fan on, badge connected */
#define COLOR_WARNING           0xFC9300   /* fan error, badge lost */
#define COLOR_BADGE_GREY        0x3D4F67
#define COLOR_DISCONNECTED      0x999999
```

### Layout (homewind_ui.c)
```c
#define RADIUS_CARD      28   // HR card, rows
#define RADIUS_CONTROL   32   // fan control
#define RADIUS_FIELD     8    // minus, mode, plus fields
#define RADIUS_BUTTON    36   // QR close button
#define RADIUS_QR        24   // QR area
#define SCREEN_GAP       8
#define STATUS_BAR_H     44
#define ROW_H            56
#define FAN_CONTROL_H    132
#define LONG_PRESS_MS    700  // mode field
```

### Animation and Timing
```c
#define MODAL_ANIMATION_TIME_MS  180     // Modal overlay animation duration
#define MODAL_SLIDE_DISTANCE     60      // Modal overlay slide distance (px)
#define BREATHING_ANIM_TIME      6000    // Breathing animation cycle (powersave.c)
#define DIM_TIMEOUT_MS           8000    // powersave.c
#define SOFT_TIMEOUT_MS          20000
#define PANEL_OFF_TIMEOUT_MS     (10UL * 60UL * 1000UL)
```

### Fonts
- **Inter Black 64px**: HR value
- **Inter Bold 24px**: HR sensor name, row values, "Disconnected", "Add Sensors", fan mode and speed, screensaver values, QR text, buttons
- **Inter Bold 14px**: Row sensor names, badges
- **Icon Font 56px**: U+31 to U+36 and U+39 (HR card heart; screensaver heart, disconnected heart, bolt, crank, fan)
- **Icon Font 36px**: U+31 to U+3A (all icons below)

### Icon Font Mapping (`ui_style_helpers.h`)
- **"1" `ICON_HEART`**: Heart
- **"2"**: Checkmark (unused since 1.7.0)
- **"3"**: Error "!" (unused since 1.7.0)
- **"4" `ICON_HEART_OFF`**: Disconnected heart (HR card and screensaver)
- **"5" `ICON_CRANK`**: Crank (cadence)
- **"6" `ICON_FAN`**: Fan / wind
- **"7" `ICON_PLUS`**, **"8" `ICON_MINUS`**: Fan control (since 1.7.0)
- **"9" `ICON_BOLT`**: Power (since 1.7.0)
- **":" `ICON_GEAR`**: Settings (since 1.7.0)

New glyphs go into `fonts/FanFont.ttf` with `fonts/add_icons.py`, then `fonts/generate_fonts.sh`.

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
  
  // Initialize the library (display, LVGL, touch, screens, powersave)
  homewind_init();

  // Leave the boot screen; powersave stays off until unlocked
  if (lcd_lvgl_lock(-1)) {
    homewind_show_main_screen();
    powersave_unlock();
    lcd_lvgl_unlock();
  }
  
  // Set initial states
  homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", 75);
  homewind_set_power_row(ROW_ACTIVE, "Assioma Duo", 180);
  homewind_set_cadence_row(ROW_ACTIVE, "Wahoo", 90);
  homewind_set_badges(BADGE_GREEN, BADGE_HIDDEN);

  // Fan dots and the centre field
  homewind_set_fan(0, FAN_STATE_ACTIVE, true);     // green
  homewind_set_fan(1, FAN_STATE_INACTIVE, false);  // white
  homewind_set_fan_control(FAN_MODE_SMART, true, 63);
}

void loop() {
  // Update values
  static uint16_t hrValue = 75;
  hrValue += random(-2, 3);
  homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", hrValue);
  
  delay(1000);
}
```

### Dynamic State Updates
```cpp
void update_sensors() {
  // HR sensor management
  if (hr_sensor_connected()) {
    homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", get_hr_value());
  } else if (hr_sensor_configured()) {
    homewind_set_hr(HR_STATE_INACTIVE, "TickrFit", 0);
  } else {
    homewind_set_hr_state(HR_NOT_CONFIGURATED);
  }
  
  // Cadence row
  if (csc_sensor_connected()) {
    homewind_set_cadence_row(ROW_ACTIVE, "Ridesense", get_cadence());
  } else if (csc_sensor_configured()) {
    homewind_set_cadence_row(ROW_DISCONNECTED, "Ridesense", 0);
  } else {
    homewind_set_cadence_row(ROW_HIDDEN, NULL, 0);
  }
  
  // Fan dots
  for (int i = 0; i < 4; i++) {
    if (fan_connected(i)) {
      homewind_set_fan(i, FAN_STATE_ACTIVE, fan_running(i));
    } else if (fan_configured(i)) {
      homewind_set_fan(i, FAN_STATE_ERROR, false);
    } else {
      homewind_set_fan_state(i, FAN_STATE_NOT_CONFIGURATED);
    }
  }
}
```

Call the setters from a task other than the LVGL task only with `lcd_lvgl_lock()` held.

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
- `LV_FLEX_FLOW_ROW` for the fan control fields; hidden rows (`LV_OBJ_FLAG_HIDDEN`) leave the flex layout, the HR card grows (`flex_grow`)
- Alignment using `LV_ALIGN_*` constants
- Padding and gaps controlled via style properties

### Key Implementation Details

#### Modal Overlay Positioning
- Overlay positioned absolutely at `(-2, -6)` to cover screen edges
- Size is `w + 4, h + 6` to extend beyond screen bounds
- Animation must account for negative offset (animates to `-4` not `0`)
- Hidden by default with `LV_OBJ_FLAG_HIDDEN`

#### Icon Font Implementation
- Icons use ASCII characters mapped to the icon font (`ICON_*` in `ui_style_helpers.h`, see [Icon Font Mapping](#icon-font-mapping-ui_style_helpersh))
- HR card and screensaver: `lv_font_icon_56`; rows, fan control and status bar: `lv_font_icon_36`

#### Thread Safety
- All UI updates should be called from LVGL task context
- If called from other threads, use mutex protection (see `lcd_bsp.c`)
- Mutex functions: `example_lvgl_lock()`, `example_lvgl_unlock()`
- LVGL task runs continuously in background

#### Memory Management
- All objects are static (created once, persist for lifetime)
- No dynamic allocation in UI code
- String buffers have fixed sizes (64 chars for names, 128 for URLs)
- Font data stored in program memory (Flash)
- **PSRAM for LVGL:** To move LVGL’s internal heap to PSRAM (saves internal RAM; may slightly slow UI), use the snippet in `extras/lv_conf_psram.h` and include it from your project’s `lv_conf.h`. See [PSRAM.md](PSRAM.md) for step-by-step instructions.

#### Power Save Implementation
- Uses `lv_timer` for inactivity checking (500ms interval)
- Transparent overlay for touch detection in dimmed state
- Breathing animation uses custom path callback
- Brightness controlled via `set_amoled_backlight()` function
- State transitions: ACTIVE → DIMMED (8 s) → SOFT_POWERSAVE (20 s later) → PANEL_OFF (10 min without activity)

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
  - `MODAL_ANIMATION_TIME_MS` (default: 180ms) and `MODAL_SLIDE_DISTANCE` (default: 60px) - Settings modal slide animation
  - `BREATHING_ANIM_TIME` - Power save breathing animation
- Modify easing function in animation setup
- Available easing: `ease_in`, `ease_out`, `ease_in_out`, `linear`, `overshoot`
- Custom breathing curves via `set_breathing_*_curve()` functions

#### Adding New State
1. Add enum value to state enum in `homewind_ui.h`
2. Add case in `update_*_widget()` switch statement
3. Define visual appearance for new state (colors, icons, labels)
4. Update powersave screen if widget appears there

#### Modifying Power Save Timing
- Edit constants in `powersave.c`:
  - `DIM_TIMEOUT_MS` (default: 8000 ms)
  - `SOFT_TIMEOUT_MS` (default: 20000 ms after dimming)
  - `PANEL_OFF_TIMEOUT_MS` (default: 10 min in the screensaver without activity)
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

// Power and cadence rows
void homewind_set_power_row(row_state_t state, const char* name, uint16_t watts);
void homewind_set_cadence_row(row_state_t state, const char* name, uint16_t rpm);
void homewind_set_csc(csc_state_t state, const char* sensor_name, uint16_t cadence);  // cadence row
void homewind_set_csc_state(csc_state_t state);
void homewind_set_csc_cadence(uint16_t cadence);
void homewind_set_csc_power(int32_t watts);  // deprecated

// Status badges
void homewind_set_badges(badge_state_t app, badge_state_t trainer);

// Fan control
void homewind_set_fan(uint8_t fan_index, fan_state_t state, bool is_on);
void homewind_set_fan_state(uint8_t fan_index, fan_state_t state);
void homewind_set_fan_toggle(uint8_t fan_index, bool is_on);
void homewind_set_fan_control(fan_mode_t mode, bool target_on, uint8_t target_pct);
void homewind_set_fan_control_callbacks(hr_shift_callback_t shift, fan_mode_callback_t mode);
typedef void (*hr_shift_callback_t)(int8_t steps);  // -1 = plus, +1 = minus
typedef void (*fan_mode_callback_t)(void);           // long press on the mode field
void homewind_set_fan_toggle_callback(fan_toggle_callback_t callback);  // deprecated

// Settings
void homewind_set_qr_code_url(const char* url);
void homewind_hide_settings_overlay(void);

// Power Save
void powersave_init(void);
void on_user_activity(void);
void homewind_powersave_wake(void);
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
    HR_NOT_CONFIGURATED = 0,
    HR_STATE_INACTIVE,
    HR_STATE_ACTIVE
} hr_state_t;

// CSC States (cadence row)
typedef enum {
    CSC_NOT_CONFIGURATED = 0,
    CSC_STATE_INACTIVE,
    CSC_STATE_ACTIVE
} csc_state_t;

// Row States
typedef enum {
    ROW_HIDDEN = 0,
    ROW_DISCONNECTED,
    ROW_ACTIVE
} row_state_t;

// Badge States
typedef enum {
    BADGE_HIDDEN = 0,
    BADGE_GREY,
    BADGE_ORANGE,
    BADGE_GREEN
} badge_state_t;

// Fan Modes
typedef enum {
    FAN_MODE_PURE = 0,
    FAN_MODE_SMART,
    FAN_MODE_HYBRID
} fan_mode_t;

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
    UI_POWER_STATE_SOFT_POWERSAVE,
    UI_POWER_STATE_PANEL_OFF
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

Versions after 1.5.4 are described in [CHANGELOG.md](../CHANGELOG.md); 1.7.0 is the Touch UI redesign (status bar, power and cadence rows, fan control, new screensaver, panel off).

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

