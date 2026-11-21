#!/bin/bash

OUTDIR="fonts/Inter_LVGL"
mkdir -p "$OUTDIR"

echo "🔠 Generating Inter LVGL fonts (auto-patched names)…"
echo ""

#####################################
# Helper: patch font name
#####################################
patch_font_name() {
  local file="$1"
  local symbol="$2"

  # Patch the final font symbol name inside the .c file:
  sed -i '' "s/const lv_font_t .*/const lv_font_t ${symbol} = {/" "$file"
  echo "  → Patched symbol name: $symbol"
}

#####################################
# Helper: replace conditional LVGL include with plain #include "lvgl.h"
#####################################
patch_lvgl_include() {
  local file="$1"
  perl -i -0pe 's/#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n#include "lvgl.h"\n#else\n#include "lvgl\/lvgl.h"\n#endif/#include "lvgl.h"/g' "$file"
  echo "  → Patched include to #include \"lvgl.h\""
}

#####################################
# 64px Numbers Only – Black
#####################################
FILE="$OUTDIR/lv_font_inter_black_64.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Black.ttf" \
  --size 64 \
  --bpp 4 \
  --format lvgl \
  --range 48-57 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_black_64"
patch_lvgl_include "$FILE"


#####################################
# Bold 14 und 24: druckbares ASCII ohne $ ( ) [ ] ` { } | – diese Zeichen
# vergrößern die Zeilenhöhe (14 → 15/18, 23 → 24/29) und damit das Layout.
# Sensornamen kommen aus dem BLE-Advertising (z. B. "HRM-Pro:123456").
#####################################
NAME_RANGE="32-35,37-39,42-90,92,94-95,97-122,126"

#####################################
# Bold 14 (Namen in den Sensorzeilen)
#####################################
FILE="$OUTDIR/lv_font_inter_bold_14.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 14 \
  --bpp 4 \
  --format lvgl \
  --range "$NAME_RANGE" \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_14"
patch_lvgl_include "$FILE"





#####################################
# Bold 24 (HR-Name, Werte, Screensaver)
#####################################
FILE="$OUTDIR/lv_font_inter_bold_24.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --range "$NAME_RANGE" \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_24"
patch_lvgl_include "$FILE"


#####################################
# 56px Icons – 1 Herz, 2 Haken, 3 !, 4 Herz getrennt, 5 Kurbel, 6 Lüfter, 9 Blitz (Screensaver)
#####################################
FILE="$OUTDIR/lv_font_icon_56.c"
lv_font_conv \
  --font "FanFont.ttf" \
  --size 56 \
  --bpp 4 \
  --format lvgl \
  --range 49-54,57 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_icon_56"
patch_lvgl_include "$FILE"


#####################################
# 36px Icons – 1-6 wie 56, 7 plus, 8 minus, 9 Blitz, : Zahnrad (add_icons.py)
#####################################
FILE="$OUTDIR/lv_font_icon_36.c"
lv_font_conv \
  --font "FanFont.ttf" \
  --size 32 \
  --bpp 4 \
  --format lvgl \
  --range 49-58 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_icon_36"
patch_lvgl_include "$FILE"


echo ""
echo "🎉 DONE — All fonts generated + name-patched."
echo "   Declare them in LVGL with:"
echo "       LV_FONT_DECLARE(lv_font_inter_bold_24);"
