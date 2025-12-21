#include "resource_manager.h"
#include "../memory/memory_manager.h"
#include "esp_log.h"

static const char *TAG = "RESOURCE_MGR";

//  Add proper node structure
typedef struct resource_node_t
{
    uflake_resource_t resource;
    struct resource_node_t *next;
} resource_node_t;

static resource_node_t *resource_list = NULL;
static uint32_t next_resource_id = 1;
static SemaphoreHandle_t resource_mutex = NULL;

uflake_result_t uflake_resource_init(void)
{
    resource_mutex = xSemaphoreCreateMutex();
    if (!resource_mutex)
    {
        ESP_LOGE(TAG, "Failed to create resource mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    ESP_LOGI(TAG, "Resource manager initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_resource_register(const char *name, resource_type_t type,
                                         void *resource_ptr, bool is_shareable, uint32_t *resource_id)
{
    if (!name || !resource_ptr || !resource_id)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(resource_mutex, portMAX_DELAY);

    //  Allocate node properly
    resource_node_t *node = (resource_node_t *)uflake_malloc(sizeof(resource_node_t), UFLAKE_MEM_INTERNAL);
    if (!node)
    {
        xSemaphoreGive(resource_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    node->resource.resource_id = next_resource_id++;
    node->resource.type = type;
    node->resource.resource_ptr = resource_ptr;
    node->resource.is_shareable = is_shareable;
    node->resource.ref_count = 1;
    node->resource.created_time = uflake_kernel_get_tick_count();

    strncpy(node->resource.name, name, sizeof(node->resource.name) - 1);
    node->resource.name[sizeof(node->resource.name) - 1] = '\0';

    uflake_process_t *current_process = uflake_process_get_current();
    node->resource.owner_pid = current_process ? current_process->pid : 0;

    //  Insert into linked list
    node->next = resource_list;
    resource_list = node;

    *resource_id = node->resource.resource_id;

    xSemaphoreGive(resource_mutex);
    ESP_LOGI(TAG, "Registered resource '%s', ID: %d, type: %d", name, (int)*resource_id, type);

    return UFLAKE_OK;
}

uflake_result_t uflake_resource_acquire(uint32_t resource_id, uint32_t requesting_pid)
{
    xSemaphoreTake(resource_mutex, portMAX_DELAY);

    //  Search linked list
    resource_node_t *current = resource_list;
    while (current)
    {
        if (current->resource.resource_id == resource_id)
        {
            // Check if shareable or not already owned
            if (!current->resource.is_shareable && current->resource.ref_count > 0 &&
                current->resource.owner_pid != requesting_pid)
            {
                xSemaphoreGive(resource_mutex);
                ESP_LOGW(TAG, "Resource %d is not shareable and already owned", (int)resource_id);
                return UFLAKE_ERROR;
            }

            current->resource.ref_count++;
            xSemaphoreGive(resource_mutex);
            ESP_LOGD(TAG, "Resource %d acquired by PID: %d (ref_count: %d)",
                     (int)resource_id, (int)requesting_pid, (int)current->resource.ref_count);
            return UFLAKE_OK;
        }
        current = current->next;
    }

    xSemaphoreGive(resource_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_resource_release(uint32_t resource_id, uint32_t releasing_pid)
{
    xSemaphoreTake(resource_mutex, portMAX_DELAY);

    //  Search and update
    resource_node_t *current = resource_list;
    while (current)
    {
        if (current->resource.resource_id == resource_id)
        {
            if (current->resource.ref_count > 0)
            {
                current->resource.ref_count--;
            }

            xSemaphoreGive(resource_mutex);
            ESP_LOGD(TAG, "Resource %d released by PID: %d (ref_count: %d)",
                     (int)resource_id, (int)releasing_pid, (int)current->resource.ref_count);
            return UFLAKE_OK;
        }
        current = current->next;
    }

    xSemaphoreGive(resource_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_resource_find_by_name(const char *name, uint32_t *resource_id)
{
    if (!name || !resource_id)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(resource_mutex, portMAX_DELAY);

    //  Search by name
    resource_node_t *current = resource_list;
    while (current)
    {
        if (strcmp(current->resource.name, name) == 0)
        {
            *resource_id = current->resource.resource_id;
            xSemaphoreGive(resource_mutex);
            return UFLAKE_OK;
        }
        current = current->next;
    }

    xSemaphoreGive(resource_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_resource_cleanup_for_process(uint32_t pid)
{
    xSemaphoreTake(resource_mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Cleaning up resources for PID: %d", (int)pid);

    //  Iterate and cleanup owned resources
    resource_node_t *prev = NULL;
    resource_node_t *current = resource_list;
    uint32_t cleaned_count = 0;

    while (current)
    {
        resource_node_t *next = current->next;

        if (current->resource.owner_pid == pid)
        {
            // Remove from list
            if (prev)
            {
                prev->next = next;
            }
            else
            {
                resource_list = next;
            }

            ESP_LOGD(TAG, "Cleaning up resource '%s' (ID: %d)",
                     current->resource.name, (int)current->resource.resource_id);
            uflake_free(current);
            cleaned_count++;
            current = next;
            continue;
        }

        prev = current;
        current = next;
    }

    xSemaphoreGive(resource_mutex);
    ESP_LOGI(TAG, "Cleaned up %d resources for PID: %d", (int)cleaned_count, (int)pid);
    return UFLAKE_OK;
}
