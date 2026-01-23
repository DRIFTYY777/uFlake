#if !defined(U_GUI_H)
#define U_GUI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the uGUI subsystem with LVGL
     *
     * This initializes the display, LVGL, and creates a simple demo UI.
     */
    void uGui_init(void);

#ifdef __cplusplus
}
#endif

#endif // U_GUI_H
