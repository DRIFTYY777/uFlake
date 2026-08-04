/**
 * @file uGui_types.h
 * @brief Core GUI type definitions for simplified Flipper Zero-style interface
 *
 * Minimal type definitions for the new GUI system.
 */

#ifndef UGUI_TYPES_H
#define UGUI_TYPES_H

#include "lvgl.h"
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

#define UGUI_NOTIFICATION_HEIGHT 24  // Minimal notification bar
#define UGUI_CONTENT_HEIGHT (UGUI_DISPLAY_HEIGHT - UGUI_NOTIFICATION_HEIGHT)

// ============================================================================
// BUTTON TYPES
// ============================================================================

typedef enum
{
    UGUI_BTN_UP = 0,
    UGUI_BTN_DOWN,
    UGUI_BTN_LEFT,
    UGUI_BTN_RIGHT,
    UGUI_BTN_OK,      // Select/Enter
    UGUI_BTN_BACK,    // Back/Escape
    UGUI_BTN_COUNT
} ugui_button_t;

// ============================================================================
// MEMORY PRESSURE LEVELS
// ============================================================================

typedef enum
{
    UGUI_MEM_NORMAL = 0,      // >50% free RAM
    UGUI_MEM_LOW,             // 20-50% free RAM
    UGUI_MEM_CRITICAL         // <20% free RAM
} ugui_mem_pressure_t;

// ============================================================================
// APP STATE
// ============================================================================

typedef enum
{
    UGUI_APP_STATE_HOME = 0,  // Showing home/launcher
    UGUI_APP_STATE_RUNNING,   // App is running
    UGUI_APP_STATE_EXITING    // App is exiting
} ugui_app_state_t;

// ============================================================================
// INPUT CALLBACK TYPES
// ============================================================================

/**
 * @brief Callback function for button presses
 * @param button Button ID
 * @param pressed true if pressed, false if released
 * @param userdata User-provided data
 */
typedef void (*ugui_input_callback_t)(ugui_button_t button, bool pressed, void *userdata);

/**
 * @brief Callback function for app exit
 * @param userdata User-provided data
 */
typedef void (*ugui_app_exit_callback_t)(void *userdata);

// ============================================================================
// DISPLAY INTERFACE
// ============================================================================

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t bpp;  // Bits per pixel
    lv_display_t *handle;
} ugui_display_t;

// ============================================================================
// COLOR THEME (MINIMAL)
// ============================================================================

typedef struct
{
    lv_color_t bg_primary;         // Main background
    lv_color_t bg_secondary;       // Secondary background
    lv_color_t text_primary;       // Main text
    lv_color_t text_secondary;     // Secondary text
    lv_color_t accent;             // Accent color
    lv_color_t notification_bg;    // Notification bar background
    lv_color_t notification_fg;    // Notification bar foreground
} ugui_theme_t;

// ============================================================================
// GLOBAL STATE
// ============================================================================

typedef struct
{
    ugui_app_state_t state;              // Current app state
    ugui_mem_pressure_t mem_pressure;    // Memory pressure level
    uint32_t free_ram;                   // Free RAM in bytes
    uint32_t current_app_id;             // Currently running app ID (0 = home)
    bool notification_enabled;           // Show notification bar
    lv_obj_t *screen;                    // Main screen object
    lv_obj_t *content_container;         // Content area container
} ugui_state_t;

#ifdef __cplusplus
}
#endif

#endif // UGUI_TYPES_H
