#include "synchronization.h"
#include "../memory/memory_manager.h"
#include "esp_log.h"

static const char *TAG = "SYNC";

uflake_result_t uflake_sync_init(void)
{
    ESP_LOGI(TAG, "Synchronization subsystem initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_mutex_create(uflake_mutex_t **mutex)
{
    if (!mutex)
        return UFLAKE_ERROR_INVALID_PARAM;

    uflake_mutex_t *new_mutex = (uflake_mutex_t *)uflake_malloc(sizeof(uflake_mutex_t), UFLAKE_MEM_INTERNAL);
    if (!new_mutex)
    {
        return UFLAKE_ERROR_MEMORY;
    }

    new_mutex->handle = xSemaphoreCreateMutex();
    if (!new_mutex->handle)
    {
        uflake_free(new_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    new_mutex->owner_pid = 0;
    new_mutex->lock_count = 0;

    *mutex = new_mutex;
    return UFLAKE_OK;
}

uflake_result_t uflake_mutex_lock(uflake_mutex_t *mutex, uint32_t timeout_ms)
{
    if (!mutex || !mutex->handle)
        return UFLAKE_ERROR_INVALID_PARAM;

    //  Cannot lock mutex from ISR
    if (uflake_kernel_is_in_isr())
    {
        ESP_LOGE(TAG, "FATAL: Attempted to lock mutex from ISR!");
        return UFLAKE_ERROR;
    }

    TickType_t timeout_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    //  Store lock attempt time for deadlock detection
    uint32_t lock_start = xTaskGetTickCount();

    if (xSemaphoreTake(mutex->handle, timeout_ticks) == pdTRUE)
    {
        mutex->lock_count++;
        mutex->owner_pid = uflake_process_get_current() ? uflake_process_get_current()->pid : 0;

        //  Warn if lock took too long
        uint32_t lock_duration = xTaskGetTickCount() - lock_start;
        if (lock_duration > pdMS_TO_TICKS(100))
        {
            ESP_LOGW(TAG, "Mutex lock took %d ms - possible contention", (int)(lock_duration * portTICK_PERIOD_MS));
        }

        return UFLAKE_OK;
    }

    ESP_LOGW(TAG, "Mutex lock timeout after %d ms", (int)timeout_ms);
    return UFLAKE_ERROR_TIMEOUT;
}

uflake_result_t uflake_mutex_unlock(uflake_mutex_t *mutex)
{
    if (!mutex || !mutex->handle)
        return UFLAKE_ERROR_INVALID_PARAM;

    if (mutex->lock_count > 0)
    {
        mutex->lock_count--;
    }

    if (xSemaphoreGive(mutex->handle) == pdTRUE)
    {
        return UFLAKE_OK;
    }

    return UFLAKE_ERROR;
}

uflake_result_t uflake_mutex_destroy(uflake_mutex_t *mutex)
{
    if (!mutex)
        return UFLAKE_ERROR_INVALID_PARAM;

    if (mutex->handle)
    {
        vSemaphoreDelete(mutex->handle);
    }

    uflake_free(mutex);
    return UFLAKE_OK;
}

uflake_result_t uflake_semaphore_create(uflake_semaphore_t **semaphore, uint32_t initial_count, uint32_t max_count)
{
    if (!semaphore || max_count == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    uflake_semaphore_t *new_sem = (uflake_semaphore_t *)uflake_malloc(sizeof(uflake_semaphore_t), UFLAKE_MEM_INTERNAL);
    if (!new_sem)
    {
        return UFLAKE_ERROR_MEMORY;
    }

    new_sem->handle = xSemaphoreCreateCounting(max_count, initial_count);
    if (!new_sem->handle)
    {
        uflake_free(new_sem);
        return UFLAKE_ERROR_MEMORY;
    }

    new_sem->max_count = max_count;
    new_sem->current_count = initial_count;

    *semaphore = new_sem;
    return UFLAKE_OK;
}

uflake_result_t uflake_semaphore_take(uflake_semaphore_t *semaphore, uint32_t timeout_ms)
{
    if (!semaphore || !semaphore->handle)
        return UFLAKE_ERROR_INVALID_PARAM;

    TickType_t timeout_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    if (xSemaphoreTake(semaphore->handle, timeout_ticks) == pdTRUE)
    {
        if (semaphore->current_count > 0)
        {
            semaphore->current_count--;
        }
        return UFLAKE_OK;
    }

    return UFLAKE_ERROR_TIMEOUT;
}

uflake_result_t uflake_semaphore_give(uflake_semaphore_t *semaphore)
{
    if (!semaphore || !semaphore->handle)
        return UFLAKE_ERROR_INVALID_PARAM;

    //  ISR-SAFE: Auto-detect context
    if (uflake_kernel_is_in_isr())
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        if (xSemaphoreGiveFromISR(semaphore->handle, &xHigherPriorityTaskWoken) == pdTRUE)
        {
            if (semaphore->current_count < semaphore->max_count)
            {
                semaphore->current_count++;
            }
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            return UFLAKE_OK;
        }
        return UFLAKE_ERROR;
    }

    // Normal context
    if (xSemaphoreGive(semaphore->handle) == pdTRUE)
    {
        if (semaphore->current_count < semaphore->max_count)
        {
            semaphore->current_count++;
        }
        return UFLAKE_OK;
    }

    return UFLAKE_ERROR;
}

uflake_result_t uflake_semaphore_destroy(uflake_semaphore_t *semaphore)
{
    if (!semaphore)
        return UFLAKE_ERROR_INVALID_PARAM;

    if (semaphore->handle)
    {
        vSemaphoreDelete(semaphore->handle);
    }

    uflake_free(semaphore);
    return UFLAKE_OK;
}
