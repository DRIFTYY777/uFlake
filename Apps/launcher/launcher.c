/**
 * @file launcher.cpp
 * @brief Launcher Application - Home screen and app list
 *
 * The launcher is a GUI app that displays all registered apps.
 * It's special because:
 * - It has is_launcher = true (auto-launched on boot)
 * - When other apps exit, they return here
 * - It queries app_loader for the app list
 */

#include <stdio.h>
#include <string.h>
#include "appLoader.h"
#include "uGui.h"
#include "lvgl.h"

static const char *TAG = "Launcher";

// ============================================================================
// APP MANIFEST - Define metadata for this app
// ============================================================================

static const app_manifest_t launcher_manifest = {
    .name = "Launcher",
    .version = "1.0.0",
    .author = "DRIFTYY",
    .description = "Home screen and app launcher",
    .icon = "home.png",
    .type = APP_TYPE_LAUNCHER,
    .stack_size = 8192,
    .priority = 10,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

// Forward declare entry point
void launcher_app_main(void);

// Export app bundle for registration (single line registration in main.cpp)
const app_bundle_t launcher_app = {
    .manifest = &launcher_manifest,
    .entry_point = launcher_app_main,
    .is_launcher = true};

// ============================================================================
// LAUNCHER UI STATE
// ============================================================================

static lv_obj_t *app_list = NULL;

// ============================================================================
// EVENT HANDLERS
// ============================================================================

static void app_btn_click_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return;

    // Get app_id stored in user data
    uint32_t app_id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    UFLAKE_LOGI(TAG, "Launching app ID: %lu", app_id);
    app_loader_launch(app_id);
}

// ============================================================================
// UI CREATION
// ============================================================================

static void create_launcher_ui(void)
{
    lv_obj_t *parent = uGui_get_content_container();
    if (parent == NULL)
    {
        UFLAKE_LOGE(TAG, "No content container");
        return;
    }

    UFLAKE_LOGI(TAG, "Content container: %p", (void *)parent);

    // Create main container
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, 5, 0);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "uFlake Apps");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    // App list
    app_list = lv_list_create(cont);
    lv_obj_set_size(app_list, lv_pct(100), lv_pct(85));
    lv_obj_set_style_bg_color(app_list, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(app_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(app_list, 8, 0);
    lv_obj_set_style_pad_all(app_list, 5, 0);

    // Get apps from loader
    app_descriptor_t *apps = NULL;
    uint32_t app_count = 0;

    if (app_loader_get_apps(&apps, &app_count) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to get app list");
        lv_obj_t *err_label = lv_label_create(app_list);
        lv_label_set_text(err_label, "Error loading apps");
        return;
    }

    UFLAKE_LOGI(TAG, "Found %lu apps", app_count);

    // Add each app as a list button
    lv_obj_t *first_btn = NULL;
    int btn_count = 0;
    for (uint32_t i = 0; i < app_count; i++)
    {
        // Skip launcher itself
        if (apps[i].is_launcher)
            continue;

        // Skip services (background only)
        if (apps[i].manifest.type == APP_TYPE_SERVICE)
            continue;

        UFLAKE_LOGI(TAG, "Adding app: %s", apps[i].manifest.name);

        // Create list button
        lv_obj_t *btn = lv_list_add_btn(app_list, NULL, apps[i].manifest.name);
        lv_obj_add_event_cb(btn, app_btn_click_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)apps[i].app_id);

        // Style the button
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x666666), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn, lv_color_white(), 0);

        // Add to group for navigation
        uGui_add_to_group(btn);

        if (first_btn == NULL)
            first_btn = btn;

        btn_count++;
    }

    UFLAKE_LOGI(TAG, "Created %d app buttons", btn_count);

    // Focus first app
    if (first_btn != NULL)
    {
        lv_group_focus_obj(first_btn);
    }
    else
    {
        // No apps - show message
        lv_obj_t *empty_label = lv_label_create(app_list);
        lv_label_set_text(empty_label, "No apps installed");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x888888), 0);
    }
}

// ============================================================================
// APP ENTRY POINT
// ============================================================================

void launcher_app_main(void)
{
    UFLAKE_LOGI(TAG, "Launcher Started");

    // Register ourselves as the launcher with uGui
    // This way when apps exit, uGui calls our entry point again
    uGui_set_launcher(launcher_app_main);
    UFLAKE_LOGI(TAG, "Registered as launcher");

    // Create the UI
    create_launcher_ui();

    UFLAKE_LOGI(TAG, "Launcher UI created");
    // No loop needed - LVGL handles input events
    // When an app is launched, this function's UI is cleared
    // When app exits, launcher_app_main() is called again
}
