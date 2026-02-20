/**
 * LVGL PSRAM memory configuration snippet
 *
 * Use this so that LVGL allocates its internal heap (objects, styles, etc.)
 * from PSRAM instead of internal RAM. Saves ~5–15 KB+ internal RAM; UI may
 * be slightly slower due to PSRAM access.
 *
 * How to use (choose one):
 *
 * 1) Include from your project's lv_conf.h (before any other LV_MEM_* defines):
 *    #include "extras/lv_conf_psram.h"
 *    (Adjust path if your lv_conf.h is not at project root.)
 *
 * 2) Copy the defines below into your lv_conf.h.
 *
 * Requirements:
 * - ESP32 with PSRAM (e.g. ESP32-S3, ESP32-P4, or ESP32 with SPIRAM).
 * - In lv_conf.h you must use LV_MEM_CUSTOM 1 (this snippet sets it).
 * - LVGL v7/v8 (LV_MEM_CUSTOM_* macro names).
 *
 * HomeWindWSAmoled – optional extra
 */

#ifndef LV_CONF_PSRAM_H
#define LV_CONF_PSRAM_H

#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "esp_heap_caps.h"
#define LV_MEM_CUSTOM_ALLOC(size)     heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
#define LV_MEM_CUSTOM_FREE(ptr)       heap_caps_free(ptr)
#define LV_MEM_CUSTOM_REALLOC(ptr, size)  heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM)

#endif /* LV_CONF_PSRAM_H */
