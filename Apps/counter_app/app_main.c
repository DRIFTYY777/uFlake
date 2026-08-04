#include <stdio.h>
#include "appLoader_simple.h"
#include "uGui_simple.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "CounterApp";

// ============================================================================
// APP MANIFEST - Define metadata for this app
// ============================================================================
static const app_manifest_t counter_manifest = {
    .name = "Counter",
    .version = "1.3.0",
    .author = "DRIFTYY",
    .description = "Simple counter app",
    .icon = "counter.png",
    .category = APP_CAT_UTILITY,
    .subtype = APP_SUBTYPE_GUI,
    .stack_size = 4096,
    .priority = PROCESS_PRIORITY_NORMAL,
    .requires_sdcard = false,
    .requires_network = false,
    .min_ram_bytes = 8192};

// Forward declare entry point
void counter_app_main(void);

// Export app bundle for registration
const app_bundle_t counter_app = {
    .manifest = counter_manifest,
    .entry_point = counter_app_main,
    .is_launcher = false};

// ============================================================================
// COUNTER APP STATE
// ============================================================================

static int counter = 0;
static lv_obj_t *counter_label = NULL;

// ============================================================================
// INPUT HANDLER
// ============================================================================

static void counter_input_handler(ugui_button_t btn, bool pressed, void *userdata)
{
    if (!pressed)
        return;

    (void)userdata;

    switch (btn)
    {
    case UGUI_BTN_OK:
        counter++;
        break;

    case UGUI_BTN_BACK:
        // Exit app
        uGui_exit_app();
        break;

    case UGUI_BTN_UP:
        counter += 10;
        break;

    case UGUI_BTN_DOWN:
        counter = (counter > 0) ? counter - 1 : 0;
        break;

    default:
        break;
    }

    // Update display
    if (counter_label != NULL)
    {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Counter: %d", counter);
        lv_label_set_text(counter_label, buffer);
    }
}

// ============================================================================
// UI CREATION
// ============================================================================

static void create_counter_ui(lv_obj_t *parent)
{
    if (parent == NULL)
    {
        ESP_LOGE(TAG, "No parent container");
        return;
    }

    // Main container
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Counter App");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, LV_FONT_MONTSERRAT_10, 0);

    // Counter display
    counter_label = lv_label_create(cont);
    lv_obj_set_style_text_font(counter_label, LV_FONT_MONTSERRAT_28, 0);
    lv_obj_set_style_text_color(counter_label, lv_color_hex(0x00ff00), 0);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Counter: %d", counter);
    lv_label_set_text(counter_label, buffer);

    // Instructions
    lv_obj_t *info = lv_label_create(cont);
    lv_label_set_text(info, "OK: +1 | UP: +10\nDOWN: -1 | BACK: Exit");
    lv_obj_set_style_text_color(info, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
}

// ============================================================================
// APP ENTRY POINT
// ============================================================================

void counter_app_main(void)
{
    ESP_LOGI(TAG, "Counter App Started");

    // Get container from GUI system
    lv_obj_t *container = uGui_get_content_container();
    if (container == NULL)
    {
        ESP_LOGE(TAG, "Failed to get content container");
        return;
    }

    // Register input handler
    uGui_register_input_handler(UGUI_BTN_OK, counter_input_handler, NULL);
    uGui_register_input_handler(UGUI_BTN_UP, counter_input_handler, NULL);
    uGui_register_input_handler(UGUI_BTN_DOWN, counter_input_handler, NULL);
    uGui_register_input_handler(UGUI_BTN_BACK, counter_input_handler, NULL);

    // Create UI
    create_counter_ui(container);

    ESP_LOGI(TAG, "Counter App UI created");
}
