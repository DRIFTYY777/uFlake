#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "../../uAppLoader/appLoader.h"

static const char *TAG = "Launcher";

// ============================================================================
// APP MANIFEST - Define metadata for this app
// ============================================================================
static const app_manifest_t launcher_manifest = {
    .name = "Launcher",
    .version = "1.0.0",
    .author = "uFlake Team",
    .description = "Home screen and app launcher",
    .icon = "home.png",
    .type = APP_TYPE_LAUNCHER,
    .stack_size = 8192,
    .priority = 10,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false
};

// Forward declare entry point
void launcher_app_main(void);

// Export app bundle for registration (single line registration in main.cpp)
const app_bundle_t launcher_app = {
    .manifest = &launcher_manifest,
    .entry_point = launcher_app_main,
    .is_launcher = true
};

// ============================================================================
// APP ENTRY POINT
// ============================================================================

// Launcher is a special app that displays all available apps
void launcher_app_main(void)
{
    ESP_LOGI(TAG, "Launcher Started");

    app_descriptor_t *apps = NULL;
    uint32_t app_count = 0;
    int selected_index = 0;

    // Main launcher loop
    while (1)
    {
        // Get list of all apps from app loader
        if (app_loader_get_apps(&apps, &app_count) == UFLAKE_OK)
        {
            ESP_LOGI(TAG, "Found %lu apps", app_count);

            // Display app list (simplified - you'd use GUI here)
            printf("\n========== uFlake Apps ==========\n");
            for (uint32_t i = 0; i < app_count; i++)
            {
                // Skip launcher itself in the list
                if (apps[i].is_launcher)
                    continue;

                char marker = (i == selected_index) ? '>' : ' ';
                printf("%c %s v%s\n",
                       marker,
                       apps[i].manifest.name,
                       apps[i].manifest.version);
                printf("  %s\n", apps[i].manifest.description);
            }
            printf("================================\n");
            printf("Up/Down: Navigate | OK: Launch\n");
        }

        // TODO: Handle button inputs
        // if (button_up_pressed) { selected_index--; }
        // if (button_down_pressed) { selected_index++; }
        // if (button_ok_pressed) {
        //     // Launch selected app
        //     uint32_t app_id = apps[selected_index].app_id;
        //     ESP_LOGI(TAG, "Launching app ID %lu", app_id);
        //     app_loader_launch(app_id);
        //
        //     // Launcher is now paused by app loader
        //     // When app exits, launcher resumes automatically
        // }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
