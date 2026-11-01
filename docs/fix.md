# Display V1 und V1.1 mit einer Firmware

## Problem

Auf dem Waveshare ESP32-S3-Touch-AMOLED-1.64 bleibt das Display mit der Homewind-Touch-Firmware schwarz, obwohl das Webinterface, WLAN und BLE normal starten. Die Werksfirmware zeigt auf demselben Board ein Bild.

Betroffen sind Platinen der Revision V1.1. Auf V1 erscheint die Homewind-Oberfläche.

Homewind setzt den Display-Chip-Select fest auf GPIO 9 (`EXAMPLE_PIN_NUM_LCD_CS` in `src/lcd_config.h`). Auf der V1.1 liegt Chip-Select auf GPIO 46. Die Init-Befehle erreichen das Panel nicht.

Takt, Datenleitungen und Reset sind auf beiden Revisionen gleich:

| Signal | GPIO |
|---|---|
| LCD_PCLK | 10 |
| LCD_DATA0–3 | 11–14 |
| LCD_RST | 21 |

| | V1 | V1.1 |
|---|---|---|
| LCD_CS | GPIO 9 | GPIO 46 |
| IMU_INT1 | GPIO 46 | GPIO 9 |
| LCD_TE | nicht vorhanden | GPIO 45 |

Der Aufdruck steht bei V1 oben auf der Platine, bei V1.1 rechts neben der Stiftleiste.

SH8601 und CO5300 sind beides QSPI-AMOLED-Treiber für 280×456, RGB565, ohne Backlight-Pin. Die Helligkeit läuft über Register `0x51`. Die Init in `src/lcd_bsp.c` (`0x11`, `0xC4 0x80`, `0x35`, `0x53`, `0x63`, `0x51`, `0x29`) ist die Waveshare-Sequenz für dieses Modul und läuft über den SH8601-Treiber. Waveshares V1.1-Beispiele verwenden dieselbe Sequenz und denselben Treiber, nur mit `EXAMPLE_PIN_NUM_LCD_CS` auf GPIO 46. Eine andere Init-Sequenz löst die schwarze V1.1 nicht. Der Chip-Select ist die Differenz, die das Bild verhindert.

Waveshare trennt die Revisionen über getrennte Beispielpakete (V1 und [V1.1](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.64-v2)) mit fest eingetragenem Chip-Select. Eine Erkennung zur Laufzeit gibt es dort nicht, und `LCD_TE` wird in den Beispielen nicht ausgewertet.

Touch (FT3168) und Lagesensor (QMI8658) liest Homewind per I2C. Die auf der V1.1 zusätzlich herausgeführten Interrupt-Leitungen (`TP_INT` GPIO 18, `IMU_INT2` GPIO 17) bleiben dafür unbenutzt. `qmi8658c.cpp` schaltet die Interrupt-Ausgänge `INT1` und `INT2` des Sensors trotzdem ein. Der Sensor treibt `IMU_INT1` also aktiv, auf der V1 an GPIO 46. Die Einstellung bleibt über einen Software-Reset des ESP erhalten.

## Lösung

Eine Touch-Firmware. Vor dem LVGL-Start in `lcd_lvgl_Init()` den Chip-Select erkennen und nur diese Leitung umschalten. Auflösung, Init-Sequenz und der übrige QSPI-Bus bleiben.

Die Erkennung nutzt `LCD_TE` an GPIO 45. Den Anschluss hat nur die V1.1, und die bestehende Init schaltet Tearing Effect mit `0x35` bereits ein. Das Panel liefert TE erst, wenn die Init angekommen ist. Flanken an GPIO 45 bestätigen deshalb Revision und Chip-Select zugleich.

Ein Test mit Chip-Select GPIO 9 sagt nichts aus: Die V1 hat kein TE, und auf der V1.1 erreicht die Init das Panel nicht. Geprüft wird darum nur GPIO 46.

1. Lagesensor stilllegen. Der QMI8658 hängt auf beiden Revisionen am selben I2C-Bus (SDA 47, SCL 48, Adresse `0x6B`). `Touch_Init()` richtet diesen Bus vor `lcd_lvgl_Init()` ein, der Zugriff ist also schon möglich. Soft-Reset wie in `qmi8658_on_demand_cali()`: `qmi8658_write_reg(Qmi8658Register_Reset, 0xb0)`, dann 10 ms warten. Nach dem Reset sind `INT1_EN` und `INT2_EN` in `CTRL1` gelöscht und die Pins sollten hochohmig sein. Das am Datenblatt bestätigen oder an GPIO 46 einer V1 messen.
2. Den SPI-Bus einmal aufbauen (PCLK und DATA0–3 unverändert). Er bleibt für den gesamten Ablauf bestehen.
3. Panel-IO mit Chip-Select GPIO 46 anlegen, Reset, bestehende Init. Danach TE-Impulse an GPIO 45 etwa 100 ms lang zählen.
4. Mindestens drei Impulse: es ist eine V1.1. Chip-Select 46 behalten.
5. Weniger: es ist eine V1. Panel und Panel-IO löschen (`esp_lcd_panel_del`, `esp_lcd_panel_io_del`), GPIO 46 mit `gpio_reset_pin` freigeben und GPIO 45 ebenso. Panel-IO mit Chip-Select GPIO 9 neu anlegen, Reset, bestehende Init.
6. LVGL wie bisher starten.

Die TE-Impulse sind kurz, deutlich unter einer Millisekunde je Bild. Eine Abfrage mit `vTaskDelay` dazwischen verpasst sie. GPIO 45 deshalb als Eingang mit Pulldown und Interrupt auf steigende Flanke einrichten und die Flanken im Interrupt zählen. Ist der ISR-Dienst schon installiert, liefert `gpio_install_isr_service` `ESP_ERR_INVALID_STATE`. Das ist kein Fehler. Nach der Messung Handler entfernen und Pin zurücksetzen. Die Schwelle von drei Impulsen verhindert, dass eine einzelne Störung auf dem offenen Pin der V1 als V1.1 gilt. Bei rund 60 Hz kommen in 100 ms etwa sechs Impulse.

Panel-Reset (160 ms im Treiber) und Init (etwa 90 ms) laufen pro Versuch. Die Erkennung kostet damit auf der V1.1 etwa 100 ms zusätzlich zum heutigen Start, auf der V1 etwa 350 ms. Sie läuft bei jedem Boot, ohne NVS-Cache. So bleibt nach einem Platinentausch kein veralteter Wert stehen.

Versagt die Erkennung auf einer V1.1, fällt die Firmware auf Chip-Select 9 zurück. Das Display bleibt dann schwarz wie heute. Schlechter als der jetzige Stand wird keine Platine. Einen manuellen Umschalter gibt es bewusst nicht: WLAN und Webinterface laufen auch bei schwarzem Display, und die Korrektur kommt per Firmware-Update.

Zur Diagnose wird die erkannte Revision angezeigt:

- Webinterface, Systembereich: „Firmware: x.y.z · Board V1.1“. Das Board hängt optional am WebSocket-Frame `FIRMWARE_VERSION`. Ältere Oberflächen lesen nur die Version.
- `GET /api/v1/system/info`: `board=V1.1` und `te_pulses=6`.
- Serielles Log: `[Display] Initialized (board V1.1, TE pulses 6)`.

Erwartete Impulszahl: V1 gleich 0, V1.1 etwa 6.

Zusätzlich die Interrupt-Ausgänge in `qmi8658c.cpp` nicht mehr einschalten. Homewind fragt den Sensor per I2C ab, und der Sensor treibt dann keine Leitung, die auf der anderen Revision zum Display gehört.

GPIO 45 und 46 sind Strapping-Pins des ESP32-S3. Nach dem Boot sind beide frei nutzbar. GPIO 45 wählt beim Chip-Reset die Flash-Spannung. Auf der V1.1 bleibt das Panel beim Druck auf den Reset-Knopf eingeschaltet und sendet weiter TE-Impulse. Trifft der Reset genau einen Impuls, startet der ESP mit falscher Flash-Spannung. Das betrifft die Hardware der V1.1 auch mit der Waveshare-Firmware und entsteht nicht durch diese Lösung. Mit `espefuse.py summary` prüfen, ob `VDD_SPI_FORCE` gesetzt ist. Dann ignoriert der Chip den Pin.

## Umsetzung

- `src/lcd_config.h`: `EXAMPLE_PIN_NUM_LCD_CS_V1` (9), `EXAMPLE_PIN_NUM_LCD_CS_V1_1` (46), `EXAMPLE_PIN_NUM_LCD_TE` (45), `LCD_TE_PROBE_MS`, `LCD_TE_MIN_PULSES`.
- `src/lcd_bsp.c`: Erkennung in `lcd_lvgl_Init()`, Abfrage über `lcd_get_board_revision()`, `lcd_get_board_revision_name()`, `lcd_get_te_pulse_count()`. In `HomeWindWSAmoled.h` als `homewind_get_board_revision*()` und `homewind_get_te_pulse_count()`.
- `src/qmi8658c.cpp`: `CTRL1` ohne `INT1_EN` und `INT2_EN`.
- Homewind: `DisplayManager::boardRevision()`, Ausgabe in `WebSocketTelemetry.cpp`, `ApiActions.cpp` und `webui_src/app.js`.

## Test

Auf einer V1 (vorhanden):

1. Log und Webinterface zeigen „V1“ mit 0 Impulsen.
2. Display, Touch und automatische Drehung funktionieren.
3. Gleiches Ergebnis nach Stromtrennung, Reset-Knopf und Neustart nach Firmware-Update.

Auf einer V1.1 (noch nicht verfügbar): „V1.1“ mit etwa 6 Impulsen und Bild auf dem Display.

Offen: Die V1.1 führt zusätzlich `TP_RESET` heraus. Waveshares V1.1-Beispiele steuern ihn nicht an. Ob der Touch ohne aktiven Reset zuverlässig startet, auf einer V1.1 prüfen.
