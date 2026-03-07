/**
 * @file app_force_exit.h
 * @brief Force Exit Monitor - Detect back button held to force close apps
 *
 * Monitors raw button input (not LVGL) to detect when user holds
 * the back button for 3 seconds to force-exit a stuck app.
 */

#ifndef APP_FORCE_EXIT_H
#define APP_FORCE_EXIT_H

#include "kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FORCE_EXIT_HOLD_TIME_MS 3000 // Hold back button for 3 seconds

    /**
     * @brief Initialize the force exit monitor
     * Creates a periodic timer that checks button state
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_force_exit_init(void);

    /**
     * @brief Check button state (call periodically, ~50ms)
     * Monitors back button hold time and triggers force exit
     */
    void app_force_exit_check(void);

    /**
     * @brief Manually trigger force exit of current app
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_force_exit_trigger(void);

#ifdef __cplusplus
}
#endif

#endif // APP_FORCE_EXIT_H
