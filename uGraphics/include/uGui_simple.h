/**
 * @file uGui_simple.h
 * @brief Simplified Flipper Zero-style GUI System API
 *
 * Core GUI functionality:
 * - Simple screen management
 * - Direct input callbacks
 * - Menu stack navigation
 * - Memory-aware operation
 *
 * No complex features:
 * - No LVGL focus groups
 * - No multi-window system
 * - No async UI operations
 * - Direct control flow
 */

#ifndef UGUI_SIMPLE_H
#define UGUI_SIMPLE_H

#include "uGui_types.h"
#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize GUI system
 * @return UFLAKE_OK on success, error code on failure
 */
uflake_result_t uGui_init(void);

/**
 * @brief GUI main task - call this repeatedly from main event loop
 * 
 * Handles:
 * - LVGL rendering
 * - Input processing
 * - App lifecycle
 * - Memory monitoring
 *
 * Call frequency: Depends on LVGL, typically 20-50 times/second
 */
void uGui_task(void);

/**
 * @brief Get current GUI state
 * @return Pointer to current GUI state
 */
ugui_state_t* uGui_get_state(void);

// ============================================================================
// APP LIFECYCLE
// ============================================================================

/**
 * @brief Start an app (clear screen, prepare for app UI)
 * 
 * After calling this, app should create its UI using the returned container.
 * All previous UI is cleared.
 *
 * @param app_id Application ID
 * @return Container object to create UI in, NULL on error
 */
lv_obj_t* uGui_start_app(uint32_t app_id);

/**
 * @brief Exit current app (return to home screen)
 */
void uGui_exit_app(void);

/**
 * @brief Check if currently on home screen
 * @return true if home screen is active
 */
bool uGui_is_home_screen(void);

/**
 * @brief Set callback for app exit events
 * @param callback Function to call when app exits
 * @param userdata User data to pass to callback
 */
void uGui_set_app_exit_callback(ugui_app_exit_callback_t callback, void *userdata);

// ============================================================================
// INPUT HANDLING
// ============================================================================

/**
 * @brief Process button input
 * 
 * Called by input driver when button state changes.
 * Routes input to current app or handles system keys.
 *
 * @param button Button ID
 * @param pressed true if pressed, false if released
 */
void uGui_input_button(ugui_button_t button, bool pressed);

/**
 * @brief Register input callback for current screen
 * 
 * App can register to handle specific button presses.
 * System buttons (BACK) are always handled by GUI.
 *
 * @param button Button to intercept (UGUI_BTN_COUNT for all)
 * @param callback Function to call
 * @param userdata User data
 */
void uGui_register_input_handler(ugui_button_t button, 
                                  ugui_input_callback_t callback, 
                                  void *userdata);

/**
 * @brief Unregister input callback
 * @param button Button ID
 */
void uGui_unregister_input_handler(ugui_button_t button);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * @brief Get current free RAM
 * @return Free RAM in bytes
 */
uint32_t uGui_get_free_ram(void);

/**
 * @brief Get memory pressure level
 * @return Current memory pressure (NORMAL, LOW, CRITICAL)
 */
ugui_mem_pressure_t uGui_get_memory_pressure(void);

/**
 * @brief Check if enough memory is available
 * @param required_bytes Bytes needed
 * @return true if enough RAM, false otherwise
 */
bool uGui_has_memory(uint32_t required_bytes);

/**
 * @brief Register callback for memory pressure changes
 * 
 * Useful for apps to clean up when memory is low.
 *
 * @param callback Function to call on pressure change
 * @param userdata User data
 */
typedef void (*ugui_memory_pressure_cb_t)(ugui_mem_pressure_t level, void *userdata);
void uGui_register_memory_callback(ugui_memory_pressure_cb_t callback, void *userdata);

// ============================================================================
// SCREEN MANAGEMENT
// ============================================================================

/**
 * @brief Get main content container
 * 
 * Apps add their UI objects as children of this container.
 *
 * @return lv_obj_t pointer to container
 */
lv_obj_t* uGui_get_content_container(void);

/**
 * @brief Get LVGL display handle
 * @return Display handle for LVGL operations
 */
lv_display_t* uGui_get_display(void);

/**
 * @brief Get active screen object
 * @return Screen object pointer
 */
lv_obj_t* uGui_get_screen(void);

/**
 * @brief Clear all content from screen
 */
void uGui_clear_screen(void);

// ============================================================================
// THEME / COLORS
// ============================================================================

/**
 * @brief Get current theme
 * @return Pointer to theme structure
 */
ugui_theme_t* uGui_get_theme(void);

/**
 * @brief Set color theme
 * @param theme Theme to apply
 */
void uGui_set_theme(ugui_theme_t *theme);

// ============================================================================
// NOTIFICATION BAR (OPTIONAL)
// ============================================================================

/**
 * @brief Show notification bar
 */
void uGui_notification_show(void);

/**
 * @brief Hide notification bar
 */
void uGui_notification_hide(void);

/**
 * @brief Update notification text
 * @param text Text to show
 * @param timeout_ms How long to show (0 = permanent)
 */
void uGui_notification_set_text(const char *text, uint32_t timeout_ms);

/**
 * @brief Update app name in notification
 * @param app_name App name to display
 */
void uGui_notification_set_app_name(const char *app_name);

// ============================================================================
// HOME SCREEN / MENU
// ============================================================================

/**
 * @brief Show home screen (app list)
 */
void uGui_show_home_screen(void);

/**
 * @brief Refresh home screen (reload app list)
 */
void uGui_refresh_home_screen(void);

#ifdef __cplusplus
}
#endif

#endif // UGUI_SIMPLE_H
