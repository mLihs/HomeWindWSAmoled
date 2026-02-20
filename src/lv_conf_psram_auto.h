#ifndef LV_CONF_PSRAM_AUTO_H
#define LV_CONF_PSRAM_AUTO_H

/* Optional: enable LVGL heap in PSRAM when HOMEWIND_LVGL_PSRAM is defined */
#ifdef HOMEWIND_LVGL_PSRAM
#include "../extras/lv_conf_psram.h"
#endif

#endif /* LV_CONF_PSRAM_AUTO_H */
