/**
 * @file uGui_navigation.h
 * @brief Keyboard Navigation System
 * 
 * Routes button inputs to the correct focused object.
 * Handles navigation (up/down/left/right), OK, back, and menu buttons.
 */

#ifndef UGUI_NAVIGATION_H
#define UGUI_NAVIGATION_H

#include "uGui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize navigation system
 * Must be called after ugui_focus_init()
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_init(void);

/**
 * @brief Set the LVGL input device for navigation
 * 
 * @param indev LVGL input device handle
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_set_indev(lv_indev_t *indev);

// ============================================================================
// BUTTON INPUT ROUTING
// ============================================================================

/**
 * @brief Route button press to focused object
 * 
 * Call this from your button handler when a button is pressed/released.
 * The navigation system will route input to the currently focused layer.
 * 
 * @param button Button that was pressed
 * @param pressed true if pressed, false if released
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_button_event(ugui_nav_button_t button, bool pressed);

/**
 * @brief Register a global navigation callback
 * 
 * Receives all navigation events (for debugging or global handlers)
 * 
 * @param callback Navigation callback
 * @param userdata User data for callback
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_register_callback(ugui_nav_cb_t callback, void *userdata);

/**
 * @brief Unregister global navigation callback
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_unregister_callback(void);

// ============================================================================
// NAVIGATION HELPERS
// ============================================================================

/**
 * @brief Navigate to next focusable object on active layer
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_next(void);

/**
 * @brief Navigate to previous focusable object on active layer
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_prev(void);

/**
 * @brief Trigger OK/Enter action on focused object
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_ok(void);

/**
 * @brief Trigger back/cancel action
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_back(void);

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief Enable/disable navigation
 * 
 * @param enabled true to enable, false to disable
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_navigation_set_enabled(bool enabled);

/**
 * @brief Check if navigation is enabled
 * 
 * @return true if enabled
 */
bool ugui_navigation_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // UGUI_NAVIGATION_H
