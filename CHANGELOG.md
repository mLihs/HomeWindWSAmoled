# Changelog

All notable changes to the HomeWindWSAmoled library are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [1.7.0] - 2026-11-21

Touch UI redesign (Homewind `Refactoring/TOUCH_UI_CONCEPT_V2.md`, Figma "Homewind Touch").

### Added
- **Main screen layout:** status bar (APP and Trainer badges, settings gear) and HR card in one pink block rounded at the bottom; a tap anywhere on it opens the QR modal. Then power row, cadence row and fan control field in one flex column with a 2 px black margin all round (case bezel). Hidden rows give their height to the HR card (140, 204 or 268 px).
- **Power and cadence rows:** `homewind_set_power_row(state, name, watts)` and `homewind_set_cadence_row(state, name, rpm)` with `row_state_t` (`ROW_HIDDEN`, `ROW_DISCONNECTED`, `ROW_ACTIVE`). Each row shows the source name (long names are cut off at the right edge) and "210 W" / "85 RPM", or "Disconnected". Power is violet, cadence blue. Units in capitals ("RPM", "BPM").
- **Status badges:** `homewind_set_badges(app, trainer)` with `badge_state_t` (`BADGE_HIDDEN`, `BADGE_GREY`, `BADGE_ORANGE`, `BADGE_GREEN`). Grey badges have white text. 28 px high, on the main screen and in the screensaver.
- **Fan control field:** minus | mode, speed and fan dots | plus. `homewind_set_fan_control(mode, target_on, pct)` sets the mode in effect (`fan_mode_t`) and the speed in percent ("63", no "%", green while the fans run). `homewind_set_fan_control_callbacks(shift, mode)`: plus/minus call `shift(-1)` / `shift(+1)` on a tap (pressed background `COLOR_CONTROL_PRESSED` #2F2F42), the mode field calls `mode()` on a 700 ms long press (same pressed background while held). Callbacks run in the LVGL task with the lock held; queue the work.
- **Screensaver:** badges at the top, then a grid of HR alone, power and cadence side by side (108 px slots, Figma 90 without units), fans; rows 24 px apart, the block centred below the badges (Figma 1251:6912, 6946, 6978). Icons in icon font 56 as before (cadence in `COLOR_CADENCE_LIGHT`), values in Inter Bold 24 with units ("170 BPM", "295 W", "85 RPM", "60 %"). A disconnected source shows its grey icon with "--"; the fan icon is green while the fans run, white at target 0. Unconfigured sources are hidden, and empty rows close up.
- **Panel off:** after 10 min in the screensaver without activity the panel is switched off (`UI_POWER_STATE_PANEL_OFF`, DISPOFF and brightness 0) against burn-in. `homewind_powersave_wake()` restarts the timeout and switches the panel back on with the screensaver; call it for non-touch activity (sensor data, app connects, web UI). `lcd_set_display_on(bool)` in `lcd_bsp.h`.
- **Icon font:** plus, minus, bolt and gear glyphs in `FanFont.ttf` (`fonts/add_icons.py`), icon font 36 now covers "1"–":", icon font 56 adds the bolt "9". Inter Bold 14 and 24 cover printable ASCII except `$ ( ) [ ] ` { } |` (those would grow the line height and move the layout), so names from BLE advertising such as "HRM-Pro:123456" show ":", "_", "+", "#", "'", "&" and "." instead of placeholder boxes.
- **Colours:** `COLOR_POWER`, `COLOR_POWER_DARK`, `COLOR_CADENCE`, `COLOR_CADENCE_DARK`, `COLOR_CADENCE_LIGHT`, `COLOR_QR_AREA`, `COLOR_GREEN`, `COLOR_WARNING`, `COLOR_BADGE_GREY`, `COLOR_CONTROL_PRESSED`, `COLOR_ACCENT_PRESSED`.

### Changed
- **Touch while dimmed or in the screensaver only wakes the display:** the press that wakes it does not reach any control.
- **HR card** (Figma): the full heart, the heart with "!" while disconnected. Connected: value and heart, sensor name below. Disconnected: sensor name above "Disconnected" instead of "--", the texts 8 px lower than in Figma. All texts in Inter Bold 24 as before; the name is one line, centred if it fits, otherwise left-aligned and cut off at the right edge. Not configured: "Add Sensors".
- **Sensor names** up to 63 characters (before 31) for the HR card and the rows; longer names are cut off on screen, not in the buffer.
- **Text positions** follow Figma's baselines, not its text boxes: the generated fonts have tight line heights (Bold 14: 14 px, Bold 24: 23 px), so the label positions and paddings are corrected for that.
- **CSC API** drives the cadence row. **Fan API** (`homewind_set_fan*`) drives the four dots in the fan control field (grey empty, white configured, orange error, green on).
- **QR modal:** full screen, QR on a dark field, "Scan to Setup Homewind", pink Close button (pressed #BD0C50). **Boot/AP screen:** heart icon, AP title "Wifisetup", content centred in the top 369 px as in Figma.
- `apply_transparent_container_style()` also clears `CLICKABLE` and `SCROLLABLE`, so a touch on a container reaches the control or screen below.
- **Examples** leave the boot screen (`homewind_show_main_screen()`, `powersave_unlock()` under the LVGL lock); since 1.5.9 they stayed on "Warming Up" and never dimmed. Timeouts in their output corrected (8 s dimmed, screensaver 20 s later).

### Deprecated
- `homewind_set_csc_power()`: shows the power row with the cadence row's name; use `homewind_set_power_row()`.
- `homewind_set_fan_toggle_callback()`: the main screen has no fan toggles; the callback is stored and never called.

### Removed
- `update_powersave_icons()`, replaced by `update_powersave_view()` (internal, filled by `homewind_ui.c`).
- Colours `COLOR_CSC`, `COLOR_TEXT_DARK`, `COLOR_SUCCESS_ICON`, `COLOR_FAN_ACTIVE_ON`.

---

## [1.6.1] - 2026-11-08

### Changed
- **Display buffer:** `EXAMPLE_LVGL_BUF_HEIGHT` from 1/8 to 1/16 screen height (28 lines, 2 × 15,680 B instead of 2 × 31,920 B), still double-buffered. About 32.5 KB more free internal (DMA) RAM on Touch. LVGL flushes full-width strips of 28 lines (before: 56, the rounder keeps even rows); the last strip is 8 lines.
- **QSPI bus `max_transfer_sz`:** one draw buffer (15,680 B) instead of a full frame (255,360 B). The SPI bus allocates its DMA descriptors (12 B per 4092 B, TX and RX) from this size: 4 instead of 63 each, about 1.4 KB less internal RAM. The bus rounds the limit up to whole descriptors (4 × 4092 = 16,368 B), and every flush is at most one draw buffer (15,680 B), so each flush stays one transfer.
- **Settings modal animation:** the overlay slides in over the last 60 px (`MODAL_SLIDE_DISTANCE`) in 180 ms with ease-out, and out the same way with ease-in, instead of the full screen height in 300 ms with ease-in-out. Every animation frame redraws almost the whole screen, so the long slide showed only a few frames and looked slow.

### Fixed
- **Powersave screen showed stale sensor values:** while the powersave screen was not shown, `update_powersave_icons()` stored new HR/CSC values in its change cache without drawing them. On the next enter the values matched the cache, so the labels kept the values from the previous powersave until the value changed (for example cadence 0 or a disconnected sensor stayed wrong). The cache now only holds what the labels show. Fan counts were not affected.
- **Touch felt sticky:** LVGL read the touch every 50 ms (`LV_INDEV_DEF_READ_PERIOD`), and the read callback skipped two of three reads, so the FT3168 was polled only about every 150 ms; releases came late and short taps could be lost. The library now sets its own touch read period to 20 ms (`TOUCH_READ_PERIOD_MS`) and polls on every read. The global `lv_conf.h` is unchanged.
- **Touch read errors:** `getTouch()` evaluated an uninitialised status byte when the I2C read failed and could report a press at random coordinates; it now reports no touch. `I2C_read_buff()` and `I2C_writr_buff()` return `esp_err_t` instead of a truncated `uint8_t`.
- **Main screen froze after waking from powersave with the settings modal open:** `homewind_hide_settings_overlay()` hid the overlay but left the modal marked as visible, so all HR, cadence and fan updates were skipped until the modal was opened and closed again. It now stops the slide animation and clears the modal state.

---

## [1.6.0] - 2026-11-07

### Added
- **API**: `homewind_set_csc_power(int32_t watts)` shows power on the cadence card. `watts < 0` hides it (status line as before, e.g. "85 RPM"); `watts >= 0` shows power and cadence, e.g. "210 W 85 rpm". Inter Bold 24 has no middle dot, and "1999 W 199 rpm" is the widest text that fits the 189 px label, hence no separator and lowercase "rpm". The string is cached like the cadence string. The powersave screen still shows the cadence number only.

---

## [1.5.10] - 2026-10-29

### Fixed
- **Board V1.1 black display**: V1.1 moves `LCD_CS` from GPIO 9 to GPIO 46 (swapped with `IMU_INT1`). `lcd_lvgl_Init()` now detects the revision via `LCD_TE` (GPIO 45, V1.1 only): panel started with CS 46, TE pulses counted for 100 ms, ≥3 pulses = V1.1, otherwise restart with CS 9 (V1). Details in `docs/fix.md`.
- **QMI8658**: `INT1`/`INT2` outputs no longer enabled (IMU is polled over I2C). A soft reset before detection releases `INT1` after ESP-only restarts.

### Added
- **API**: `homewind_get_board_revision()`, `homewind_get_board_revision_name()`, `homewind_get_te_pulse_count()` (`lcd_get_*` in `lcd_bsp.h`).

### Changed
- **lcd_config.h**: `EXAMPLE_PIN_NUM_LCD_CS` replaced by `EXAMPLE_PIN_NUM_LCD_CS_V1` (9), `EXAMPLE_PIN_NUM_LCD_CS_V1_1` (46), plus `EXAMPLE_PIN_NUM_LCD_TE` (45), `LCD_TE_PROBE_MS`, `LCD_TE_MIN_PULSES`.
- **Startup**: about 100 ms longer on V1.1, about 350 ms longer on V1 (second panel start).

---

## [1.5.9] - 2026-02-20

### Added
- **Boot Screen**: "Warming Up" screen shown from display init until PH_RUNNING (heart icon, centered)
- **AP Screen**: "Wifisetup Mode" screen when WiFi Captive Portal active (heart icon, centered text)
- **API**: `homewind_show_ap_screen()`, `homewind_show_main_screen()` for DisplayManager integration
- **Powersave Lock**: `powersave_lock()` / `powersave_unlock()` for Boot/AP screens – prevents dimming during setup

### Changed
- **should_skip_main_screen_update()**: Now checks `lv_scr_act() != scr_main` (Boot/AP/Powersave) before main screen updates
- **homewind_create_screens()**: Creates Boot + AP screens first, loads Boot as initial screen

### Result
- No breaking API changes; compatible with Homewind Boot/AP integration

---

## [1.5.8] - 2026-02-02

### Changed (BREAKING)
- **HR/CSC State Enums:** Umbenennung für klarere Semantik
  - `HR_STATE_INACTIVE` → `HR_NOT_CONFIGURATED` (nicht konfiguriert)
  - `HR_STATE_ERROR` → `HR_STATE_INACTIVE` (konfiguriert, nicht verbunden)
  - `CSC_STATE_INACTIVE` → `CSC_NOT_CONFIGURATED` (nicht konfiguriert)
  - `CSC_STATE_ERROR` → `CSC_STATE_INACTIVE` (konfiguriert, nicht verbunden)
- **Docs:** Migration guide in `docs/MIGRATION_1.5.7_to_1.5.8.md`

### Migration
```
HR_STATE_INACTIVE   →  HR_NOT_CONFIGURATED
HR_STATE_ERROR      →  HR_STATE_INACTIVE
CSC_STATE_INACTIVE  →  CSC_NOT_CONFIGURATED
CSC_STATE_ERROR     →  CSC_STATE_INACTIVE
```

---

## [1.5.7] - 2026-02-02

### Changed
- **Powersave Fan-Anzeige:** Anzeige entspricht jetzt der Lüfteranzahl im UI
  - Kein Lüfter konfiguriert → `--`
  - Mindestens ein Lüfter konfiguriert → `(aktiv/konfiguriert)` z. B. `(1/2)` (Anzahl „an“ / Anzahl konfigurierter Slots)
  - Gesamtanzahl ist nicht mehr fest 4, sondern Anzahl konfigurierter Lüfter; „aktiv“ = Lüfter mit `is_on`
- **Powersave Update:** Anzeige aktualisiert sich sofort bei Fan-Änderungen (`homewind_set_fan`, `homewind_set_fan_state`, `homewind_set_fan_toggle` rufen intern `update_powersave_display()` auf)

### Result
- Keine API-Änderungen; abwärtskompatibel

---

## [1.5.6] - 2026-02-02

### Changed
- **Refactor:** Gemeinsame Helper (`ui_colors.h`, `ui_style_helpers.h`) für Farben/Styles
- **UI Update Guards:** Zentraler Guard `should_skip_main_screen_update()` + HR-Widget Cache
- **Powersave:** Slot-Factory + String-Caching (weniger `snprintf`/Stack-Overhead)
- **LVGL Timing:** Max-Delay 200 ms, Touch-Polling ~66 Hz
- **PSRAM Opt-in:** `HOMEWIND_LVGL_PSRAM` aktiviert `lv_conf_psram_auto.h`

### Fixed
- **Stability:** LVGL Buffer wieder auf 1/8 und Double-Buffer; Task-Stack wieder 4 KB

### Result
- Weniger Duplikate, weniger LVGL-Calls, stabiler Stack; keine API-Änderungen

---

## [1.5.5] - 2026-02-02

### Changed
- **Fan item (ACTIVE):** Label links, Punkt rechts – Reihenfolge per `lv_obj_move_foreground` (Label links, Kreis rechts im ACTIVE-Zustand; INACTIVE unverändert: Kreis links, Label rechts)
- **Fan item Label:** 4 px nach unten (pad_top) für bessere optische Ausrichtung
- **Docs:** Homewind_Implementation.md – Abschnitte 5 (Powersave) und 6 (Kurzüberblick) in korrekter Reihenfolge

### Result
- Keine API-Änderungen; abwärtskompatibel

---

## [1.5.4] - 2026-01-30

### Added
- **PSRAM for LVGL:** Snippet `extras/lv_conf_psram.h` for custom allocator (heap_caps_* with MALLOC_CAP_SPIRAM)
  - Include from project `lv_conf.h` or copy defines; PSRAM.md step-by-step instructions
  - Saves internal RAM (~5–15 KB+); optional README/DOCUMENTATION/ISSUES references

### Fixed
- **FullFeaturesHeapDebug:** Heap stats now use internal RAM only (MALLOC_CAP_INTERNAL) for free, largest, frag
  - Prevents wrong fragmentation (e.g. −2903%) when LVGL uses PSRAM (MALLOC_CAP_8BIT mixed heaps)
  - Optional second line: `free_psram=…`, `largest_psram=…` when PSRAM present

### Result
- Correct heap/frag reporting with PSRAM; optional LVGL heap in PSRAM for more free internal RAM
- No API changes; backwards compatible

---

## [1.5.3] - 2026-01-30

### Changed
- **Display buffer:** `EXAMPLE_LVGL_BUF_HEIGHT` from 1/4 to 1/8 screen height (57 lines, ~64 KB total)
  - Saves ~64 KB internal RAM; measured: free +64.5 KB, largest +65.5 KB, frag 26.9% → 18.8%
- **qmi8658:** IMU debug log (`qmi8658_log` = printf) disabled by default
  - Define `QMI8658_DEBUG` in build to get WhoAmI/Ctrl/cali messages on Serial

### Added
- **Docs:** PSRAM.md (LVGL/PSRAM options), Homewind_Implementation.md (Homewind integration)
- **Example:** FullFeaturesHeapDebug (heap log every 30s: free, largest, frag, drift, minFree)

### Result
- More free heap, lower fragmentation; Serial output cleaner without IMU debug
- No API changes; backwards compatible

---

## [1.5.2] - 2026-01-30

### Fixed
- **Powersave Timer:** Now resets on ANY touch on main screen (fan toggles, settings button, etc.)
  - Added `main_screen_touch_cb()` registered with `LV_EVENT_PRESSING` on `scr_main`
  - Previously only reset when touching dimming overlay or powersave container

- **UI Updates While Modal Open:** Skip LVGL updates when settings modal is visible
  - Added `settings_modal_visible` and `ui_refresh_pending` static flags
  - Update functions now check visibility before making LVGL calls
  - UI automatically refreshes when modal closes if values changed

- **Dimming Overlay:** Added visibility check before modifying overlay state
  - Uses `lv_obj_has_flag()` to prevent redundant LVGL calls

### Result
- Better user experience (no dimming while actively using UI)
- Lower CPU usage when modal is open
- **No heap allocation** – all fixes use static variables and callbacks

---

## [1.5.1] - 2026-01-30

### Removed
- **FT3168:** Unused `I2C_master_write_read_device()` (never called in codebase)
- **qmi8658c:** Unused `qmi8658c_example()` (IMU is initialized via `lcd_start_imu_rotation_task()`)
- **qmi8658c:** Removed public declaration of `qmi8658c_example()` from header

### Changed
- **qmi8658c:** Disabled `QMI8658_USE_FIFO` – FIFO code was compiled but never used

### Result
- ~2.6 KB smaller flash footprint
- No API or behavior changes; fully backwards compatible

---

## [1.5.0] - Previous

### Added
- O1 Change-Guards: All `homewind_set_*` APIs skip LVGL updates when values unchanged
- O3 Style Reuse: Pre-defined `lv_style_t` for CSC states
- O3 UI State Cache: Fan widget caches previous state to skip redundant updates

### Changed
- Breathing animation only writes backlight when value changes
- ~80% fewer LVGL calls when sensor values are stable

---

## [1.4.0]

- New clean API: `homewind_set_hr()`, `homewind_set_csc()`, `homewind_set_fan()` and variants
- Touch polling rate limited to 50Hz
- String formatting cached; legacy macros kept for compatibility

## [1.3.0]

- I2C stack buffers; fan animations use static allocation
- Heap stability improvements

## [1.2.0]

- LVGL tick 2ms → 5ms; IMU rotation hysteresis; powersave dirty-checking

## [1.1.0]

- Idempotent `homewind_init()`; IMU timing fixes; C++ linkage fixes

## [1.0.0]

- Initial release: HR, CSC, Fan widgets, power save, touch, QR settings
