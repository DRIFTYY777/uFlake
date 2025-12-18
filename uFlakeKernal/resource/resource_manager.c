#include "resource_manager.h"
#include "../memory/memory_manager.h"
#include "esp_log.h"

static const char* TAG = "RESOURCE_MGR";
static uflake_resource_t* resource_list = NULL;
static uint32_t next_resource_id = 1;
static SemaphoreHandle_t resource_mutex = NULL;

uflake_result_t uflake_resource_init(void) {
    resource_mutex = xSemaphoreCreateMutex();
    if (!resource_mutex) {
        ESP_LOGE(TAG, "Failed to create resource mutex");
        return UFLAKE_ERROR_MEMORY;
    }
    
    ESP_LOGI(TAG, "Resource manager initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_resource_register(const char* name, resource_type_t type, 
                                        void* resource_ptr, bool is_shareable, uint32_t* resource_id) {
    if (!name || !resource_ptr || !resource_id) return UFLAKE_ERROR_INVALID_PARAM;
    
    xSemaphoreTake(resource_mutex, portMAX_DELAY);
    
    uflake_resource_t* resource = (uflake_resource_t*)uflake_malloc(sizeof(uflake_resource_t), UFLAKE_MEM_INTERNAL);
    if (!resource) {
        xSemaphoreGive(resource_mutex);
        return UFLAKE_ERROR_MEMORY;
    }
    
    resource->resource_id = next_resource_id++;
    resource->type = type;
    resource->resource_ptr = resource_ptr;
    resource->is_shareable = is_shareable;
    resource->ref_count = 1;
    resource->created_time = uflake_kernel_get_tick_count();
    
    strncpy(resource->name, name, sizeof(resource->name) - 1);
    resource->name[sizeof(resource->name) - 1] = '\0';
    
    uflake_process_t* current_process = uflake_process_get_current();
    resource->owner_pid = current_process ? current_process->pid : 0;
    
    // Add to resource list (simplified implementation)
    // In real code, use proper linked list or hash table
    
    *resource_id = resource->resource_id;
    
    xSemaphoreGive(resource_mutex);
    ESP_LOGI(TAG, "Registered resource '%s', ID: %d, type: %d", name, (int)*resource_id, type);
    
    return UFLAKE_OK;
}

uflake_result_t uflake_resource_acquire(uint32_t resource_id, uint32_t requesting_pid) {
    xSemaphoreTake(resource_mutex, portMAX_DELAY);
    
    // Find resource by ID
    // In real implementation, search through resource list
    // Check if resource is shareable or already owned
    // Increment reference count if acquisition is allowed
    
    xSemaphoreGive(resource_mutex);
    ESP_LOGD(TAG, "Resource %d acquired by PID: %d", (int)resource_id, (int)requesting_pid);
    return UFLAKE_OK;
}

uflake_result_t uflake_resource_release(uint32_t resource_id, uint32_t releasing_pid) {
    xSemaphoreTake(resource_mutex, portMAX_DELAY);
    
    // Find resource by ID
    // Check if process owns the resource
    // Decrement reference count
    // If ref_count reaches 0 and resource is not persistent, free it
    
    xSemaphoreGive(resource_mutex);
    ESP_LOGD(TAG, "Resource %d released by PID: %d", (int)resource_id, (int)releasing_pid);
    return UFLAKE_OK;
}

uflake_result_t uflake_resource_find_by_name(const char* name, uint32_t* resource_id) {
    if (!name || !resource_id) return UFLAKE_ERROR_INVALID_PARAM;
    
    xSemaphoreTake(resource_mutex, portMAX_DELAY);
    
    // Search through resource list for matching name
    // In real implementation, iterate through proper linked list
    
    xSemaphoreGive(resource_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_resource_cleanup_for_process(uint32_t pid) {
    xSemaphoreTake(resource_mutex, portMAX_DELAY);
    
    ESP_LOGI(TAG, "Cleaning up resources for PID: %d", (int)pid);
    
    // Find all resources owned by this process
    // Release or free them as appropriate
    // In real implementation, iterate through resource list
    
    xSemaphoreGive(resource_mutex);
    return UFLAKE_OK;
}
