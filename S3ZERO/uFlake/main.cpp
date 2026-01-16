#include <stdio.h>
#include "esp_log.h"

#include "uFlakeCore.h"

static const char *TAG = "MAIN";

extern "C"
{
    void app_main(void)
    {
        ESP_LOGI(TAG, "Starting uFlake OS...");

        // Initialize and start the entire OS
        uflake_core_init();

        ESP_LOGI(TAG, "uFlake OS running");
    }
}
