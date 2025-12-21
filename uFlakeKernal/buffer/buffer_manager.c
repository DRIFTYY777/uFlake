#include "buffer_manager.h"
#include "../memory/memory_manager.h"
#include "esp_log.h"

static const char *TAG = "BUFFER_MGR";
static SemaphoreHandle_t buffer_mutex = NULL;

uflake_result_t uflake_buffer_init(void)
{
    buffer_mutex = xSemaphoreCreateMutex();
    if (!buffer_mutex)
    {
        ESP_LOGE(TAG, "Failed to create buffer mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    ESP_LOGI(TAG, "Buffer manager initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_buffer_create(uflake_buffer_t **buffer, size_t capacity)
{
    if (!buffer || capacity == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    uflake_buffer_t *new_buffer = (uflake_buffer_t *)uflake_malloc(sizeof(uflake_buffer_t), UFLAKE_MEM_INTERNAL);
    if (!new_buffer)
    {
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    new_buffer->data = uflake_malloc(capacity, UFLAKE_MEM_INTERNAL);
    if (!new_buffer->data)
    {
        uflake_free(new_buffer);
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    new_buffer->size = 0;
    new_buffer->capacity = capacity;
    new_buffer->ref_count = 1;
    new_buffer->is_allocated = true;

    *buffer = new_buffer;

    xSemaphoreGive(buffer_mutex);
    ESP_LOGD(TAG, "Created buffer with capacity: %d bytes", (int)capacity);

    return UFLAKE_OK;
}

uflake_result_t uflake_buffer_write(uflake_buffer_t *buffer, const void *data, size_t size)
{
    if (!buffer || !data || size == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    if (!buffer->is_allocated)
    {
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR;
    }

    if (buffer->size + size > buffer->capacity)
    {
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    memcpy((uint8_t *)buffer->data + buffer->size, data, size);
    buffer->size += size;

    xSemaphoreGive(buffer_mutex);
    return UFLAKE_OK;
}

uflake_result_t uflake_buffer_read(uflake_buffer_t *buffer, void *data, size_t size)
{
    if (!buffer || !data || size == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    if (!buffer->is_allocated)
    {
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR;
    }

    size_t read_size = (size > buffer->size) ? buffer->size : size;
    memcpy(data, buffer->data, read_size);

    xSemaphoreGive(buffer_mutex);
    return UFLAKE_OK;
}

uflake_result_t uflake_buffer_resize(uflake_buffer_t *buffer, size_t new_capacity)
{
    if (!buffer || new_capacity == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    if (!buffer->is_allocated)
    {
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR;
    }

    void *new_data = uflake_realloc(buffer->data, new_capacity);
    if (!new_data)
    {
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;

    if (buffer->size > new_capacity)
    {
        buffer->size = new_capacity;
    }

    xSemaphoreGive(buffer_mutex);
    return UFLAKE_OK;
}

uflake_result_t uflake_buffer_destroy(uflake_buffer_t *buffer)
{
    if (!buffer)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    buffer->ref_count--;

    if (buffer->ref_count == 0)
    {
        if (buffer->data)
        {
            uflake_free(buffer->data);
        }
        buffer->is_allocated = false;
        uflake_free(buffer);
    }

    xSemaphoreGive(buffer_mutex);
    return UFLAKE_OK;
}

// Add circular buffer functionality
uflake_result_t uflake_buffer_create_circular(uflake_buffer_t **buffer, size_t capacity)
{
    if (!buffer || capacity == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    uflake_result_t result = uflake_buffer_create(buffer, capacity);
    if (result != UFLAKE_OK)
        return result;

    // Mark as circular buffer (could use a flag in buffer struct)
    ESP_LOGD(TAG, "Created circular buffer with capacity: %d bytes", (int)capacity);
    return UFLAKE_OK;
}

uflake_result_t uflake_buffer_write_circular(uflake_buffer_t *buffer, const void *data, size_t size)
{
    if (!buffer || !data || size == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    if (!buffer->is_allocated)
    {
        xSemaphoreGive(buffer_mutex);
        return UFLAKE_ERROR;
    }

    // Circular buffer logic - overwrite old data if necessary
    for (size_t i = 0; i < size; i++)
    {
        size_t write_pos = buffer->size % buffer->capacity;
        ((uint8_t *)buffer->data)[write_pos] = ((uint8_t *)data)[i];
        if (buffer->size < buffer->capacity)
        {
            buffer->size++;
        }
    }

    xSemaphoreGive(buffer_mutex);
    return UFLAKE_OK;
}

uflake_result_t uflake_buffer_get_stats(uflake_buffer_t *buffer, size_t *used, size_t *free)
{
    if (!buffer || !used || !free)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(buffer_mutex, portMAX_DELAY);

    *used = buffer->size;
    *free = buffer->capacity - buffer->size;

    xSemaphoreGive(buffer_mutex);
    return UFLAKE_OK;
}
