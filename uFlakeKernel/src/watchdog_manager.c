#include "watchdog_manager.h"
#include "memory_manager.h"
#include "panic_handler.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

static const char *TAG = "WATCHDOG";

// Add proper node structure
typedef struct watchdog_node_t
{
    uflake_watchdog_t watchdog;
    struct watchdog_node_t *next;
} watchdog_node_t;

static watchdog_node_t *watchdog_list = NULL;
static uint32_t next_watchdog_id = 1;
static SemaphoreHandle_t watchdog_mutex = NULL;

uflake_result_t uflake_watchdog_init(void)
{
    watchdog_mutex = xSemaphoreCreateMutex();
    if (!watchdog_mutex)
    {
        ESP_LOGE(TAG, "Failed to create watchdog mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    // Configure ESP32-S3 hardware task watchdog
    // ARCHITECTURE: Only kernel_task will be subscribed and feed this watchdog
    // User tasks are completely isolated from hardware watchdog
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,     // 30 seconds - generous timeout for kernel
        .idle_core_mask = 0,     // Don't monitor IDLE tasks (they don't do work)
        .trigger_panic = false}; // Log warnings only, don't reset system

    esp_err_t err = esp_task_wdt_reconfigure(&wdt_config);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "WDT reconfigure: %s (using defaults)", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Watchdog Manager Initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_watchdog_create(const char *name, watchdog_type_t type,
                                       uint32_t timeout_ms, uint32_t *watchdog_id)
{
    if (!name || !watchdog_id || timeout_ms == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(watchdog_mutex, portMAX_DELAY);

    //  Allocate node properly
    watchdog_node_t *node = (watchdog_node_t *)uflake_malloc(sizeof(watchdog_node_t), UFLAKE_MEM_INTERNAL);
    if (!node)
    {
        xSemaphoreGive(watchdog_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    node->watchdog.watchdog_id = next_watchdog_id++;
    node->watchdog.type = type;
    node->watchdog.timeout_ms = timeout_ms;
    node->watchdog.last_feed = xTaskGetTickCount();
    node->watchdog.is_active = true;

    strncpy(node->watchdog.name, name, sizeof(node->watchdog.name) - 1);
    node->watchdog.name[sizeof(node->watchdog.name) - 1] = '\0';

    //  Insert into linked list
    node->next = watchdog_list;
    watchdog_list = node;

    *watchdog_id = node->watchdog.watchdog_id;

    xSemaphoreGive(watchdog_mutex);
    ESP_LOGI(TAG, "Created watchdog '%s' with ID: %d, timeout: %d ms", name, (int)*watchdog_id, (int)timeout_ms);

    return UFLAKE_OK;
}

uflake_result_t uflake_watchdog_feed_by_id(uint32_t watchdog_id)
{
    xSemaphoreTake(watchdog_mutex, portMAX_DELAY);

    //  Search linked list
    watchdog_node_t *current = watchdog_list;
    while (current)
    {
        if (current->watchdog.watchdog_id == watchdog_id)
        {
            current->watchdog.last_feed = xTaskGetTickCount();
            xSemaphoreGive(watchdog_mutex);
            ESP_LOGV(TAG, "Fed watchdog ID: %d", (int)watchdog_id);
            return UFLAKE_OK;
        }
        current = current->next;
    }

    xSemaphoreGive(watchdog_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

void uflake_watchdog_check_timeouts(void)
{
    if (!watchdog_mutex)
        return;

    // Use timeout to prevent deadlock
    if (xSemaphoreTake(watchdog_mutex, pdMS_TO_TICKS(10)) != pdTRUE)
        return;

    uint32_t current_time = xTaskGetTickCount();

    //  Check all watchdogs in list
    watchdog_node_t *current = watchdog_list;
    while (current)
    {
        if (current->watchdog.is_active)
        {
            uint32_t elapsed = current_time - current->watchdog.last_feed;
            uint32_t timeout_ticks = pdMS_TO_TICKS(current->watchdog.timeout_ms);

            if (elapsed >= timeout_ticks)
            {
                ESP_LOGE(TAG, "Watchdog timeout: '%s' (ID: %d)",
                         current->watchdog.name, (int)current->watchdog.watchdog_id);
                uflake_panic_trigger(PANIC_REASON_WATCHDOG_TIMEOUT, current->watchdog.name);
            }
        }
        current = current->next;
    }

    xSemaphoreGive(watchdog_mutex);
}

uflake_result_t uflake_watchdog_delete(uint32_t watchdog_id)
{
    xSemaphoreTake(watchdog_mutex, portMAX_DELAY);

    // Remove from linked list
    watchdog_node_t *prev = NULL;
    watchdog_node_t *current = watchdog_list;

    while (current)
    {
        if (current->watchdog.watchdog_id == watchdog_id)
        {
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                watchdog_list = current->next;
            }

            uflake_free(current);
            xSemaphoreGive(watchdog_mutex);
            ESP_LOGI(TAG, "Deleted watchdog ID: %d", (int)watchdog_id);
            return UFLAKE_OK;
        }
        prev = current;
        current = current->next;
    }

    xSemaphoreGive(watchdog_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}
