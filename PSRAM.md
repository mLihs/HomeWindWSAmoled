# LVGL-Ressourcen im PSRAM

LVGL profitiert stark von externem Speicher; Grafiken und Objekte können viel Platz belegen. Diese Seite beschreibt die Optionen und die aktuelle Bibliothek-Konfiguration.

---

## 1. Display-Buffer im PSRAM

**Allgemein (LVGL/ESP32):**  
Bei großen Displays (z. B. 480×320 oder höher) können ein oder beide Framebuffer in PSRAM liegen. Das ermöglicht z. B. den **Direct Mode**, erfordert aber ausreichende Bandbreite.

- **Empfehlung:** PSRAM mit **80 MHz** Takt betreiben.
- **Einschränkung:** SPI-DMA kann auf vielen ESP32-Setups nur aus **internem RAM** lesen. Wenn der Flush-Callback den Buffer per SPI-DMA zum Display schickt, muss die Quelle DMA-fähig sein (internes RAM).

**In dieser Bibliothek (HomeWindWSAmoled):**

- Es werden **partielle** Double-Buffer verwendet (`EXAMPLE_LVGL_BUF_HEIGHT = V_RES/4`, also 114 Zeilen), keine vollen Framebuffer.
- Die Buffer werden in `lcd_bsp.c` mit `heap_caps_malloc(..., MALLOC_CAP_DMA)` allokiert, weil der esp_lcd SPI/QSPI-Flush per DMA aus dem Buffer liest.
- **Aktuell:** Display-Buffer bleiben im **internen RAM** (DMA-Pflicht für diesen Treiber).
- **Möglich bei Refactor:** Voll-Framebuffer im PSRAM + anderer Flush-Weg (z. B. Copy in kleinen DMA-Blöcken oder anderer Controller-Mode) – dann 80 MHz PSRAM beachten.

---

## 2. LVGL-Memory-Pool im PSRAM

Der gesamte Heap für LVGL-Objekte (Widgets, Styles, etc.) kann in PSRAM liegen. Das spart internes RAM (~5–15 KB+); die UI kann minimal träger werden.

### PSRAM für LVGL ausprobieren (Schritt für Schritt)

1. **lv_conf.h im Projekt**  
   Deine LVGL-Konfiguration liegt in der Regel in einer `lv_conf.h` im Sketch-Ordner oder im Include-Pfad des Projekts (nicht in dieser Bibliothek).

2. **Snippet einbinden** (empfohlen)  
   Die Bibliothek liefert eine fertige Snippet-Datei:
   - **Pfad:** `extras/lv_conf_psram.h` (relativ zum Bibliotheksordner `HomeWindWSAmoled`).
   - In deiner **lv_conf.h** ganz oben (vor anderen `LV_MEM_*`-Defines) einfügen:
     ```c
     #include "extras/lv_conf_psram.h"
     ```
   - Den Include-Pfad ggf. anpassen (z. B. `libraries/HomeWindWSAmoled/extras/lv_conf_psram.h` oder absoluter Pfad), je nachdem, wo deine `lv_conf.h` liegt.

3. **Oder Defines manuell setzen**  
   Wenn du nichts einbinden willst, in deiner **lv_conf.h** die folgenden Zeilen setzen (und sicherstellen, dass `LV_MEM_CUSTOM` nicht später wieder auf 0 gesetzt wird):
   ```c
   #define LV_MEM_CUSTOM 1
   #define LV_MEM_CUSTOM_INCLUDE "esp_heap_caps.h"
   #define LV_MEM_CUSTOM_ALLOC(size)     heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
   #define LV_MEM_CUSTOM_FREE(ptr)       heap_caps_free(ptr)
   #define LV_MEM_CUSTOM_REALLOC(ptr, size)  heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM)
   ```

4. **Projekt bauen und testen**  
   - Nur mit **PSRAM-fähigem ESP32** (z. B. ESP32-S3, ESP32-P4 oder ESP32 mit SPIRAM) verwenden.
   - Bauen, flashen, UI prüfen. Bei knappem internem RAM sollte der freie interne Heap steigen; bei Bedarf mit dem Beispiel `FullFeaturesHeapDebug` messen.

5. **Hinweis**  
   Mit PSRAM-Allokator kann die UI etwas träger werden. Bei knappem internem RAM lohnt sich der Trade-off in der Regel.

**In dieser Bibliothek:** Es gibt keine eigene `lv_conf.h`; die LVGL-Konfiguration kommt aus dem Sketch/Projekt. Wer den LVGL-Heap in PSRAM legen will, nutzt die obige Snippet-Datei oder die Defines in seiner Projekt-`lv_conf.h`.

---

## 3. Bilder und Schriftarten im PSRAM

- **Große Bitmaps** oder **komplexe Fonts** können:
  - direkt in PSRAM geladen werden, oder
  - von Flash nach PSRAM kopiert werden,
  um die **Rendergeschwindigkeit** gegenüber reinem Flash-Zugriff zu erhöhen.

**In dieser Bibliothek:**  
Schriftarten liegen als `const` in den Font-`.c`-Dateien (Flash). Eine Auslagerung nach PSRAM wäre ein optionaler Optimierungsschritt (z. B. beim Start kopieren), wenn Font-Zugriffe als Flaschenhals messbar sind.

---

## Kurzüberblick

| Ressource              | PSRAM möglich? | In dieser Lib aktuell | Anmerkung |
|------------------------|----------------|------------------------|-----------|
| Display-Buffer        | Ja*, wenn kein DMA nötig / anderer Flush | Nein (DMA) | *Full-FB + 80 MHz PSRAM; hier: partielle DMA-Buffer |
| LVGL-Memory-Pool      | Ja             | Konfiguration im Projekt (lv_conf.h) | `LV_MEM_CUSTOM` + PSRAM-Allokator |
| Bilder / große Fonts   | Ja             | Fonts im Flash         | Optional: Copy nach PSRAM für schnelleres Rendering |

---

*Stand: 2026-01-30*
