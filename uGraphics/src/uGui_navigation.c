/**
 * @file uGui_navigation.c
 * @brief Keyboard Navigation System Implementation
 */

#include "uGui_navigation.h"
#include "uGui_focus.h"
#include "uGui_appwindow.h"
#include "logger.h"
#include <string.h>

static const char *TAG = "uGUI_Nav";

// ============================================================================
// NAVIGATION STATE
// ============================================================================

typedef struct
{
    bool initialized;
    bool enabled;
    lv_indev_t *indev;
    ugui_nav_cb_t global_callback;
    void *global_userdata;
    uflake_mutex_t *mutex;
} navigation_t;

static navigation_t g_nav = {0};

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t ugui_navigation_init(void)
{
    if (g_nav.initialized)
    {
        UFLAKE_LOGW(TAG, "Navigation already initialized");
        return UFLAKE_OK;
    }

    memset(&g_nav, 0, sizeof(navigation_t));

    // Create mutex
    if (uflake_mutex_create(&g_nav.mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create navigation mutex");
        return UFLAKE_ERROR;
    }

    g_nav.enabled = true;
    g_nav.initialized = true;

    UFLAKE_LOGI(TAG, "Navigation system initialized");

    return UFLAKE_OK;
}

uflake_result_t ugui_navigation_set_indev(lv_indev_t *indev)
{
    if (!g_nav.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_nav.indev = indev;

    UFLAKE_LOGI(TAG, "Input device set");

    return UFLAKE_OK;
}

// ============================================================================
// BUTTON INPUT ROUTING
// ============================================================================

uflake_result_t ugui_navigation_button_event(ugui_nav_button_t button, bool pressed)
{
    if (!g_nav.initialized || !g_nav.enabled)
    {
        return UFLAKE_ERROR;
    }

    // Call global callback if registered
    if (g_nav.global_callback)
    {
        g_nav.global_callback(button, pressed, g_nav.global_userdata);
    }

    // Only process button press (not release) for most actions
    if (!pressed)
    {
        return UFLAKE_OK;
    }

    // Route based on button type
    switch (button)
    {
    case UGUI_NAV_UP:
    case UGUI_NAV_LEFT:
        ugui_navigation_prev();
        break;

    case UGUI_NAV_DOWN:
    case UGUI_NAV_RIGHT:
        ugui_navigation_next();
        break;

    case UGUI_NAV_OK:
        ugui_navigation_ok();
        break;

    case UGUI_NAV_BACK:
        ugui_navigation_back();
        break;

    case UGUI_NAV_MENU:
        // TODO: Open app menu
        UFLAKE_LOGI(TAG, "Menu button pressed");
        break;

    default:
        break;
    }

    return UFLAKE_OK;
}

uflake_result_t ugui_navigation_register_callback(ugui_nav_cb_t callback, void *userdata)
{
    if (!g_nav.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_nav.global_callback = callback;
    g_nav.global_userdata = userdata;

    return UFLAKE_OK;
}

uflake_result_t ugui_navigation_unregister_callback(void)
{
    if (!g_nav.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_nav.global_callback = NULL;
    g_nav.global_userdata = NULL;

    return UFLAKE_OK;
}

// ============================================================================
// NAVIGATION HELPERS
// ============================================================================

uflake_result_t ugui_navigation_next(void)
{
    if (!g_nav.initialized || !g_nav.enabled)
    {
        return UFLAKE_ERROR;
    }

    // Get active app window
    ugui_appwin_t *active_win = ugui_appwindow_get_active();
    if (active_win)
    {
        return ugui_appwindow_focus_next(active_win);
    }

    return UFLAKE_ERROR;
}

uflake_result_t ugui_navigation_prev(void)
{
    if (!g_nav.initialized || !g_nav.enabled)
    {
        return UFLAKE_ERROR;
    }

    // Get active app window
    ugui_appwin_t *active_win = ugui_appwindow_get_active();
    if (active_win)
    {
        return ugui_appwindow_focus_prev(active_win);
    }

    return UFLAKE_ERROR;
}

uflake_result_t ugui_navigation_ok(void)
{
    if (!g_nav.initialized || !g_nav.enabled)
    {
        return UFLAKE_ERROR;
    }

    // Send ENTER key event to LVGL
    // This will trigger the focused button or widget
    // TODO: Implement proper LVGL key event sending

    UFLAKE_LOGI(TAG, "OK button pressed");

    return UFLAKE_OK;
}

uflake_result_t ugui_navigation_back(void)
{
    if (!g_nav.initialized || !g_nav.enabled)
    {
        return UFLAKE_ERROR;
    }

    // Back button typically closes current app/dialog
    // This should be handled by app loader or dialog system

    UFLAKE_LOGI(TAG, "Back button pressed");

    return UFLAKE_OK;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

uflake_result_t ugui_navigation_set_enabled(bool enabled)
{
    if (!g_nav.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_nav.enabled = enabled;

    UFLAKE_LOGI(TAG, "Navigation %s", enabled ? "enabled" : "disabled");

    return UFLAKE_OK;
}

bool ugui_navigation_is_enabled(void)
{
    return g_nav.enabled;
}
