/**
 * @file launcher.c
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
#include "appLoader_simple.h"
#include "uGui_simple.h"
#include "lvgl.h"
#include "esp_log.h"

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
    .category = APP_CAT_SYSTEM,
    .subtype = APP_SUBTYPE_GUI,
    .stack_size = 8192,
    .priority = PROCESS_PRIORITY_NORMAL,
    .requires_sdcard = false,
    .requires_network = false,
    .min_ram_bytes = 16384};

// Forward declare entry point
void launcher_app_main(void);

// Export app bundle for registration
const app_bundle_t launcher_app = {
    .manifest = launcher_manifest,
    .entry_point = launcher_app_main,
    .is_launcher = true};

// ============================================================================
// LAUNCHER UI STATE
// ============================================================================

static lv_obj_t *app_list = NULL;
static uint32_t selected_app_id = 0;

// ============================================================================
// INPUT HANDLER
// ============================================================================

static void launcher_input_handler(ugui_button_t btn, bool pressed, void *userdata)
{
    if (!pressed)
        return; // Only handle press events

    (void)userdata;

    switch (btn)
    {
    case UGUI_BTN_OK:
        if (selected_app_id != 0)
        {
            ESP_LOGI(TAG, "Launching app ID: %lu", selected_app_id);
            app_loader_launch_app(selected_app_id);
        }
        break;

    case UGUI_BTN_DOWN:
        // TODO: Handle navigation down
        break;

    case UGUI_BTN_UP:
        // TODO: Handle navigation up
        break;

    default:
        break;
    }
}

// LVGL click handler
static void app_btn_click_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return;

    uint32_t app_id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    selected_app_id = app_id;

    ESP_LOGI(TAG, "App button clicked: %lu", app_id);
    if (app_id != 0)
    {
        app_loader_launch_app(app_id);
    }
}

// ============================================================================
// UI CREATION
// ============================================================================

static void create_launcher_ui(lv_obj_t *parent)
{
    if (parent == NULL)
    {
        ESP_LOGE(TAG, "No content container");
        return;
    }

    ESP_LOGI(TAG, "Content container: %p", (void *)parent);

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
    uint32_t app_count = 0;
    app_descriptor_t *apps = app_loader_get_app_list(&app_count);
    if (apps == NULL || app_count == 0)
    {
        ESP_LOGE(TAG, "Failed to get app list");
        lv_obj_t *err_label = lv_label_create(app_list);
        lv_label_set_text(err_label, "No apps found");
        return;
    }

    ESP_LOGI(TAG, "Found %lu apps", app_count);

    // Add each app as a list button
    lv_obj_t *first_btn = NULL;
    int btn_count = 0;
    for (uint32_t i = 0; i < app_count; i++)
    {
        // Skip launcher itself
        if (apps[i].is_launcher)
            continue;

        // Skip non-GUI apps
        if (apps[i].manifest.subtype != APP_SUBTYPE_GUI)
            continue;

        ESP_LOGI(TAG, "Adding app: %s", apps[i].manifest.name);

        // Create list button
        lv_obj_t *btn = lv_list_add_btn(app_list, NULL, apps[i].manifest.name);
        lv_obj_add_event_cb(btn, app_btn_click_handler, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)apps[i].app_id);

        // Style the button
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x666666), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn, lv_color_white(), 0);

        if (first_btn == NULL)
            first_btn = btn;

        btn_count++;
    }

    ESP_LOGI(TAG, "Created %d app buttons", btn_count);

    if (first_btn == NULL)
    {
        // No apps - show message
        ESP_LOGI(TAG, "No GUI apps found");
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
    ESP_LOGI(TAG, "Launcher Started");

    // Get container from GUI system
    lv_obj_t *container = uGui_get_content_container();
    if (container == NULL)
    {
        ESP_LOGE(TAG, "Failed to get content container");
        return;
    }

    // Register input handler for this app
    uGui_register_input_handler(UGUI_BTN_OK, launcher_input_handler, NULL);
    uGui_register_input_handler(UGUI_BTN_UP, launcher_input_handler, NULL);
    uGui_register_input_handler(UGUI_BTN_DOWN, launcher_input_handler, NULL);

    // Create the UI
    create_launcher_ui(container);

    ESP_LOGI(TAG, "Launcher UI created");
    // No loop needed - LVGL handles events in GUI task
    // When an app is launched, this app is unloaded
    // When other apps exit, launcher_app_main() is called again
}
