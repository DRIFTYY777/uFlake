/**
 * @file uGui_types.h
 * @brief Common types and definitions for uFlake GUI system
 *
 * This file contains all shared types, enums, and constants used across
 * the GUI library components.
 */

#ifndef UGUI_TYPES_H
#define UGUI_TYPES_H

#include "lvgl.h"
#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // ============================================================================
    // DISPLAY CONFIGURATION
    // ============================================================================

#define UGUI_DISPLAY_WIDTH 320
#define UGUI_DISPLAY_HEIGHT 240

#define UGUI_NOTIFICATION_HEIGHT 30
#define UGUI_APPWINDOW_WIDTH 320
#define UGUI_APPWINDOW_HEIGHT (240 - UGUI_NOTIFICATION_HEIGHT) // 210
#define UGUI_APPWINDOW_X_OFFSET 0
#define UGUI_APPWINDOW_Y_OFFSET UGUI_NOTIFICATION_HEIGHT

    // ============================================================================
    // THEME AND COLOR TYPES
    // ============================================================================

    /**
     * @brief Theme color palette
     */
    typedef struct
    {
        lv_color_t primary;         // Primary theme color
        lv_color_t secondary;       // Secondary/accent color
        lv_color_t background;      // Default background
        lv_color_t text;            // Text color
        lv_color_t notification_bg; // Notification bar background
        lv_color_t notification_fg; // Notification bar foreground
        uint8_t opacity;            // Global opacity (0-255)
    } ugui_theme_t;

    /**
     * @brief Background type
     */
    typedef enum
    {
        UGUI_BG_SOLID_COLOR = 0, // Solid color background
        UGUI_BG_IMAGE_SDCARD,    // Image from SD card
        UGUI_BG_IMAGE_FLASH      // Image from flash storage
    } ugui_bg_type_t;

    /**
     * @brief Background configuration
     */
    typedef struct
    {
        ugui_bg_type_t type;
        union
        {
            lv_color_t color;     // For solid color
            char image_path[256]; // For image (SD/flash)
        };
    } ugui_background_t;

    // ============================================================================
    // FOCUS MANAGEMENT TYPES
    // ============================================================================

    /**
     * @brief Focus layer priority (higher = on top)
     */
    typedef enum
    {
        UGUI_LAYER_BACKGROUND = 0, // Wallpaper layer
        UGUI_LAYER_APP_WINDOW,     // Main app window
        UGUI_LAYER_NOTIFICATION,   // Notification bar (always visible)
        UGUI_LAYER_DIALOG,         // Modal dialogs
        UGUI_LAYER_SYSTEM          // System overlays (widgets, loading, etc.)
    } ugui_layer_t;

    /**
     * @brief Focus state for objects
     */
    typedef enum
    {
        UGUI_FOCUS_NONE = 0,
        UGUI_FOCUS_ACTIVE,  // Has focus, receives input
        UGUI_FOCUS_INACTIVE // No focus, input blocked
    } ugui_focus_state_t;

    /**
     * @brief Focus context - tracks focus state
     */
    typedef struct ugui_focus_ctx_t
    {
        lv_obj_t *focused_obj; // Currently focused object
        ugui_layer_t layer;    // Focus layer
        bool input_enabled;    // Input routing enabled
        void *userdata;        // User data pointer
    } ugui_focus_ctx_t;

    // ============================================================================
    // APP WINDOW TYPES
    // ============================================================================

    /**
     * @brief App window flags
     */
    typedef enum
    {
        UGUI_APPWIN_FLAG_NONE = 0,
        UGUI_APPWIN_FLAG_FULLSCREEN = (1 << 0),  // Hides notification bar
        UGUI_APPWIN_FLAG_CUSTOM_SIZE = (1 << 1), // Custom dimensions
        UGUI_APPWIN_FLAG_NO_SCROLL = (1 << 2)    // Disable scrolling
    } ugui_appwin_flags_t;

    /**
     * @brief App window configuration
     */
    typedef struct
    {
        const char *app_name;      // App name (shown in notification)
        uint16_t width;            // Custom width (0 = default)
        uint16_t height;           // Custom height (0 = default)
        ugui_appwin_flags_t flags; // Window flags
        lv_color_t bg_color;       // Background color
        uint8_t bg_opacity;        // Background opacity (0-255)
    } ugui_appwin_config_t;

    /**
     * @brief App window handle
     */
    typedef struct ugui_appwin_t
    {
        lv_obj_t *container;         // Main container object
        lv_obj_t *content;           // Content area for app
        ugui_focus_ctx_t focus;      // Focus context
        ugui_appwin_config_t config; // Window configuration
        bool is_active;              // Window active state
        uint32_t app_id;             // Associated app ID
    } ugui_appwin_t;

    // ============================================================================
    // WIDGET TYPES
    // ============================================================================

    /**
     * @brief Widget dialog button type
     */
    typedef enum
    {
        UGUI_DIALOG_BTN_OK = 0,
        UGUI_DIALOG_BTN_CANCEL,
        UGUI_DIALOG_BTN_YES,
        UGUI_DIALOG_BTN_NO,
        UGUI_DIALOG_BTN_RETRY
    } ugui_dialog_btn_t;

    /**
     * @brief Widget dialog result callback
     */
    typedef void (*ugui_dialog_cb_t)(ugui_dialog_btn_t button, void *userdata);

    /**
     * @brief Loading widget style
     */
    typedef enum
    {
        UGUI_LOADING_DOTS = 0, // Windows mobile style dots
        UGUI_LOADING_SPINNER,  // Spinning circle
        UGUI_LOADING_BAR       // Progress bar
    } ugui_loading_style_t;

    // ============================================================================
    // NAVIGATION TYPES
    // ============================================================================

    /**
     * @brief Navigation button mapping
     */
    typedef enum
    {
        UGUI_NAV_UP = 0,
        UGUI_NAV_DOWN,
        UGUI_NAV_LEFT,
        UGUI_NAV_RIGHT,
        UGUI_NAV_OK,
        UGUI_NAV_BACK,
        UGUI_NAV_MENU,
        UGUI_NAV_COUNT
    } ugui_nav_button_t;

    /**
     * @brief Navigation event callback
     */
    typedef void (*ugui_nav_cb_t)(ugui_nav_button_t button, bool pressed, void *userdata);

    // ============================================================================
    // NOTIFICATION TYPES
    // ============================================================================

    /**
     * @brief Notification bar icons
     */
    typedef struct
    {
        bool show_battery;
        bool show_wifi;
        bool show_bluetooth;
        bool show_sdcard;
        bool show_time;
    } ugui_notif_icons_t;

    /**
     * @brief System status data
     */
    typedef struct
    {
        uint8_t battery_percent; // 0-100
        bool charging;
        bool wifi_connected;
        bool bt_connected;
        bool sdcard_mounted;
        uint8_t hour;   // 0-23
        uint8_t minute; // 0-59
    } ugui_system_status_t;

    // ============================================================================
    // CALLBACKS AND EVENTS
    // ============================================================================

    /**
     * @brief GUI event types
     */
    typedef enum
    {
        UGUI_EVENT_APP_STARTED = 0,
        UGUI_EVENT_APP_STOPPED,
        UGUI_EVENT_FOCUS_CHANGED,
        UGUI_EVENT_THEME_CHANGED
    } ugui_event_t;

    /**
     * @brief GUI event callback
     */
    typedef void (*ugui_event_cb_t)(ugui_event_t event, void *data, void *userdata);

#ifdef __cplusplus
}
#endif

#endif // UGUI_TYPES_H
