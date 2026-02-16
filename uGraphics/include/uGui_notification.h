/**
 * @file uGui_notification.h
 * @brief Notification Bar - Always-visible system status bar
 * 
 * Displays:
 * - Battery status (percent, charging icon)
 * - WiFi status
 * - Bluetooth status
 * - SD card status
 * - Time (HH:MM)
 * - App name (when app starts, fades back to system info)
 * - Loading indicator (Windows mobile style dots)
 * 
 * Features:
 * - Transparent background with theme color
 * - Can be hidden in fullscreen apps
 * - Always on top (UGUI_LAYER_NOTIFICATION)
 * - Size: 240x25 pixels
 */

#ifndef UGUI_NOTIFICATION_H
#define UGUI_NOTIFICATION_H

#include "uGui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize the notification bar
 * Must be called after ugui_focus_init()
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_init(void);

/**
 * @brief Set the theme for notification bar
 * 
 * @param theme Theme configuration
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_set_theme(const ugui_theme_t *theme);

// ============================================================================
// VISIBILITY CONTROL
// ============================================================================

/**
 * @brief Show notification bar
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_show(void);

/**
 * @brief Hide notification bar (for fullscreen apps)
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_hide(void);

/**
 * @brief Check if notification bar is visible
 * 
 * @return true if visible
 */
bool ugui_notification_is_visible(void);

// ============================================================================
// SYSTEM STATUS UPDATE
// ============================================================================

/**
 * @brief Update system status icons
 * 
 * @param status System status data
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_update_status(const ugui_system_status_t *status);

/**
 * @brief Update battery status
 * 
 * @param percent Battery percentage (0-100)
 * @param charging true if charging
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_update_battery(uint8_t percent, bool charging);

/**
 * @brief Update WiFi status
 * 
 * @param connected true if connected
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_update_wifi(bool connected);

/**
 * @brief Update Bluetooth status
 * 
 * @param connected true if connected
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_update_bluetooth(bool connected);

/**
 * @brief Update SD card status
 * 
 * @param mounted true if mounted
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_update_sdcard(bool mounted);

/**
 * @brief Update time display
 * 
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_update_time(uint8_t hour, uint8_t minute);

// ============================================================================
// APP NAME DISPLAY
// ============================================================================

/**
 * @brief Show app name in notification bar
 * Displays for duration_ms, then fades back to system info
 * 
 * @param app_name App name to display
 * @param duration_ms Display duration in milliseconds (0 = permanent)
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_show_app_name(const char *app_name, uint32_t duration_ms);

/**
 * @brief Clear app name and return to system info display
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_clear_app_name(void);

// ============================================================================
// LOADING INDICATOR
// ============================================================================

/**
 * @brief Show loading indicator (Windows mobile style dots)
 * Displays animated dots in notification bar
 * 
 * @param show true to show, false to hide
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_show_loading(bool show);

/**
 * @brief Check if loading indicator is active
 * 
 * @return true if loading indicator is showing
 */
bool ugui_notification_is_loading(void);

// ============================================================================
// ICON CONFIGURATION
// ============================================================================

/**
 * @brief Configure which icons are displayed
 * 
 * @param icons Icon visibility configuration
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_set_icons(const ugui_notif_icons_t *icons);

/**
 * @brief Get current icon configuration
 * 
 * @param icons Output: current icon configuration
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_get_icons(ugui_notif_icons_t *icons);

// ============================================================================
// ADVANCED
// ============================================================================

/**
 * @brief Get the notification bar LVGL object
 * For advanced customization
 * 
 * @return LVGL object, NULL if not initialized
 */
lv_obj_t *ugui_notification_get_object(void);

/**
 * @brief Refresh notification bar display
 * Redraws all elements
 * 
 * @return UFLAKE_OK on success
 */
uflake_result_t ugui_notification_refresh(void);

#ifdef __cplusplus
}
#endif

#endif // UGUI_NOTIFICATION_H
