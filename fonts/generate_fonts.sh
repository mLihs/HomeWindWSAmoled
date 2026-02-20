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
# Bold 14 (Space, -, /, 0-9, A-Z, a-z – "-" für "--", "/" für "0/4")
#####################################
FILE="$OUTDIR/lv_font_inter_bold_14.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 14 \
  --bpp 4 \
  --format lvgl \
  --range 32,45,47,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_14"
patch_lvgl_include "$FILE"





#####################################
# Bold 24 (Space, -, /, 0-9, A-Z, a-z – "-" für "--", "/" für "0/4")
#####################################
FILE="$OUTDIR/lv_font_inter_bold_24.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --range 32,45,47,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_24"
patch_lvgl_include "$FILE"


#####################################
# 64px Icons Only – 123
#####################################
FILE="$OUTDIR/lv_font_icon_56.c"
lv_font_conv \
  --font "FanFont.ttf" \
  --size 56 \
  --bpp 4 \
  --format lvgl \
  --range 48-57 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_icon_56"
patch_lvgl_include "$FILE"


#####################################
# 36px Icons Only – 123
#####################################
FILE="$OUTDIR/lv_font_icon_36.c"
lv_font_conv \
  --font "FanFont.ttf" \
  --size 32 \
  --bpp 4 \
  --format lvgl \
  --range 48-57 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_icon_36"
patch_lvgl_include "$FILE"


echo ""
echo "🎉 DONE — All fonts generated + name-patched."
echo "   Declare them in LVGL with:"
echo "       LV_FONT_DECLARE(lv_font_inter_bold_24);"
