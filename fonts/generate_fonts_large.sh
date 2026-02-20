#!/bin/bash

OUTDIR="fonts/Inter_LVGL"
mkdir -p "$OUTDIR"

echo "🔠 Generating Inter LVGL fonts (auto-patched names)…"
echo ""

#####################################
# Helper function: patch font name
#####################################
patch_font_name() {
  local file="$1"
  local symbol="$2"

  # Patch the final font symbol name inside the .c file:
  sed -i '' "s/const lv_font_t .*/const lv_font_t ${symbol} = {/" "$file"
  echo "  → Patched symbol name: $symbol"
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


#####################################
# Bold 14 (Space, 0-9, A-Z, a-z only – no punctuation/symbols)
#####################################
FILE="$OUTDIR/lv_font_inter_bold_14.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 14 \
  --bpp 4 \
  --format lvgl \
  --range 32,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_14"


#####################################
# Bold 16
#####################################
FILE="$OUTDIR/lv_font_inter_bold_16.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 16 \
  --bpp 4 \
  --format lvgl \
  --range 32,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_16"


#####################################
# Bold 20
#####################################
FILE="$OUTDIR/lv_font_inter_bold_20.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 20 \
  --bpp 4 \
  --format lvgl \
  --range 32,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_20"


#####################################
# Bold 24
#####################################
FILE="$OUTDIR/lv_font_inter_bold_24.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --range 32,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_bold_24"


#####################################
# Regular 20
#####################################
FILE="$OUTDIR/lv_font_inter_regular_20.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Regular.ttf" \
  --size 20 \
  --bpp 4 \
  --format lvgl \
  --range 32,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_regular_20"


#####################################
# Regular 24
#####################################
FILE="$OUTDIR/lv_font_inter_regular_24.c"
lv_font_conv \
  --font "Inter/Inter_18pt-Regular.ttf" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --range 32,48-57,65-90,97-122 \
  --no-compress \
  --output "$FILE"

patch_font_name "$FILE" "lv_font_inter_regular_24"




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


echo ""
echo "🎉 DONE — All fonts generated + name-patched."
echo "   Declare them in LVGL with:"
echo "       LV_FONT_DECLARE(lv_font_inter_bold_20);"
