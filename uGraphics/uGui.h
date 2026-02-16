#if !defined(U_GUI_H)
#define U_GUI_H

#include <stdint.h>
#include "ST7789.h"

#define LV_TICK_PERIOD_MS 10

#ifdef __cplusplus
extern "C"
{
#endif

// Include all uGUI subsystems
#include "uGui_types.h"
#include "uGui_focus.h"
#include "uGui_notification.h"
#include "uGui_appwindow.h"
#include "uGui_theme.h"
#include "uGui_widgets.h"
#include "uGui_navigation.h"

    /**
     * @brief Initialize the uGUI subsystem with LVGL
     *
     * This initializes:
     * - LVGL display and input
     * - Focus manager (automatic, crash-free focus handling)
     * - Notification bar (system status display)
     * - App window manager (safe app container)
     * - Theme manager (background and colors)
     * - Widget library (dialogs, loading, etc.)
     * - Navigation system (keyboard input routing)
     *
     * After this, apps can use ugui_appwindow_create() to safely create UI.
     *
     * @param drv ST7789 display driver
     */
    void uGui_init(st7789_driver_t *drv);

    /**
     * @brief Get GUI mutex for thread-safe LVGL operations
     *
     * Lock this before any LVGL calls from non-GUI thread.
     *
     * @return GUI mutex handle
     */
    uflake_mutex_t *uGui_get_mutex(void);

    /**
     * @brief Check if uGUI is fully initialized
     *
     * @return true if initialized
     */
    bool uGui_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // U_GUI_H
