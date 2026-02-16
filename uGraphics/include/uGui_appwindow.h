/**
 * @file uGui_appwindow.h
 * @brief App Window Manager - Safe container for app UI with lifecycle management
 * 
 * This module solves the app UI lifecycle problem:
 * - Apps don't need while(1) loops - LVGL handles everything
 * - Automatic focus enter/exit - no crashes
 * - Safe object creation/deletion
 * - Configurable window size (default, fullscreen, custom)
 * - Integration with notification bar
 * 
 * USAGE PATTERN:
 * 1. App calls ugui_appwindow_create() - gets a container
 * 2. App adds LVGL objects to container->content
 * 3. App registers UI objects with ugui_appwindow_add_focusable()
 * 4. App exits - ugui_appwindow_destroy() cleans everything safely
 * 5. No crashes, no manual focus management!
 * 
 * PROBLEM SOLVED:
 * - Apps without while loops work correctly
 * - LVGL objects are automatically cleaned up
 * - Focus is managed automatically
 * - No system crashes on app exit
 */

#ifndef UGUI_APPWINDOW_H
#define UGUI_APPWINDOW_H

#include "uGui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize the app window manager
 * Must be called after ugui_focus_init()
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_init(void);

// ============================================================================
// APP WINDOW LIFECYCLE
// ============================================================================

/**
 * @brief Create an app window
 * 
 * This creates a safe container for app UI. The app should add all
 * LVGL objects to window->content. Focus is automatically managed.
 * 
 * @param config Window configuration (NULL for defaults)
 * @param app_id App ID from app loader
 * @return App window handle, NULL on error
 */
ugui_appwin_t *ugui_appwindow_create(const ugui_appwin_config_t *config, uint32_t app_id);

/**
 * @brief Destroy an app window
 * 
 * CRITICAL: This safely destroys all app UI objects and releases focus.
 * Call this when app exits. No crashes, guaranteed.
 * 
 * @param window App window handle
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_destroy(ugui_appwin_t *window);

/**
 * @brief Get the content container for adding app UI
 * 
 * Apps should add all LVGL objects to this container:
 *   lv_obj_t *content = ugui_appwindow_get_content(window);
 *   lv_obj_t *btn = lv_btn_create(content);
 * 
 * @param window App window handle
 * @return Content container object
 */
lv_obj_t *ugui_appwindow_get_content(ugui_appwin_t *window);

// ============================================================================
// FOCUS MANAGEMENT (Automatic)
// ============================================================================

/**
 * @brief Add a focusable object to app window
 * 
 * This registers an object for keyboard navigation.
 * Objects must be focusable (buttons, text areas, etc.)
 * 
 * Example:
 *   lv_obj_t *btn = lv_btn_create(content);
 *   ugui_appwindow_add_focusable(window, btn);
 * 
 * @param window App window handle
 * @param obj LVGL object to add
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_add_focusable(ugui_appwin_t *window, lv_obj_t *obj);

/**
 * @brief Remove a focusable object from app window
 * 
 * @param window App window handle
 * @param obj LVGL object to remove
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_remove_focusable(ugui_appwin_t *window, lv_obj_t *obj);

/**
 * @brief Navigate to next focusable object
 * 
 * @param window App window handle
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_focus_next(ugui_appwin_t *window);

/**
 * @brief Navigate to previous focusable object
 * 
 * @param window App window handle
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_focus_prev(ugui_appwin_t *window);

// ============================================================================
// WINDOW STATE
// ============================================================================

/**
 * @brief Activate app window (bring to front, request focus)
 * 
 * @param window App window handle
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_activate(ugui_appwin_t *window);

/**
 * @brief Deactivate app window (release focus, move to background)
 * 
 * @param window App window handle
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_deactivate(ugui_appwin_t *window);

/**
 * @brief Check if app window is active
 * 
 * @param window App window handle
 * @return true if active
 */
bool ugui_appwindow_is_active(ugui_appwin_t *window);

// ============================================================================
// WINDOW CONFIGURATION
// ============================================================================

/**
 * @brief Set window fullscreen mode
 * 
 * Hides notification bar and expands window to full display
 * 
 * @param window App window handle
 * @param fullscreen true for fullscreen, false for normal
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_set_fullscreen(ugui_appwin_t *window, bool fullscreen);

/**
 * @brief Set window custom size
 * 
 * @param window App window handle
 * @param width Window width (0 = default)
 * @param height Window height (0 = default)
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_set_size(ugui_appwin_t *window, uint16_t width, uint16_t height);

/**
 * @brief Set window background color
 * 
 * @param window App window handle
 * @param color Background color
 * @param opacity Opacity (0-255)
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_set_background(ugui_appwin_t *window, lv_color_t color, uint8_t opacity);

/**
 * @brief Show/hide window
 * 
 * @param window App window handle
 * @param visible true to show, false to hide
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_set_visible(ugui_appwin_t *window, bool visible);

// ============================================================================
// ANIMATION SUPPORT
// ============================================================================

/**
 * @brief Fade in window (smooth entry animation)
 * 
 * @param window App window handle
 * @param duration_ms Animation duration in milliseconds
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_fade_in(ugui_appwin_t *window, uint32_t duration_ms);

/**
 * @brief Fade out window (smooth exit animation)
 * 
 * @param window App window handle
 * @param duration_ms Animation duration in milliseconds
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_fade_out(ugui_appwin_t *window, uint32_t duration_ms);

// ============================================================================
// SAFE CLEANUP HELPERS
// ============================================================================

/**
 * @brief Clear all content from window
 * 
 * Safely removes all child objects from content container
 * 
 * @param window App window handle
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_clear_content(ugui_appwin_t *window);

/**
 * @brief Safely delete a child object
 * 
 * Use this instead of lv_obj_del() for objects in app window
 * 
 * @param window App window handle
 * @param obj Object to delete
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_appwindow_delete_object(ugui_appwin_t *window, lv_obj_t *obj);

// ============================================================================
// INTEGRATION WITH APP LOADER
// ============================================================================

/**
 * @brief Get app window by app ID
 * 
 * @param app_id App ID from app loader
 * @return App window handle, NULL if not found
 */
ugui_appwin_t *ugui_appwindow_get_by_app_id(uint32_t app_id);

/**
 * @brief Get currently active app window
 * 
 * @return Active app window handle, NULL if none
 */
ugui_appwin_t *ugui_appwindow_get_active(void);

// ============================================================================
// DEBUG
// ============================================================================

/**
 * @brief Print app window debug info
 * 
 * @param window App window handle
 */
void ugui_appwindow_debug_print(ugui_appwin_t *window);

#ifdef __cplusplus
}
#endif

#endif // UGUI_APPWINDOW_H
