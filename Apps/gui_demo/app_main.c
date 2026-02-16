/**
 * @file app_main.c
 * @brief GUI Demo App - Demonstrates all uFlake GUI features
 *
 * This app showcases:
 * - App window creation (no while loop needed!)
 * - Automatic focus management
 * - Themed widgets
 * - Dialog boxes
 * - Navigation
 * - Safe cleanup
 */

#include <stdio.h>
#include "appLoader.h"
#include "uGui.h"
#include "logger.h"

static const char *TAG = "GUIDemo";

// ============================================================================
// APP MANIFEST
// ============================================================================

static const app_manifest_t gui_demo_manifest = {
    .name = "GUI Demo",
    .version = "1.0.0",
    .author = "uFlake Team",
    .description = "Demonstrates GUI features",
    .icon = "demo.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 8192,
    .priority = 5,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

// Forward declare entry point
void gui_demo_app_main(void);

// Export app bundle
const app_bundle_t gui_demo_app = {
    .manifest = &gui_demo_manifest,
    .entry_point = gui_demo_app_main,
    .is_launcher = false};

// ============================================================================
// EVENT HANDLERS
// ============================================================================

static void btn_dialog_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ugui_dialog_ok("Demo Dialog", "This is an OK dialog!", NULL, NULL);
        UFLAKE_LOGI(TAG, "Dialog button clicked");
    }
}

static void btn_loading_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t *loading = ugui_show_loading("Loading...", UGUI_LOADING_DOTS);

        // Simulate work (in real app, do this in separate task)
        vTaskDelay(pdMS_TO_TICKS(2000));

        ugui_hide_loading(loading);
        ugui_show_message("Done!", 1000);

        UFLAKE_LOGI(TAG, "Loading demo completed");
    }
}

static void btn_theme_clicked(lv_event_t *e)
{
    static uint8_t theme_index = 0;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // Cycle through themes
        theme_index = (theme_index + 1) % 3;

        switch (theme_index)
        {
        case 0:
            ugui_theme_apply_dark();
            ugui_show_message("Dark theme", 1000);
            break;
        case 1:
            ugui_theme_apply_light();
            ugui_show_message("Light theme", 1000);
            break;
        case 2:
            ugui_theme_apply_blue();
            ugui_show_message("Blue theme", 1000);
            break;
        }

        UFLAKE_LOGI(TAG, "Theme changed to %d", theme_index);
    }
}

static void btn_fullscreen_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ugui_appwin_t *window = (ugui_appwin_t *)lv_event_get_user_data(e);

        static bool fullscreen = false;
        fullscreen = !fullscreen;

        ugui_appwindow_set_fullscreen(window, fullscreen);

        UFLAKE_LOGI(TAG, "Fullscreen: %s", fullscreen ? "ON" : "OFF");
    }
}

static void dialog_callback(ugui_dialog_btn_t button, void *userdata)
{
    (void)userdata;

    const char *btn_name = (button == UGUI_DIALOG_BTN_YES) ? "YES" : "NO";
    UFLAKE_LOGI(TAG, "Dialog result: %s", btn_name);

    if (button == UGUI_DIALOG_BTN_YES)
    {
        ugui_show_message("You clicked YES!", 1500);
    }
    else
    {
        ugui_show_message("You clicked NO!", 1500);
    }
}

static void btn_yes_no_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ugui_dialog_yes_no("Question", "Do you like uFlake?", dialog_callback, NULL);
    }
}

// ============================================================================
// APP ENTRY POINT
// ============================================================================

void gui_demo_app_main(void)
{
    UFLAKE_LOGI(TAG, "GUI Demo App Started");

    // Check if GUI is initialized
    if (!uGui_is_initialized())
    {
        UFLAKE_LOGE(TAG, "GUI not initialized!");
        return;
    }

    // Create app window (automatic focus, safe cleanup)
    ugui_appwin_config_t config = {
        .app_name = "GUI Demo",
        .width = 0,  // Default: 240px
        .height = 0, // Default: 215px (below notification bar)
        .flags = UGUI_APPWIN_FLAG_NONE,
        .bg_color = lv_color_hex(0x000000),
        .bg_opacity = 255};

    // Get app ID (would be passed from app loader in real scenario)
    static uint32_t demo_app_id = 999; // Placeholder

    ugui_appwin_t *window = ugui_appwindow_create(&config, demo_app_id);
    if (!window)
    {
        UFLAKE_LOGE(TAG, "Failed to create app window");
        return;
    }

    UFLAKE_LOGI(TAG, "App window created successfully");

    // Get content container (add all UI to this)
    lv_obj_t *content = ugui_appwindow_get_content(window);

    // Title label
    lv_obj_t *title = ugui_label_create(content, "GUI Demo App");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    // Info label
    lv_obj_t *info = ugui_label_create(content, "Demonstrates all GUI features");
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 25);

    // Button: Show Dialog
    lv_obj_t *btn_dialog = ugui_button_create(content, "OK Dialog", 200, 35);
    lv_obj_align(btn_dialog, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_add_event_cb(btn_dialog, btn_dialog_clicked, LV_EVENT_CLICKED, NULL);
    ugui_appwindow_add_focusable(window, btn_dialog);

    // Button: Yes/No Dialog
    lv_obj_t *btn_yesno = ugui_button_create(content, "Yes/No Dialog", 200, 35);
    lv_obj_align(btn_yesno, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_add_event_cb(btn_yesno, btn_yes_no_clicked, LV_EVENT_CLICKED, NULL);
    ugui_appwindow_add_focusable(window, btn_yesno);

    // Button: Loading Demo
    lv_obj_t *btn_loading = ugui_button_create(content, "Loading Demo", 200, 35);
    lv_obj_align(btn_loading, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_add_event_cb(btn_loading, btn_loading_clicked, LV_EVENT_CLICKED, NULL);
    ugui_appwindow_add_focusable(window, btn_loading);

    // Button: Change Theme
    lv_obj_t *btn_theme = ugui_button_create(content, "Change Theme", 200, 35);
    lv_obj_align(btn_theme, LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_add_event_cb(btn_theme, btn_theme_clicked, LV_EVENT_CLICKED, NULL);
    ugui_appwindow_add_focusable(window, btn_theme);

    // Button: Toggle Fullscreen
    lv_obj_t *btn_fullscreen = ugui_button_create(content, "Toggle Fullscreen", 200, 35);
    lv_obj_align(btn_fullscreen, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btn_fullscreen, btn_fullscreen_clicked, LV_EVENT_CLICKED, window);
    ugui_appwindow_add_focusable(window, btn_fullscreen);

    // Fade in animation
    ugui_appwindow_fade_in(window, 300);

    UFLAKE_LOGI(TAG, "UI created with %d buttons", 5);
    UFLAKE_LOGI(TAG, "Use navigation buttons to interact!");
    UFLAKE_LOGI(TAG, "App exiting - GUI stays alive (no while loop!)");

    // NO WHILE LOOP!
    // LVGL handles everything, window stays until app loader closes it
    // When app exits normally, this is fine. App loader will clean up.

    // In a real app, the app loader would call ugui_appwindow_destroy(window)
    // when terminating the app, which safely cleans everything up.
}
