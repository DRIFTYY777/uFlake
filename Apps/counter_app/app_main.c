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
// APP ENTRY POINT - Windows/Linux Style User Program
// ============================================================================

// Counter app that demonstrates OS-level infinite loop protection
// This is written like a normal Windows/Linux program - no embedded concerns!
void counter_app_main(void)
{
    int counter = 0;

    ESP_LOGI(TAG, "Counter App Started - Testing OS-level infinite loop protection!");
    ESP_LOGI(TAG, "This app will spin like a Windows/Linux program - no watchdog concerns!");

    // Pure infinite loop - just like you'd write in Windows/Linux
    // No delays, no yields, no hardware concerns - pure user code
    while (1)
    {
        counter++;

        // Log every million iterations to show progress without spam
        if (counter % 1000000 == 0)
        {
            ESP_LOGI(TAG, "Infinite loop: %d million iterations", counter / 1000000);
        }

        // Exit after reasonable test (50 million iterations)
        if (counter >= 50000000)
        {
            ESP_LOGI(TAG, "SUCCESS: Counter reached %d - OS protection worked!", counter);
            break;
        }

        // NO DELAYS - Pure busy loop like Windows/Linux user program
        // The uFlake OS kernel handles ALL hardware management automatically
    }

    ESP_LOGI(TAG, "Counter app completed - uFlake OS handled infinite loop successfully!");
}
