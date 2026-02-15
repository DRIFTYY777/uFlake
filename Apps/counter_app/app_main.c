#include <stdio.h>
#include "appLoader.h"
#include "logger.h"

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

    UFLAKE_LOGI(TAG, "Counter App Started - Testing OS-level infinite loop protection!");
    UFLAKE_LOGI(TAG, "This app will spin like a Windows/Linux program - no watchdog concerns!");

    // Pure infinite loop - just like you'd write in Windows/Linux
    // No delays, no yields, no hardware concerns - pure user code
    while (1)
    {
        counter++;

        if (counter >= 50)
        {
            // Exit the app
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 100 milliseconds
    }

    UFLAKE_LOGI(TAG, "Counter app completed - uFlake OS handled infinite loop successfully!");
}
