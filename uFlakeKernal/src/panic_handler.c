#include "panic_handler.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "PANIC";
static uflake_panic_info_t last_panic_info = {0};
static bool panic_occurred = false;

uflake_result_t uflake_panic_init(void)
{
    ESP_LOGI(TAG, "Panic handler initialized");
    return UFLAKE_OK;
}

void uflake_panic_trigger(panic_reason_t reason, const char *message)
{
    // Save panic information
    last_panic_info.reason = reason;
    last_panic_info.timestamp = xTaskGetTickCount();
    last_panic_info.task_handle = (uint32_t)xTaskGetCurrentTaskHandle();

    const char *task_name = pcTaskGetName(NULL);
    strncpy(last_panic_info.task_name, task_name ? task_name : "unknown",
            sizeof(last_panic_info.task_name) - 1);

    if (message)
    {
        strncpy(last_panic_info.message, message, sizeof(last_panic_info.message) - 1);
    }

    panic_occurred = true;

    ESP_LOGE(TAG, "╔════════════════════════════════════════════════════════════╗");
    ESP_LOGE(TAG, "║                    KERNEL PANIC                            ║");
    ESP_LOGE(TAG, "╠════════════════════════════════════════════════════════════╣");
    ESP_LOGE(TAG, "║ Reason: %-52s ║",
             reason == PANIC_REASON_STACK_OVERFLOW ? "Stack Overflow" : reason == PANIC_REASON_MEMORY_CORRUPTION ? "Memory Corruption"
                                                                    : reason == PANIC_REASON_WATCHDOG_TIMEOUT    ? "Watchdog Timeout"
                                                                    : reason == PANIC_REASON_ASSERTION_FAILED    ? "Assertion Failed"
                                                                    : reason == PANIC_REASON_USER_ABORT          ? "User Abort"
                                                                                                                 : "Unknown");
    ESP_LOGE(TAG, "║ Task: %-54s ║", last_panic_info.task_name);
    ESP_LOGE(TAG, "║ Message: %-51s ║", message ? message : "(none)");
    ESP_LOGE(TAG, "╚════════════════════════════════════════════════════════════╝");

    // For severe panics, trigger a system restart
    if (reason == PANIC_REASON_MEMORY_CORRUPTION ||
        reason == PANIC_REASON_STACK_OVERFLOW)
    {
        ESP_LOGE(TAG, "Critical panic - system will restart in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
}

void uflake_panic_check(void)
{
    // Check for various panic conditions

    // Check free heap
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 1024)
    { // Less than 1KB free
        uflake_panic_trigger(PANIC_REASON_MEMORY_CORRUPTION, "Low memory");
    }

    // Check stack overflow (simplified)
    UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(NULL);
    if (stack_high_water < 256)
    { // Less than 256 bytes stack remaining
        uflake_panic_trigger(PANIC_REASON_STACK_OVERFLOW, "Stack overflow detected");
    }
}

uflake_result_t uflake_panic_get_last_info(uflake_panic_info_t *info)
{
    if (!info)
        return UFLAKE_ERROR_INVALID_PARAM;

    if (!panic_occurred)
    {
        return UFLAKE_ERROR_NOT_FOUND;
    }

    *info = last_panic_info;
    return UFLAKE_OK;
}
