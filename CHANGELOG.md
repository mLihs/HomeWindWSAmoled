# Changelog

All notable changes to the HomeWindWSAmoled library are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
