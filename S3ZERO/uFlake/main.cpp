#include <stdio.h>
#include "esp_log.h"

#include "uFlakeCore.h"
#include "kernel.h"

static const char *TAG = "MAIN";

extern "C"
{
    void app_main(void)
    {
        UFLAKE_LOGI(TAG, "Starting uFlake OS...");

        // Initialize and start the entire OS
        uflake_core_init();

        UFLAKE_LOGI(TAG, "uFlake OS running");
    }
}
