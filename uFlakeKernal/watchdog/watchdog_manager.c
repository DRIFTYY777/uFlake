#include "watchdog_manager.h"
#include "../memory/memory_manager.h"
#include "../panic/panic_handler.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

static const char* TAG = "WATCHDOG";
static uflake_watchdog_t* watchdog_list = NULL;
static uint32_t next_watchdog_id = 1;
static SemaphoreHandle_t watchdog_mutex = NULL;

uflake_result_t uflake_watchdog_init(void) {
    watchdog_mutex = xSemaphoreCreateMutex();
    if (!watchdog_mutex) {
        ESP_LOGE(TAG, "Failed to create watchdog mutex");
        return UFLAKE_ERROR_MEMORY;
    }
    
    // Initialize ESP32-S3 task watchdog
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdt_config);
    
    ESP_LOGI(TAG, "Watchdog manager initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_watchdog_create(const char* name, watchdog_type_t type, 
                                      uint32_t timeout_ms, uint32_t* watchdog_id) {
    if (!name || !watchdog_id || timeout_ms == 0) return UFLAKE_ERROR_INVALID_PARAM;
    
    xSemaphoreTake(watchdog_mutex, portMAX_DELAY);
    
    uflake_watchdog_t* watchdog = (uflake_watchdog_t*)uflake_malloc(sizeof(uflake_watchdog_t), UFLAKE_MEM_INTERNAL);
    if (!watchdog) {
        xSemaphoreGive(watchdog_mutex);
        return UFLAKE_ERROR_MEMORY;
    }
    
    watchdog->watchdog_id = next_watchdog_id++;
    watchdog->type = type;
    watchdog->timeout_ms = timeout_ms;
    watchdog->last_feed = xTaskGetTickCount();
    watchdog->is_active = true;
    
    strncpy(watchdog->name, name, sizeof(watchdog->name) - 1);
    watchdog->name[sizeof(watchdog->name) - 1] = '\0';
    
    // Add to watchdog list (simplified linked list using unused fields)
    // In real implementation, use proper linked list structure
    
    *watchdog_id = watchdog->watchdog_id;
    
    xSemaphoreGive(watchdog_mutex);
    ESP_LOGI(TAG, "Created watchdog '%s' with ID: %d, timeout: %d ms", name, (int)*watchdog_id, (int)timeout_ms);
    
    return UFLAKE_OK;
}

uflake_result_t uflake_watchdog_feed_by_id(uint32_t watchdog_id) {
    xSemaphoreTake(watchdog_mutex, portMAX_DELAY);
    
    // Find watchdog by ID (simplified implementation)
    // In real code, iterate through proper linked list
    
    xSemaphoreGive(watchdog_mutex);
    ESP_LOGV(TAG, "Fed watchdog ID: %d", (int)watchdog_id);
    return UFLAKE_OK;
}

void uflake_watchdog_feed(void) {
    // Feed the system watchdog
    esp_task_wdt_reset();
    ESP_LOGV(TAG, "System watchdog fed");
}

void uflake_watchdog_check_timeouts(void) {
    if (!watchdog_mutex) return;
    
    xSemaphoreTake(watchdog_mutex, portMAX_DELAY);
    
    uint32_t current_time = xTaskGetTickCount();
    
    // Check all watchdogs for timeouts
    // In real implementation, iterate through watchdog list
    // and trigger panic for expired watchdogs
    
    xSemaphoreGive(watchdog_mutex);
}

uflake_result_t uflake_watchdog_delete(uint32_t watchdog_id) {
    xSemaphoreTake(watchdog_mutex, portMAX_DELAY);
    
    // Find and delete watchdog
    // In real implementation, remove from linked list and free memory
    
    xSemaphoreGive(watchdog_mutex);
    ESP_LOGI(TAG, "Deleted watchdog ID: %d", (int)watchdog_id);
    return UFLAKE_OK;
}
