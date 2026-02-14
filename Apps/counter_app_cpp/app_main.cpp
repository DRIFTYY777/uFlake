// ============================================================================
// EXAMPLE: C++ File with C-Style Programming
// ============================================================================
// This demonstrates converting a .c file to .cpp while maintaining C-style
// programming practices. The changes are minimal:
// 1. Headers are wrapped in extern "C" blocks
// 2. Public functions are exported with extern "C"
// 3. Can use true/false instead of 1/0
// 4. Stricter type checking at compile time
// ============================================================================

#include <cstdio>  // C++ style header (or can use <stdio.h>)

// Wrap C headers in extern "C" block
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "appLoader.h"
}

static const char *TAG = "CounterAppCPP";

// ============================================================================
// APP MANIFEST - Define metadata for this app
// ============================================================================
// Note: In C++, designated initializers work the same way
static const app_manifest_t counter_manifest = {
    .name = "CounterCPP",
    .version = "1.3.0",
    .author = "DRIFTYY",
    .description = "Counter app in C++ with C-style code",
    .icon = "counter.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 4096,
    .priority = 5,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

// Forward declare entry point with C linkage
extern "C" void counter_cpp_app_main(void);

// Export app bundle for registration
// Note: This needs to be accessible from C code, so use extern "C"
extern "C" {
    const app_bundle_t counter_cpp_app = {
        .manifest = &counter_manifest,
        .entry_point = counter_cpp_app_main,
        .is_launcher = false};
}

// ============================================================================
// APP ENTRY POINT - Windows/Linux Style User Program
// ============================================================================

// Counter app that demonstrates C++ file with C-style programming
// Changes from pure C version:
// - Can use 'true' instead of '1'
// - Stricter type checking (catches more bugs)
// - Better compile-time error messages
// - Still looks and feels like C code!
extern "C" void counter_cpp_app_main(void)
{
    int counter = 0;

    ESP_LOGI(TAG, "Counter App (C++) Started - Same C-style code!");
    ESP_LOGI(TAG, "This demonstrates .cpp file with C programming style");

    // Pure infinite loop - C-style programming in C++ file
    // Note: Can use 'true' instead of '1' - but either works!
    while (true)  // or while (1) - both work
    {
        counter++;

        if (counter >= 50)
        {
            // Exit the app
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 100 milliseconds
    }

    ESP_LOGI(TAG, "Counter app (C++) completed successfully!");
}

// ============================================================================
// NOTES ON C++ WITH C-STYLE PROGRAMMING
// ============================================================================
// 
// Benefits gained:
// 1. Stricter type checking - catches more errors at compile time
// 2. Can use true/false keywords (cleaner than 1/0)
// 3. Better error messages from compiler
// 4. Can gradually add C++ features if needed (optional)
// 5. Compatible with C++ libraries
//
// Drawbacks:
// 1. Must wrap C headers in extern "C"
// 2. Must export C-callable functions with extern "C"
// 3. Slightly longer compile time
// 4. More strict about const-correctness
//
// Performance: No difference! C-style code in .cpp compiles to same machine code.
// Memory: No overhead with -fno-exceptions -fno-rtti (already set in uFlake)
//
// ============================================================================
