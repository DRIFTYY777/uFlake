#if !defined(U_GUI_H)
#define U_GUI_H

#include <stdint.h>
#include "ST7789.h"

#define LV_TICK_PERIOD_MS 10

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the uGUI subsystem with LVGL
     *
     * This initializes the display, LVGL, and creates a simple demo UI.
     */
    void uGui_init(st7789_driver_t *drv);

#ifdef __cplusplus
}
#endif

#endif // U_GUI_H
