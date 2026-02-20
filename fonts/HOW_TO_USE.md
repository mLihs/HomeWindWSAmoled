# Font-Generierung – How to Use

Das Skript `generate_fonts.sh` erzeugt LVGL-kompatible C-Fonts aus TTF-Dateien.

## Verwendetes Tool

**`lv_font_conv`** – der offizielle [LVGL Font Converter](https://github.com/lvgl/lv_font_conv).  
Er wandelt TrueType-Fonts (TTF) in LVGL-Format (`.c`-Dateien mit `lv_font_t`) um.

### Installation von lv_font_conv

**Option A – npm (empfohlen):**
```bash
npm install -g lv_font_conv
```

**Option B – von Quellen (Node.js nötig):**
```bash
git clone https://github.com/lvgl/lv_font_conv.git
cd lv_font_conv
npm install
# Nutzung z.B. mit: node lv_font_conv.js ...
```

Ohne globalen npm-Install nutzt du den Aufruf aus dem Klon:
```bash
node /pfad/zu/lv_font_conv/bin/lv_font_conv.js ...
```

## Ablauf

1. **Voraussetzungen**
   - `lv_font_conv` im PATH (oder Skript anpassen, siehe oben)
   - TTF-Dateien vorhanden:
     - `Inter/Inter_18pt-Black.ttf`, `Inter_18pt-Bold.ttf`, `Inter_18pt-Regular.ttf`
     - `FanFont.ttf` (im selben Ordner wie das Skript)

2. **Skript ausführen** (im Repo-Root, sodass der Ordner `fonts` stimmt):
   ```bash
   cd /pfad/zu/HomeWindWSAmoled/fonts
   ./generate_fonts.sh
   ```
   Oder von einem anderen Ort mit vollem Pfad:
   ```bash
   bash /pfad/zu/HomeWindWSAmoled/fonts/generate_fonts.sh
   ```
   Das Skript legt `fonts/Inter_LVGL/` an und schreibt dort die `.c`-Dateien.

3. **Ergebnis**
   - Alle generierten Fonts liegen in `fonts/Inter_LVGL/`.
   - Die Symbolnamen in den `.c`-Dateien werden per `sed` auf die gewünschten Namen gepatcht (z.B. `lv_font_inter_bold_20`).

## Zeichen pro Font (welche Zeichen werden konvertiert)

| Font-Datei | Quelle (TTF) | Größe | `--range` | Zeichen (konvertiert) |
|------------|--------------|-------|-----------|------------------------|
| `lv_font_inter_black_64.c` | Inter_18pt-Black.ttf | 64px | 48–57 | **Ziffern:** `0` `1` `2` `3` `4` `5` `6` `7` `8` `9` |
| `lv_font_inter_bold_14.c` | Inter_18pt-Bold.ttf | 14px | 32, 48–57, 65–90, 97–122 | **Nur:** Space, `0`–`9`, `A`–`Z`, `a`–`z` (keine Satzzeichen/Sonderzeichen) |
| `lv_font_inter_bold_16.c` | Inter_18pt-Bold.ttf | 16px | 32, 48–57, 65–90, 97–122 | wie oben |
| `lv_font_inter_bold_20.c` | Inter_18pt-Bold.ttf | 20px | 32, 48–57, 65–90, 97–122 | wie oben |
| `lv_font_inter_bold_24.c` | Inter_18pt-Bold.ttf | 24px | 32, 48–57, 65–90, 97–122 | wie oben |
| `lv_font_inter_regular_20.c` | Inter_18pt-Regular.ttf | 20px | 32, 48–57, 65–90, 97–122 | wie oben |
| `lv_font_inter_regular_24.c` | Inter_18pt-Regular.ttf | 24px | 32, 48–57, 65–90, 97–122 | wie oben |
| `lv_font_icon_56.c` | FanFont.ttf | 56px | 48–57 | **Ziffern (als Icons):** `0`–`9` |
| `lv_font_icon_36.c` | FanFont.ttf | 32px | 48–57 | **Ziffern (als Icons):** `0`–`9` |

### Range-Erklärung

- **48–57** = ASCII-Codes für die Zeichen `0` (48) bis `9` (57).
- **32, 48–57, 65–90, 97–122** (Inter Bold/Regular): nur **Leerzeichen**, **0–9**, **A–Z**, **a–z** (keine Satzzeichen/Sonderzeichen wie `!` `,` `.` `:` usw.) – spart Glyphen und Speicher.
- Früher **32–126** = volles druckbares ASCII (inkl. Satzzeichen); bei Bedarf wieder auf `--range 32-126` stellen.

## Optionen, die das Skript an lv_font_conv übergibt

- `--font` – Quell-TTF
- `--size` – Font-Größe in Pixel
- `--bpp` – 4 (Bits pro Pixel für Antialiasing)
- `--format lvgl` – Ausgabe als LVGL-C-Font
- `--range` – Zeichenbereich (inklusive)
- `--no-compress` – keine RLE-Kompression (einfacher, etwas größer)
- `--output` – Ziel-`.c`-Datei

Nach der Generierung patcht das Skript in jeder `.c`-Datei den Bezeichner auf den gewünschten Font-Namen (z.B. `lv_font_inter_bold_20`).

## Verwendung in LVGL

Nach dem Generieren die Fonts in deinem Code deklarieren, z.B.:

```c
LV_FONT_DECLARE(lv_font_inter_bold_20);
LV_FONT_DECLARE(lv_font_inter_black_64);
LV_FONT_DECLARE(lv_font_icon_56);
```

Dann z.B. einem Label zuweisen:

```c
lv_obj_set_style_text_font(label, &lv_font_inter_bold_20, 0);
```
