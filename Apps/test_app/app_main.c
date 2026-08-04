#include <stdio.h>
#include <string.h>

#include "appLoader_simple.h"
#include "kernel.h"
#include "esp_log.h"

static const char *TAG = "TestApp";

static const app_manifest_t test_manifest = {
    .name = "test",
    .version = "1.0.0",
    .author = "DRIFTYY",
    .description = "Resource Manager Testing Application",
    .icon = "counter.png",
    .category = APP_CAT_UTILITY,
    .subtype = APP_SUBTYPE_NON_GUI,
    .stack_size = 8192,
    .priority = PROCESS_PRIORITY_NORMAL,
    .requires_sdcard = false,
    .requires_network = false,
    .min_ram_bytes = 8192
};

void test_app_main(void);

const app_bundle_t test_app = {
    .manifest = test_manifest,
    .entry_point = test_app_main,
    .is_launcher = false
};

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, test_name)           \
    do                                              \
    {                                               \
        if (condition)                              \
        {                                            \
            ESP_LOGI(TAG, "✓ PASS: %s", test_name); \
            tests_passed++;                         \
        }                                           \
        else                                        \
        {                                           \
            ESP_LOGE(TAG, "✗ FAIL: %s", test_name); \
            tests_failed++;                         \
        }                                           \
    } while (0)

void print_test_summary(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "     TEST SUMMARY      ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Tests Passed: %d", tests_passed);
    ESP_LOGI(TAG, "Tests Failed: %d", tests_failed);
    int total = tests_passed + tests_failed;
    if (total > 0)
    {
        int pass_percentage = (tests_passed * 100) / total;
        ESP_LOGI(TAG, "Success Rate: %d%%", pass_percentage);
    }
    ESP_LOGI(TAG, "========================================\n");
}

void test_app_main(void)
{
    ESP_LOGI(TAG, "\n\nTest App Started");
    ESP_LOGI(TAG, "Running basic functionality tests...\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    // Run basic tests
    TEST_ASSERT(1 == 1, "Basic math test");
    TEST_ASSERT(2 + 2 == 4, "Addition test");
    TEST_ASSERT(10 - 5 == 5, "Subtraction test");

    // Print summary
    print_test_summary();

    ESP_LOGI(TAG, "Test App completed");
}
