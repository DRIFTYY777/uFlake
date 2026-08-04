#ifndef UGUI_H
#define UGUI_H

#include "lvgl.h"
#include "kernel.h"
#include "gui_types.h"

#include "ST7789.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define LV_TICK_PERIOD_MS 10
#define DISP_BUF_SIZE (240 * 30) // width * 2

    // Main initialization
    void uGui_init(st7789_driver_t *drv);

    // Accessors for global input device and group (for multi-window management)
    lv_indev_t *uGui_get_input_device(void);
    lv_group_t *uGui_get_group(void);
    uflake_mutex_t *uGui_get_mutex(void);

    // Get the content container for app UI (properly positioned below notification bar)
    lv_obj_t *uGui_get_content_container(void);

    // Group management for app windows
    void uGui_clear_group(void); // Clear all objects from group
    void uGui_reset_group(void); // Reset group for new window

    // ============================================================================
    // APP WINDOW MANAGEMENT
    // ============================================================================

    /**
     * @brief Start a new app window - clears content container and group, shows app name
     *
     * Call this when starting an app. It:
     * 1. Safely clears all objects from content container
     * 2. Clears group (no crash)
     * 3. Shows app name in notification bar briefly
     * 4. Returns the content container for app to create UI in
     *
     * @param app_name Name to show in notification bar (NULL to skip)
     * @return lv_obj_t* Content container to create app UI in
     */
    lv_obj_t *uGui_start_app(const char *app_name);

    /**
     * @brief Clear app content - removes all children from content container safely
     *
     * Use this when switching between app screens without going back to menu
     */
    void uGui_clear_app_content(void);

    /**
     * @brief Add object to group for keyboard navigation
     *
     * @param obj Object to add to group
     */
    void uGui_add_to_group(lv_obj_t *obj);

    /**
     * @brief Auto-focus a specific UI object
     * 
     * Call this after creating UI to focus a specific object (typically the first button).
     * Uses deferred execution via lv_async_call() to ensure LVGL is in stable state
     * before attempting focus operations.
     * 
     * This fixes the "first button not focused" issue where manual lv_group_focus_obj()
     * calls fail because the group isn't fully initialized yet.
     * 
     * @param obj The object to focus (pass the first button reference)
     */
    void uGui_auto_focus_object(lv_obj_t *obj);

    // ============================================================================
    // APP LOADER INTEGRATION
    // ============================================================================

    /**
     * @brief Callback type for when a GUI app exits (user presses back to home)
     * @param app_id ID of the app that exited
     */
    typedef void (*uGui_app_exit_cb_t)(uint32_t app_id);

    /**
     * @brief Set callback for app exit notification
     * @param callback Function to call when app exits to home
     */
    void uGui_set_app_exit_callback(uGui_app_exit_cb_t callback);

    /**
     * @brief Launch a GUI app (called by appLoader)
     * This sets up the app context and calls its entry function.
     * For GUI apps, no separate task is created - the entry function
     * just sets up UI, then LVGL handles everything.
     *
     * @param app_id App ID from appLoader
     * @param app_name Name to show in notification
     * @param entry_fn App's entry function (sets up UI)
     */
    void uGui_launch_gui_app(uint32_t app_id, const char *app_name, void (*entry_fn)(void));

    /**
     * @brief Signal that current GUI app should exit
     * Clears content, shows home screen, notifies appLoader
     */
    void uGui_exit_current_app(void);

    /**
     * @brief Get ID of currently active GUI app
     * @return App ID, or 0 if no app active (home screen)
     */
    uint32_t uGui_get_current_app_id(void);

    /**
     * @brief Check if currently at home screen (no app active)
     * @return true if at home screen
     */
    bool uGui_is_home_screen(void);

    /**
     * @brief Set launcher function to call when returning to home
     * @param launcher_fn Function that creates the launcher/home UI
     */
    void uGui_set_launcher(void (*launcher_fn)(void));

#ifdef __cplusplus
}
#endif

#endif // UGUI_H