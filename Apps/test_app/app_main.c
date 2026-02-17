#include <stdio.h>

#include "esp_log.h"
#include "appLoader.h"

static const char *TAG = "TestApp";

static const app_manifest_t test_manifest = {
    .name = "test",
    .version = "1.0.0",
    .author = "DRIFTYY",
    .description = "simple CPU eating test app",
    .icon = "counter.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 4096,
    .priority = 5,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

void test_app_main(void);

const app_bundle_t test_app = {
    .manifest = &test_manifest,
    .entry_point = test_app_main,
    .is_launcher = false};

void test_app_main(void)
{
    UFLAKE_LOGI(TAG, "Test App Started - Testing OS-level infinite loop protection!");

    // Infinite loop should yield to prevent starving other tasks
    while (1)
    {
        // Yield CPU to allow kernel and other tasks to run
        uflake_process_yield(100); // Yield for 100ms
    }
}
