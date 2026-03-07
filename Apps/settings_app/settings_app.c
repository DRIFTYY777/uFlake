/**
 * @file settings_app.cpp
 * @brief Settings Application - Example GUI app using uGui
 *
 * This demonstrates how to create a GUI app for uFlake:
 * - GUI apps don't need a main loop (LVGL handles events)
 * - Just set up UI elements and event handlers
 * - App exits when user presses ESC (handled by uGui)
 */

#include <stdio.h>
#include <string.h>
#include "appLoader.h"
#include "uGui.h"
#include "lvgl.h"

static const char *TAG = "SettingsApp";

// ============================================================================
// APP MANIFEST
// ============================================================================
static const app_manifest_t settings_manifest = {
    .name = "Settings",
    .version = "1.0.0",
    .author = "uFlake",
    .description = "System settings and configuration",
    .icon = "settings.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 4096,
    .priority = 5,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

// Forward declare entry point
void settings_app_main(void);

// Export app bundle for registration
const app_bundle_t settings_app = {
    .manifest = &settings_manifest,
    .entry_point = settings_app_main,
    .is_launcher = false};

// ============================================================================
// UI EVENT HANDLERS
// ============================================================================

static void brightness_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    UFLAKE_LOGI(TAG, "Brightness: %ld%%", value);
    // TODO: Apply brightness setting
}

static void theme_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        UFLAKE_LOGI(TAG, "Theme toggle clicked");
        // TODO: Toggle theme
    }
}

static void wifi_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
    UFLAKE_LOGI(TAG, "WiFi: %s", state ? "ON" : "OFF");
    // TODO: Toggle WiFi
}

// ============================================================================
// APP ENTRY POINT
// ============================================================================

void settings_app_main(void)
{
    UFLAKE_LOGI(TAG, "Settings App Started");

    // Get the content container from uGui
    lv_obj_t *parent = uGui_get_content_container();
    if (parent == NULL)
    {
        UFLAKE_LOGE(TAG, "Failed to get content container");
        return;
    }

    // Create main container with vertical scroll
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_pad_gap(cont, 10, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    // --- Display Section ---
    lv_obj_t *display_section = lv_obj_create(cont);
    lv_obj_set_size(display_section, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(display_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(display_section, 8, 0);
    lv_obj_set_style_pad_gap(display_section, 5, 0);
    lv_obj_set_style_bg_color(display_section, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(display_section, 8, 0);

    lv_obj_t *display_label = lv_label_create(display_section);
    lv_label_set_text(display_label, "Display");
    lv_obj_set_style_text_color(display_label, lv_color_hex(0xAAAAAA), 0);

    // Brightness slider row
    lv_obj_t *bright_row = lv_obj_create(display_section);
    lv_obj_set_size(bright_row, lv_pct(100), 40);
    lv_obj_set_flex_flow(bright_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bright_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bright_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bright_row, 0, 0);
    lv_obj_set_style_pad_all(bright_row, 0, 0);

    lv_obj_t *bright_label = lv_label_create(bright_row);
    lv_label_set_text(bright_label, "Brightness");
    lv_obj_set_style_text_color(bright_label, lv_color_white(), 0);

    lv_obj_t *slider = lv_slider_create(bright_row);
    lv_obj_set_width(slider, 120);
    lv_slider_set_value(slider, 75, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    uGui_add_to_group(slider);

    // Theme toggle button
    lv_obj_t *theme_btn = lv_btn_create(display_section);
    lv_obj_set_size(theme_btn, lv_pct(100), 35);
    lv_obj_add_event_cb(theme_btn, theme_btn_cb, LV_EVENT_CLICKED, NULL);
    uGui_add_to_group(theme_btn);

    lv_obj_t *theme_btn_label = lv_label_create(theme_btn);
    lv_label_set_text(theme_btn_label, "Toggle Dark/Light Theme");
    lv_obj_center(theme_btn_label);

    // --- Connectivity Section ---
    lv_obj_t *conn_section = lv_obj_create(cont);
    lv_obj_set_size(conn_section, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(conn_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(conn_section, 8, 0);
    lv_obj_set_style_pad_gap(conn_section, 5, 0);
    lv_obj_set_style_bg_color(conn_section, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(conn_section, 8, 0);

    lv_obj_t *conn_label = lv_label_create(conn_section);
    lv_label_set_text(conn_label, "Connectivity");
    lv_obj_set_style_text_color(conn_label, lv_color_hex(0xAAAAAA), 0);

    // WiFi switch row
    lv_obj_t *wifi_row = lv_obj_create(conn_section);
    lv_obj_set_size(wifi_row, lv_pct(100), 35);
    lv_obj_set_flex_flow(wifi_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wifi_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(wifi_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_row, 0, 0);
    lv_obj_set_style_pad_all(wifi_row, 0, 0);

    lv_obj_t *wifi_label = lv_label_create(wifi_row);
    lv_label_set_text(wifi_label, "WiFi");
    lv_obj_set_style_text_color(wifi_label, lv_color_white(), 0);

    lv_obj_t *wifi_sw = lv_switch_create(wifi_row);
    lv_obj_add_event_cb(wifi_sw, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    uGui_add_to_group(wifi_sw);

    // --- About Section ---
    lv_obj_t *about_section = lv_obj_create(cont);
    lv_obj_set_size(about_section, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(about_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(about_section, 8, 0);
    lv_obj_set_style_pad_gap(about_section, 5, 0);
    lv_obj_set_style_bg_color(about_section, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(about_section, 8, 0);

    lv_obj_t *about_label = lv_label_create(about_section);
    lv_label_set_text(about_label, "About");
    lv_obj_set_style_text_color(about_label, lv_color_hex(0xAAAAAA), 0);

    lv_obj_t *version_label = lv_label_create(about_section);
    lv_label_set_text(version_label, "uFlake OS v1.0.0");
    lv_obj_set_style_text_color(version_label, lv_color_white(), 0);

    lv_obj_t *hw_label = lv_label_create(about_section);
    lv_label_set_text(hw_label, "ESP32-S3 @ 240MHz");
    lv_obj_set_style_text_color(hw_label, lv_color_hex(0x888888), 0);

    // Focus first interactive element
    lv_group_focus_obj(slider);

    UFLAKE_LOGI(TAG, "Settings UI created - press ESC to exit");
    // No loop needed - LVGL handles everything
    // App stays "running" until user presses ESC
}
