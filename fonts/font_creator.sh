#!/bin/bash

# Inter Black – Numbers Only (0–9)
lv_font_conv \
  --font "Inter/Inter_18pt-Black.ttf" \
  --size 64 \
  --bpp 4 \
  --format lvgl \
  --range 48-57 \
  --no-compress \
  --output "lv_font_inter_black_64.c"


# Inter Bold – General ASCII (14, 16, 20, 24 px)
lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 14 \
  --bpp 4 \
  --format lvgl \
  --range 32-126 \
  --no-compress \
  --output "lv_font_inter_bold_14.c"

lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 16 \
  --bpp 4 \
  --format lvgl \
  --range 32-126 \
  --no-compress \
  --output "lv_font_inter_bold_16.c"

lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 20 \
  --bpp 4 \
  --format lvgl \
  --range 32-126 \
  --no-compress \
  --output "lv_font_inter_bold_20.c"

lv_font_conv \
  --font "Inter/Inter_18pt-Bold.ttf" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --range 32-126 \
  --no-compress \
  --output "lv_font_inter_bold_24.c"


# Inter Regular – General ASCII (20, 24 px)
lv_font_conv \
  --font "Inter/Inter_18pt-Regular.ttf" \
  --size 20 \
  --bpp 4 \
  --format lvgl \
  --range 32-126 \
  --no-compress \
  --output "lv_font_inter_regular_20.c"

lv_font_conv \
  --font "Inter/Inter_18pt-Regular.ttf" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --range 32-126 \
  --no-compress \
  --output "lv_font_inter_regular_24.c"

echo "✔ Alle LVGL-Fonts erfolgreich erzeugt!"
