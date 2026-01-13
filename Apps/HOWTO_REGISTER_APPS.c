// Example: How to register apps in main.cpp (SIMPLIFIED - ONE LINE PER APP!)

#include "appLoader.h"

// Import app bundles (defined in each app's source file)
extern const app_bundle_t launcher_app; // From Apps/launcher/app_main.c
extern const app_bundle_t counter_app;  // From Apps/counter_app/app_main.c

void register_all_apps(void)
{
    // Initialize app loader first
    if (app_loader_init() != UFLAKE_OK)
    {
        ESP_LOGE("MAIN", "Failed to initialize app loader");
        return;
    }

    // ========================================================================
    // Register apps - ONE LINE EACH! (like Flipper Zero)
    // ========================================================================

    uint32_t launcher_id = app_loader_register(&launcher_app); // That's it!
    uint32_t counter_id = app_loader_register(&counter_app);   // That's it!

    // Check registration
    if (launcher_id == 0)
    {
        ESP_LOGE("MAIN", "Failed to register launcher");
        return;
    }

    if (counter_id == 0)
    {
        ESP_LOGE("MAIN", "Failed to register counter app");
        return;
    }

    // ========================================================================
    // Launch Launcher (starts the UI)
    // ========================================================================
    ESP_LOGI("MAIN", "Launching launcher...");
    if (app_loader_launch(launcher_id) != UFLAKE_OK)
    {
        ESP_LOGE("MAIN", "Failed to launch launcher");
        return;
    }

    ESP_LOGI("MAIN", "App system initialized successfully");
}

// ============================================================================
// Even simpler - register and launch in 3 lines total:
// ============================================================================
void minimal_example(void)
{
    app_loader_init();
    uint32_t launcher_id = app_loader_register(&launcher_app);
    app_loader_launch(launcher_id);
}

// Call this from app_main() in main.cpp:
// register_all_apps();
