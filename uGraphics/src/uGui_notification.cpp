/**
 * @file uGui_notification.c
 * @brief Notification Bar Implementation
 */

#include "uGui_notification.h"
// #include "uGui_focus.h"
#include "gui_types.h"
#include "kernel.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "uGUI_Notif";

// ============================================================================
// NOTIFICATION BAR STATE
// ============================================================================

typedef struct
{
    bool initialized;
    bool visible;

    // LVGL objects
    lv_obj_t *container;
    lv_obj_t *battery_label;
    lv_obj_t *wifi_icon;
    lv_obj_t *bt_icon;
    lv_obj_t *sd_icon;
    lv_obj_t *time_label;
    lv_obj_t *app_name_label;
    lv_obj_t *loading_label;

    // Focus management
    // ugui_focus_ctx_t *focus_ctx;

    // State
    ugui_system_status_t status;
    ugui_notif_icons_t icon_config;
    ugui_theme_t theme;

    // App name display
    char app_name[64];
    lv_timer_t *app_name_timer; // LVGL timer (runs in lv_timer_handler context)
    bool showing_app_name;

    // Loading indicator (simplified - no animation)
    bool loading_active;

} notification_bar_t;

static notification_bar_t g_notif = {};

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// LVGL timer callback - runs safely within lv_timer_handler()
static void app_name_lv_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    // Clear app name after timeout
    g_notif.showing_app_name = false;
    if (g_notif.app_name_timer)
    {
        lv_timer_delete(g_notif.app_name_timer);
        g_notif.app_name_timer = NULL;
    }
    // Update display is safe here - we're in LVGL context
    if (g_notif.app_name_label)
        lv_obj_add_flag(g_notif.app_name_label, LV_OBJ_FLAG_HIDDEN);
    // Show system icons again
    if (g_notif.battery_label && g_notif.icon_config.show_battery)
        lv_obj_clear_flag(g_notif.battery_label, LV_OBJ_FLAG_HIDDEN);
    if (g_notif.wifi_icon && g_notif.icon_config.show_wifi)
        lv_obj_clear_flag(g_notif.wifi_icon, LV_OBJ_FLAG_HIDDEN);
    if (g_notif.bt_icon && g_notif.icon_config.show_bluetooth)
        lv_obj_clear_flag(g_notif.bt_icon, LV_OBJ_FLAG_HIDDEN);
    if (g_notif.sd_icon && g_notif.icon_config.show_sdcard)
        lv_obj_clear_flag(g_notif.sd_icon, LV_OBJ_FLAG_HIDDEN);
    if (g_notif.time_label && g_notif.icon_config.show_time)
        lv_obj_clear_flag(g_notif.time_label, LV_OBJ_FLAG_HIDDEN);
}

static void update_display(void)
{
    if (!g_notif.visible || !g_notif.container)
    {
        return;
    }

    // Show app name or system info
    if (g_notif.showing_app_name && g_notif.app_name_label)
    {
        lv_obj_clear_flag(g_notif.app_name_label, LV_OBJ_FLAG_HIDDEN);

        // Hide system icons when showing app name
        if (g_notif.battery_label)
            lv_obj_add_flag(g_notif.battery_label, LV_OBJ_FLAG_HIDDEN);
        if (g_notif.wifi_icon)
            lv_obj_add_flag(g_notif.wifi_icon, LV_OBJ_FLAG_HIDDEN);
        if (g_notif.bt_icon)
            lv_obj_add_flag(g_notif.bt_icon, LV_OBJ_FLAG_HIDDEN);
        if (g_notif.sd_icon)
            lv_obj_add_flag(g_notif.sd_icon, LV_OBJ_FLAG_HIDDEN);
        if (g_notif.time_label)
            lv_obj_add_flag(g_notif.time_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        if (g_notif.app_name_label)
            lv_obj_add_flag(g_notif.app_name_label, LV_OBJ_FLAG_HIDDEN);

        // Show system icons
        if (g_notif.battery_label && g_notif.icon_config.show_battery)
        {
            lv_obj_clear_flag(g_notif.battery_label, LV_OBJ_FLAG_HIDDEN);
            char bat_text[16];
            snprintf(bat_text, sizeof(bat_text), "%s%d%%",
                     g_notif.status.charging ? "+" : "B",
                     g_notif.status.battery_percent);
            lv_label_set_text(g_notif.battery_label, bat_text);
        }

        if (g_notif.wifi_icon && g_notif.icon_config.show_wifi)
        {
            lv_obj_clear_flag(g_notif.wifi_icon, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_notif.wifi_icon, g_notif.status.wifi_connected ? "W+" : "W-");
        }

        if (g_notif.bt_icon && g_notif.icon_config.show_bluetooth)
        {
            lv_obj_clear_flag(g_notif.bt_icon, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_notif.bt_icon, g_notif.status.bt_connected ? "BT" : "--");
        }

        if (g_notif.sd_icon && g_notif.icon_config.show_sdcard)
        {
            lv_obj_clear_flag(g_notif.sd_icon, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_notif.sd_icon, g_notif.status.sdcard_mounted ? "SD" : "--");
        }

        if (g_notif.time_label && g_notif.icon_config.show_time)
        {
            lv_obj_clear_flag(g_notif.time_label, LV_OBJ_FLAG_HIDDEN);
            char time_text[8];
            snprintf(time_text, sizeof(time_text), "%02d:%02d",
                     g_notif.status.hour, g_notif.status.minute);
            lv_label_set_text(g_notif.time_label, time_text);
        }
    }

    // Loading indicator (overlays everything)
    if (g_notif.loading_active && g_notif.loading_label)
    {
        lv_obj_clear_flag(g_notif.loading_label, LV_OBJ_FLAG_HIDDEN);
    }
    else if (g_notif.loading_label)
    {
        lv_obj_add_flag(g_notif.loading_label, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t ugui_notification_init(void)
{
    if (g_notif.initialized)
    {
        UFLAKE_LOGW(TAG, "Notification bar already initialized");
        return UFLAKE_OK;
    }

    memset(&g_notif, 0, sizeof(notification_bar_t));

    // Default theme (will be overridden)
    g_notif.theme.primary = lv_color_hex(0x2196F3);
    g_notif.theme.notification_bg = lv_color_hex(0x000000);
    g_notif.theme.notification_fg = lv_color_hex(0xFFFFFF);
    g_notif.theme.opacity = 180; // Semi-transparent

    // Default icon config - show all
    g_notif.icon_config.show_battery = true;
    g_notif.icon_config.show_wifi = true;
    g_notif.icon_config.show_bluetooth = true;
    g_notif.icon_config.show_sdcard = true;
    g_notif.icon_config.show_time = true;

    // Default status
    g_notif.status.battery_percent = 100;
    g_notif.status.hour = 12;
    g_notif.status.minute = 0;

    // Create container (horizontal bar at top, full width, 30px height)
    g_notif.container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_notif.container, UGUI_DISPLAY_WIDTH, UGUI_NOTIFICATION_HEIGHT);
    lv_obj_set_pos(g_notif.container, 0, 0);
    lv_obj_set_style_bg_color(g_notif.container, g_notif.theme.notification_bg, 0);
    lv_obj_set_style_bg_opa(g_notif.container, g_notif.theme.opacity, 0);
    lv_obj_set_style_border_width(g_notif.container, 0, 0);
    lv_obj_set_style_pad_all(g_notif.container, 0, 0);
    lv_obj_clear_flag(g_notif.container, LV_OBJ_FLAG_SCROLLABLE);

    // Create battery label (left side of horizontal bar)
    g_notif.battery_label = lv_label_create(g_notif.container);
    lv_obj_set_style_text_color(g_notif.battery_label, g_notif.theme.notification_fg, 0);
    lv_label_set_text(g_notif.battery_label, "100%");
    lv_obj_align(g_notif.battery_label, LV_ALIGN_LEFT_MID, 5, 0);

    // Create WiFi icon (horizontal layout)
    g_notif.wifi_icon = lv_label_create(g_notif.container);
    lv_obj_set_style_text_color(g_notif.wifi_icon, g_notif.theme.notification_fg, 0);
    lv_label_set_text(g_notif.wifi_icon, "W-");
    lv_obj_align_to(g_notif.wifi_icon, g_notif.battery_label, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    // Create BT icon
    g_notif.bt_icon = lv_label_create(g_notif.container);
    lv_obj_set_style_text_color(g_notif.bt_icon, g_notif.theme.notification_fg, 0);
    lv_label_set_text(g_notif.bt_icon, "--");
    lv_obj_align_to(g_notif.bt_icon, g_notif.wifi_icon, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    // Create SD card icon
    g_notif.sd_icon = lv_label_create(g_notif.container);
    lv_obj_set_style_text_color(g_notif.sd_icon, g_notif.theme.notification_fg, 0);
    lv_label_set_text(g_notif.sd_icon, "SD");
    lv_obj_align_to(g_notif.sd_icon, g_notif.bt_icon, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    // Create time label (right side of horizontal bar)
    g_notif.time_label = lv_label_create(g_notif.container);
    lv_obj_set_style_text_color(g_notif.time_label, g_notif.theme.notification_fg, 0);
    lv_label_set_text(g_notif.time_label, "12:00");
    lv_obj_align(g_notif.time_label, LV_ALIGN_RIGHT_MID, -5, 0);

    // Create app name label (centered, hidden by default)
    g_notif.app_name_label = lv_label_create(g_notif.container);
    lv_obj_set_style_text_color(g_notif.app_name_label, g_notif.theme.notification_fg, 0);
    lv_label_set_text(g_notif.app_name_label, "");
    lv_obj_align(g_notif.app_name_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(g_notif.app_name_label, LV_OBJ_FLAG_HIDDEN);

    // Create loading label (centered, hidden by default)
    g_notif.loading_label = lv_label_create(g_notif.container);
    lv_obj_set_style_text_color(g_notif.loading_label, g_notif.theme.notification_fg, 0);
    lv_obj_align(g_notif.loading_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(g_notif.loading_label, LV_OBJ_FLAG_HIDDEN);

    // Bring notification bar to front (above all other content)
    lv_obj_move_foreground(g_notif.container);

    g_notif.visible = true;
    g_notif.initialized = true;

    UFLAKE_LOGI(TAG, "Notification bar initialized");

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_set_theme(const ugui_theme_t *theme)
{
    if (!g_notif.initialized || !theme)
    {
        return UFLAKE_ERROR;
    }

    g_notif.theme = *theme;

    // Update container style
    if (g_notif.container)
    {
        lv_obj_set_style_bg_color(g_notif.container, theme->notification_bg, 0);
        lv_obj_set_style_bg_opa(g_notif.container, theme->opacity, 0);
    }

    // Update text colors
    if (g_notif.battery_label)
        lv_obj_set_style_text_color(g_notif.battery_label, theme->notification_fg, 0);
    if (g_notif.wifi_icon)
        lv_obj_set_style_text_color(g_notif.wifi_icon, theme->notification_fg, 0);
    if (g_notif.bt_icon)
        lv_obj_set_style_text_color(g_notif.bt_icon, theme->notification_fg, 0);
    if (g_notif.sd_icon)
        lv_obj_set_style_text_color(g_notif.sd_icon, theme->notification_fg, 0);
    if (g_notif.time_label)
        lv_obj_set_style_text_color(g_notif.time_label, theme->notification_fg, 0);
    if (g_notif.app_name_label)
        lv_obj_set_style_text_color(g_notif.app_name_label, theme->notification_fg, 0);
    if (g_notif.loading_label)
        lv_obj_set_style_text_color(g_notif.loading_label, theme->notification_fg, 0);

    UFLAKE_LOGI(TAG, "Theme updated");

    return UFLAKE_OK;
}

// ============================================================================
// VISIBILITY CONTROL
// ============================================================================

uflake_result_t ugui_notification_show(void)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    if (g_notif.container)
    {
        lv_obj_clear_flag(g_notif.container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_notif.container); // Ensure it's on top after apps create UI
    }

    g_notif.visible = true;

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_hide(void)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    if (g_notif.container)
    {
        lv_obj_add_flag(g_notif.container, LV_OBJ_FLAG_HIDDEN);
    }

    g_notif.visible = false;

    return UFLAKE_OK;
}

bool ugui_notification_is_visible(void)
{
    return g_notif.visible;
}

// ============================================================================
// SYSTEM STATUS UPDATE
// ============================================================================

uflake_result_t ugui_notification_update_status(const ugui_system_status_t *status)
{
    if (!g_notif.initialized || !status)
    {
        return UFLAKE_ERROR;
    }

    g_notif.status = *status;
    update_display();

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_update_battery(uint8_t percent, bool charging)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_notif.status.battery_percent = percent > 100 ? 100 : percent;
    g_notif.status.charging = charging;
    update_display();

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_update_wifi(bool connected)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_notif.status.wifi_connected = connected;
    update_display();

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_update_bluetooth(bool connected)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_notif.status.bt_connected = connected;
    update_display();

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_update_sdcard(bool mounted)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_notif.status.sdcard_mounted = mounted;
    update_display();

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_update_time(uint8_t hour, uint8_t minute)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_notif.status.hour = hour % 24;
    g_notif.status.minute = minute % 60;
    update_display();

    return UFLAKE_OK;
}

// ============================================================================
// APP NAME DISPLAY
// ============================================================================

uflake_result_t ugui_notification_show_app_name(const char *app_name, uint32_t duration_ms)
{
    if (!g_notif.initialized || !app_name)
    {
        return UFLAKE_ERROR;
    }

    strncpy(g_notif.app_name, app_name, sizeof(g_notif.app_name) - 1);
    g_notif.app_name[sizeof(g_notif.app_name) - 1] = '\0';

    if (g_notif.app_name_label)
    {
        lv_label_set_text(g_notif.app_name_label, g_notif.app_name);
    }

    g_notif.showing_app_name = true;
    update_display();

    // Set LVGL timer if duration specified (runs safely in lv_timer_handler context)
    if (duration_ms > 0)
    {
        // Delete existing timer if any
        if (g_notif.app_name_timer != NULL)
        {
            lv_timer_delete(g_notif.app_name_timer);
        }
        // Create one-shot LVGL timer
        g_notif.app_name_timer = lv_timer_create(app_name_lv_timer_cb, duration_ms, NULL);
        if (g_notif.app_name_timer)
        {
            lv_timer_set_repeat_count(g_notif.app_name_timer, 1); // One-shot
        }
    }

    UFLAKE_LOGI(TAG, "Showing app name: %s", app_name);

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_clear_app_name(void)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_notif.showing_app_name = false;

    if (g_notif.app_name_timer != NULL)
    {
        lv_timer_delete(g_notif.app_name_timer);
        g_notif.app_name_timer = NULL;
    }

    update_display();

    return UFLAKE_OK;
}

// ============================================================================
// LOADING INDICATOR (simplified - no animation)
// ============================================================================

uflake_result_t ugui_notification_show_loading(bool show)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    g_notif.loading_active = show;

    if (show && g_notif.loading_label)
    {
        // Simple static loading text
        lv_label_set_text(g_notif.loading_label, "Loading...");
    }

    update_display();

    return UFLAKE_OK;
}

bool ugui_notification_is_loading(void)
{
    return g_notif.loading_active;
}

// ============================================================================
// ICON CONFIGURATION
// ============================================================================

uflake_result_t ugui_notification_set_icons(const ugui_notif_icons_t *icons)
{
    if (!g_notif.initialized || !icons)
    {
        return UFLAKE_ERROR;
    }

    g_notif.icon_config = *icons;
    update_display();

    return UFLAKE_OK;
}

uflake_result_t ugui_notification_get_icons(ugui_notif_icons_t *icons)
{
    if (!g_notif.initialized || !icons)
    {
        return UFLAKE_ERROR;
    }

    *icons = g_notif.icon_config;

    return UFLAKE_OK;
}

// ============================================================================
// ADVANCED
// ============================================================================

lv_obj_t *ugui_notification_get_object(void)
{
    return g_notif.container;
}

uflake_result_t ugui_notification_refresh(void)
{
    if (!g_notif.initialized)
    {
        return UFLAKE_ERROR;
    }

    update_display();

    return UFLAKE_OK;
}
