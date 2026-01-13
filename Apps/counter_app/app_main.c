#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "../../uAppLoader/appLoader.h"

static const char *TAG = "CounterApp";

// ============================================================================
// APP MANIFEST - Define metadata for this app
// ============================================================================
static const app_manifest_t counter_manifest = {
    .name = "Counter",
    .version = "1.0.0",
    .author = "uFlake Team",
    .description = "Simple counter app",
    .icon = "counter.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 4096,
    .priority = 5,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

// Forward declare entry point
void counter_app_main(void);

// Export app bundle for registration (single line registration in main.cpp)
const app_bundle_t counter_app = {
    .manifest = &counter_manifest,
    .entry_point = counter_app_main,
    .is_launcher = false};

// ============================================================================
// APP ENTRY POINT
// ============================================================================

// Simple counter app that runs until force exit
void counter_app_main(void)
{
    int counter = 0;

    ESP_LOGI(TAG, "Counter App Started");

    // Main loop - runs until app loader terminates this task
    while (1)
    {
        ESP_LOGI(TAG, "Counter: %d", counter);
        counter++;

        // Display on screen (if you have display functions)
        // display_clear();
        // display_printf("Counter: %d\n\nHold Right+Back\nto exit", counter);

        // Sleep for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));

        // App loader will terminate this task when:
        // - User holds Right+Back for 2 seconds (force exit)
        // - Launcher terminates the app
        // No need to check exit conditions yourself!
    }
}
