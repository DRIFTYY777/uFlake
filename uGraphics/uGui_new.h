/**
 * @file uGui.h
 * @brief uFlake GUI System - Lightweight, Fast, Memory-Aware
 *
 * Provides LVGL-based UI with:
 * - Fast app window management (content container + notification bar)
 * - Memory-aware app context integration
 * - Async callbacks for non-blocking operations
 * - Support for GUI apps (internal and external ELF)
 */

#ifndef UGUI_H
#define UGUI_H

#include "lvgl.h"
#include "kernel.h"
#include "gui_types.h"
#include "app_context.h"

#include "ST7789.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define LV_TICK_PERIOD_MS 10
#define DISP_BUF_SIZE (240 * 30) // width * 2

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize GUI system
 * @param drv Display driver (ST7789)
 * @return UFLAKE_OK on success
 */
void uGui_init(st7789_driver_t *drv);

// ============================================================================
// GLOBAL ACCESSORS
// ============================================================================

/**
 * @brief Get global input device for all windows
 * @return Input device handle
 */
lv_indev_t *uGui_get_input_device(void);

/**
 * @brief Get global focus group for keyboard navigation
 * @return Focus group handle
 */
lv_group_t *uGui_get_group(void);

/**
 * @brief Get GUI mutex (for thread-safe operations)
 * @return Mutex handle
 */
uflake_mutex_t *uGui_get_mutex(void);

// ============================================================================
// CONTENT CONTAINER (Main app window area)
// ============================================================================

/**
 * @brief Get content container for app UI
 * Container is positioned below notification bar.
 * Clear and reuse for each app window.
 * @return Container object
 */
lv_obj_t *uGui_get_content_container(void);

/**
 * @brief Clear all children from content container safely
 * Use when switching app screens
 */
void uGui_clear_app_content(void);

/**
 * @brief Reset focus group for new app window
 */
void uGui_reset_group(void);

// ============================================================================
// APP WINDOW MANAGEMENT (Memory and Context Aware)
// ============================================================================

/**
 * @brief Start new app window with context integration
 * - Clears content container
 * - Allocates app context
 * - Updates notification bar
 *
 * @param app_id App ID (for context tracking)
 * @param app_name Name to show in notification
 * @param app_ctx Pointer to receive allocated app context
 * @return Content container for app to create UI in
 */
lv_obj_t *uGui_start_app_with_context(uint32_t app_id, const char *app_name, app_context_t **app_ctx);

/**
 * @brief Simplified app start (for backward compatibility)
 * @param app_name Name to show in notification bar
 * @return Content container to create app UI in
 */
lv_obj_t *uGui_start_app(const char *app_name);

/**
 * @brief Add object to focus group for keyboard navigation
 * @param obj Object to add
 */
void uGui_add_to_group(lv_obj_t *obj);

/**
 * @brief Auto-focus object (deferred, non-blocking)
 * @param obj Object to focus
 */
void uGui_auto_focus_object(lv_obj_t *obj);

// ============================================================================
// APP LIFECYCLE CALLBACKS
// ============================================================================

/**
 * @brief Callback when app exits (user presses back to home)
 */
typedef void (*uGui_app_exit_cb_t)(uint32_t app_id);

/**
 * @brief Register app exit callback
 * @param callback Function to call
 */
void uGui_set_app_exit_callback(uGui_app_exit_cb_t callback);

/**
 * @brief Launch GUI app (called by appLoader)
 * - Allocates context
 * - Shows app window
 * - Calls entry function
 *
 * @param app_id App ID
 * @param app_name Name to show
 * @param entry_fn Entry function
 * @return UFLAKE_OK on success
 */
uflake_result_t uGui_launch_gui_app(uint32_t app_id, const char *app_name, void (*entry_fn)(void));

/**
 * @brief Signal current app should exit
 * - Clears content
 * - Shows home screen
 * - Notifies appLoader
 */
void uGui_exit_current_app(void);

/**
 * @brief Get currently active app ID
 * @return App ID, or 0 if home screen
 */
uint32_t uGui_get_current_app_id(void);

/**
 * @brief Check if at home screen
 * @return true if home screen
 */
bool uGui_is_home_screen(void);

/**
 * @brief Set launcher/home screen function
 * @param launcher_fn Function to create home UI
 */
void uGui_set_launcher(void (*launcher_fn)(void));

// ============================================================================
// NOTIFICATION PANEL (Status bar)
// ============================================================================

/**
 * @brief Update notification bar text (app name, status)
 * @param text Text to display
 */
void uGui_set_notification_text(const char *text);

/**
 * @brief Update notification bar with system status
 * @param battery Battery percent (0-100)
 * @param charging true if charging
 * @param signal Signal strength (0-4)
 */
void uGui_set_notification_status(uint8_t battery, bool charging, uint8_t signal);

/**
 * @brief Show brief notification (auto-hide after timeout)
 * @param text Text to show
 * @param duration_ms Duration in milliseconds
 */
void uGui_show_notification(const char *text, uint32_t duration_ms);

// ============================================================================
// MEMORY AND CONTEXT MANAGEMENT
// ============================================================================

/**
 * @brief Register memory pressure callback
 * Called when memory pressure changes (normal/low/critical)
 */
typedef void (*uGui_mem_pressure_cb_t)(mem_pressure_level_t level);
void uGui_register_memory_callback(uGui_mem_pressure_cb_t callback);

/**
 * @brief Get context for current app
 * @return Pointer to app context, NULL if none
 */
app_context_t *uGui_get_current_context(void);

/**
 * @brief Cleanup on app exit (release context)
 * @param app_id App ID to cleanup
 */
void uGui_cleanup_app(uint32_t app_id);

#ifdef __cplusplus
}
#endif

#endif // UGUI_H
