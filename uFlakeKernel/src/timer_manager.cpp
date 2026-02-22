#include "timer_manager.h"
#include "memory_manager.h"
#include "esp_log.h"

static const char *TAG = "TIMER_MGR";
static uint32_t next_timer_id = 1;
static SemaphoreHandle_t timer_mutex = NULL;

// Add proper timer structure with next pointer
typedef struct timer_node
{
    uflake_timer_t timer;
    struct timer_node *next;
} timer_node_t;

static timer_node_t *timer_list_head = NULL;

uflake_result_t uflake_timer_init(void)
{
    timer_mutex = xSemaphoreCreateMutex();
    if (!timer_mutex)
    {
        ESP_LOGE(TAG, "Failed to create timer mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    ESP_LOGI(TAG, "Timer manager initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_timer_create(uint32_t *timer_id, uint32_t interval_ms,
                                    timer_callback_t callback, void *args, bool periodic)
{
    if (!timer_id || !callback || interval_ms == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(timer_mutex, portMAX_DELAY);

    timer_node_t *node = (timer_node_t *)uflake_malloc(sizeof(timer_node_t), UFLAKE_MEM_INTERNAL);
    if (!node)
    {
        xSemaphoreGive(timer_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    node->timer.timer_id = next_timer_id++;
    node->timer.interval_ms = interval_ms;
    node->timer.next_trigger = xTaskGetTickCount() + pdMS_TO_TICKS(interval_ms);
    node->timer.callback = callback;
    node->timer.args = args;
    node->timer.is_periodic = periodic;
    node->timer.is_active = false;
    node->next = timer_list_head;
    timer_list_head = node;

    *timer_id = node->timer.timer_id;

    xSemaphoreGive(timer_mutex);
    ESP_LOGD(TAG, "Created timer ID: %d, interval: %d ms", (int)*timer_id, (int)interval_ms);

    return UFLAKE_OK;
}

uflake_result_t uflake_timer_start(uint32_t timer_id)
{
    xSemaphoreTake(timer_mutex, portMAX_DELAY);

    timer_node_t *node = timer_list_head;
    while (node)
    {
        if (node->timer.timer_id == timer_id)
        {
            node->timer.is_active = true;
            node->timer.next_trigger = xTaskGetTickCount() + pdMS_TO_TICKS(node->timer.interval_ms);
            xSemaphoreGive(timer_mutex);
            ESP_LOGD(TAG, "Started timer ID: %d", (int)timer_id);
            return UFLAKE_OK;
        }
        node = node->next;
    }

    xSemaphoreGive(timer_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_timer_stop(uint32_t timer_id)
{
    xSemaphoreTake(timer_mutex, portMAX_DELAY);

    timer_node_t *node = timer_list_head;
    while (node)
    {
        if (node->timer.timer_id == timer_id)
        {
            node->timer.is_active = false;
            xSemaphoreGive(timer_mutex);
            ESP_LOGD(TAG, "Stopped timer ID: %d", (int)timer_id);
            return UFLAKE_OK;
        }
        node = node->next;
    }

    xSemaphoreGive(timer_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_timer_delete(uint32_t timer_id)
{
    xSemaphoreTake(timer_mutex, portMAX_DELAY);

    timer_node_t *prev = NULL;
    timer_node_t *current = timer_list_head;

    while (current)
    {
        if (current->timer.timer_id == timer_id)
        {
            // Remove from linked list
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                timer_list_head = current->next;
            }

            uflake_free(current);
            xSemaphoreGive(timer_mutex);
            ESP_LOGD(TAG, "Deleted timer ID: %d", (int)timer_id);
            return UFLAKE_OK;
        }
        prev = current;
        current = current->next;
    }

    xSemaphoreGive(timer_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

void uflake_timer_process(void)
{
    if (!timer_mutex)
        return;

    // Use timeout to prevent deadlock
    if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(10)) != pdTRUE)
        return;

    uint32_t current_time = xTaskGetTickCount();
    timer_node_t *node = timer_list_head;

    while (node)
    {
        if (node->timer.is_active && current_time >= node->timer.next_trigger)
        {
            // Call the timer callback
            if (node->timer.callback)
            {
                node->timer.callback(node->timer.args);
            }

            if (node->timer.is_periodic)
            {
                node->timer.next_trigger = current_time + pdMS_TO_TICKS(node->timer.interval_ms);
            }
            else
            {
                node->timer.is_active = false;
            }
        }
        node = node->next;
    }

    xSemaphoreGive(timer_mutex);
}
