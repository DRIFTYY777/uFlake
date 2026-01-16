#include "event_manager.h"
#include "memory_manager.h"
#include "esp_log.h"

static const char *TAG = "EVENT_MGR";

typedef struct subscription_node_t
{
    event_subscription_t subscription;
    struct subscription_node_t *next;
} subscription_node_t;

static subscription_node_t *subscription_list = NULL;
static uflake_event_t *pending_events = NULL;
static uint32_t next_subscription_id = 1;
static SemaphoreHandle_t event_mutex = NULL;
static QueueHandle_t event_queue = NULL;

#define MAX_PENDING_EVENTS 50

uflake_result_t uflake_event_init(void)
{
    event_mutex = xSemaphoreCreateMutex();
    if (!event_mutex)
    {
        ESP_LOGE(TAG, "Failed to create event mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    event_queue = xQueueCreate(MAX_PENDING_EVENTS, sizeof(uflake_event_t));
    if (!event_queue)
    {
        ESP_LOGE(TAG, "Failed to create event queue");
        return UFLAKE_ERROR_MEMORY;
    }

    ESP_LOGI(TAG, "Event manager initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_event_publish(const char *event_name, event_type_t type,
                                     const void *data, size_t data_size)
{
    if (!event_name)
        return UFLAKE_ERROR_INVALID_PARAM;

    uflake_event_t event = {0};
    strncpy(event.name, event_name, sizeof(event.name) - 1);
    event.name[sizeof(event.name) - 1] = '\0';
    event.type = type;
    event.timestamp = uflake_kernel_get_tick_count();

    if (data && data_size > 0)
    {
        event.data_size = (data_size > UFLAKE_MAX_EVENT_DATA) ? UFLAKE_MAX_EVENT_DATA : data_size;
        memcpy(event.data, data, event.data_size);
    }

    // Add to event queue for processing
    if (xQueueSend(event_queue, &event, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGW(TAG, "Failed to queue event: %s", event_name);
        return UFLAKE_ERROR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Published event: %s, type: %d", event_name, type);
    return UFLAKE_OK;
}

uflake_result_t uflake_event_subscribe(const char *event_name, event_callback_t callback,
                                       uint32_t *subscription_id)
{
    if (!event_name || !callback || !subscription_id)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(event_mutex, portMAX_DELAY);

    subscription_node_t *node = (subscription_node_t *)uflake_malloc(sizeof(subscription_node_t), UFLAKE_MEM_INTERNAL);
    if (!node)
    {
        xSemaphoreGive(event_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    node->subscription.subscription_id = next_subscription_id++;
    strncpy(node->subscription.event_name, event_name, sizeof(node->subscription.event_name) - 1);
    node->subscription.event_name[sizeof(node->subscription.event_name) - 1] = '\0';
    node->subscription.callback = callback;

    uflake_process_t *current_process = uflake_process_get_current();
    node->subscription.subscriber_pid = current_process ? current_process->pid : 0;

    node->next = subscription_list;
    subscription_list = node;

    *subscription_id = node->subscription.subscription_id;

    xSemaphoreGive(event_mutex);
    ESP_LOGI(TAG, "Subscribed to event '%s', subscription ID: %d", event_name, (int)*subscription_id);

    return UFLAKE_OK;
}

uflake_result_t uflake_event_unsubscribe(uint32_t subscription_id)
{
    xSemaphoreTake(event_mutex, portMAX_DELAY);

    subscription_node_t *prev = NULL;
    subscription_node_t *current = subscription_list;

    while (current)
    {
        if (current->subscription.subscription_id == subscription_id)
        {
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                subscription_list = current->next;
            }

            uflake_free(current);
            xSemaphoreGive(event_mutex);
            ESP_LOGI(TAG, "Unsubscribed from event, subscription ID: %d", (int)subscription_id);
            return UFLAKE_OK;
        }
        prev = current;
        current = current->next;
    }

    xSemaphoreGive(event_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

void uflake_event_process(void)
{
    uflake_event_t event;

    // Process all pending events
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE)
    {
        ESP_LOGD(TAG, "Processing event: %s", event.name);

        xSemaphoreTake(event_mutex, portMAX_DELAY);

        subscription_node_t *current = subscription_list;
        uint32_t callback_count = 0;

        while (current)
        {
            if (strcmp(current->subscription.event_name, event.name) == 0)
            {
                // Call subscriber callback
                if (current->subscription.callback)
                {
                    current->subscription.callback(&event);
                    callback_count++;
                }
            }
            current = current->next;
        }

        xSemaphoreGive(event_mutex);
        ESP_LOGD(TAG, "Event '%s' delivered to %d subscribers", event.name, (int)callback_count);
    }
}
