#ifndef UI_STYLE_HELPERS_H
#define UI_STYLE_HELPERS_H

#include "lvgl.h"

static inline void apply_transparent_container_style(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

#endif /* UI_STYLE_HELPERS_H */
