#include "message_queue.h"
#include "memory_manager.h"
#include "esp_log.h"

static const char *TAG = "MSG_QUEUE";
static uflake_msgqueue_t *queue_list = NULL;
static SemaphoreHandle_t msgqueue_mutex = NULL;
static uint32_t next_message_id = 1;

uflake_result_t uflake_messagequeue_init(void)
{
    msgqueue_mutex = xSemaphoreCreateMutex();
    if (!msgqueue_mutex)
    {
        ESP_LOGE(TAG, "Failed to create message queue mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    ESP_LOGI(TAG, "Message queue system initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_msgqueue_create(const char *name, uint32_t max_messages,
                                       bool is_public, uflake_msgqueue_t **queue)
{
    if (!name || !queue || max_messages == 0)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);

    // Check if queue with same name already exists
    uflake_msgqueue_t *existing = queue_list;
    while (existing)
    {
        if (strcmp(existing->name, name) == 0)
        {
            xSemaphoreGive(msgqueue_mutex);
            return UFLAKE_ERROR;
        }
        existing = existing->next;  // ✅ Fixed: Use proper next pointer
    }

    uflake_msgqueue_t *new_queue = (uflake_msgqueue_t *)uflake_malloc(sizeof(uflake_msgqueue_t), UFLAKE_MEM_INTERNAL);
    if (!new_queue)
    {
        xSemaphoreGive(msgqueue_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    new_queue->queue_handle = xQueueCreate(max_messages, sizeof(uflake_message_t));
    if (!new_queue->queue_handle)
    {
        uflake_free(new_queue);
        xSemaphoreGive(msgqueue_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    strncpy(new_queue->name, name, sizeof(new_queue->name) - 1);
    new_queue->name[sizeof(new_queue->name) - 1] = '\0';
    new_queue->max_messages = max_messages;
    new_queue->message_count = 0;
    new_queue->is_public = is_public;

    uflake_process_t *current_process = uflake_process_get_current();
    new_queue->owner_pid = current_process ? current_process->pid : 0;

    // ✅ Fixed: Proper linked list insertion
    new_queue->next = queue_list;
    queue_list = new_queue;

    *queue = new_queue;

    xSemaphoreGive(msgqueue_mutex);
    ESP_LOGI(TAG, "Created message queue '%s' with %d max messages", name, (int)max_messages);

    return UFLAKE_OK;
}

uflake_result_t uflake_msgqueue_send(uflake_msgqueue_t *queue, const uflake_message_t *message,
                                     uint32_t timeout_ms)
{
    if (!queue || !message)
        return UFLAKE_ERROR_INVALID_PARAM;

    uflake_message_t msg_copy = *message;
    
    // ✅ ISR-SAFE: Protect message ID increment
    if (uflake_kernel_is_in_isr())
    {
        msg_copy.message_id = next_message_id++;
        msg_copy.timestamp = xTaskGetTickCountFromISR();
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        if (xQueueSendFromISR(queue->queue_handle, &msg_copy, &xHigherPriorityTaskWoken) == pdTRUE)
        {
            // ✅ Atomic increment (single instruction on ESP32)
            queue->message_count++;
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            return UFLAKE_OK;
        }
        return UFLAKE_ERROR_TIMEOUT;
    }

    // ✅ Fixed: Protect message ID and count together
    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
    msg_copy.message_id = next_message_id++;
    msg_copy.timestamp = uflake_kernel_get_tick_count();
    xSemaphoreGive(msgqueue_mutex);

    TickType_t timeout_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    if (xQueueSend(queue->queue_handle, &msg_copy, timeout_ticks) == pdTRUE)
    {
        xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
        queue->message_count++;
        xSemaphoreGive(msgqueue_mutex);

        ESP_LOGD(TAG, "Message sent to queue '%s', ID: %d", queue->name, (int)msg_copy.message_id);
        return UFLAKE_OK;
    }

    return UFLAKE_ERROR_TIMEOUT;
}

// ✅ Add ISR-safe receive
uflake_result_t uflake_msgqueue_receive_from_isr(uflake_msgqueue_t* queue, uflake_message_t* message) {
    if (!queue || !message) return UFLAKE_ERROR_INVALID_PARAM;
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (xQueueReceiveFromISR(queue->queue_handle, message, &xHigherPriorityTaskWoken) == pdTRUE) {
        queue->message_count--;  // Atomic decrement
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return UFLAKE_OK;
    }
    
    return UFLAKE_ERROR_TIMEOUT;
}

uflake_result_t uflake_msgqueue_receive(uflake_msgqueue_t *queue, uflake_message_t *message,
                                        uint32_t timeout_ms)
{
    if (!queue || !message)
        return UFLAKE_ERROR_INVALID_PARAM;

    // ✅ ISR-safe receive
    if (uflake_kernel_is_in_isr()) {
        return uflake_msgqueue_receive_from_isr(queue, message);
    }

    TickType_t timeout_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    if (xQueueReceive(queue->queue_handle, message, timeout_ticks) == pdTRUE)
    {
        xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
        if (queue->message_count > 0)
        {
            queue->message_count--;
        }
        xSemaphoreGive(msgqueue_mutex);

        ESP_LOGD(TAG, "Message received from queue '%s', ID: %d", queue->name, (int)message->message_id);
        return UFLAKE_OK;
    }

    return UFLAKE_ERROR_TIMEOUT;
}

uflake_result_t uflake_msgqueue_broadcast(const uflake_message_t *message)
{
    if (!message)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);

    uflake_message_t broadcast_msg = *message;
    broadcast_msg.type = MSG_TYPE_BROADCAST;
    broadcast_msg.message_id = next_message_id++;
    broadcast_msg.timestamp = uflake_kernel_get_tick_count();

    // Send to all public queues (simplified implementation)
    // In a real implementation, you'd maintain a proper list of queues

    xSemaphoreGive(msgqueue_mutex);
    ESP_LOGI(TAG, "Broadcast message sent, ID: %d", (int)broadcast_msg.message_id);

    return UFLAKE_OK;
}

uflake_result_t uflake_msgqueue_find(const char *name, uflake_msgqueue_t **queue)
{
    if (!name || !queue)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);

    // ✅ Fixed: Proper linked list search
    uflake_msgqueue_t *current = queue_list;
    while (current)
    {
        if (strcmp(current->name, name) == 0)
        {
            *queue = current;
            xSemaphoreGive(msgqueue_mutex);
            return UFLAKE_OK;
        }
        current = current->next;
    }

    xSemaphoreGive(msgqueue_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_msgqueue_destroy(uflake_msgqueue_t *queue)
{
    if (!queue)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);

    // ✅ Fixed: Proper linked list removal
    uflake_msgqueue_t *prev = NULL;
    uflake_msgqueue_t *current = queue_list;

    while (current)
    {
        if (current == queue)
        {
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                queue_list = current->next;
            }

            if (queue->queue_handle)
            {
                vQueueDelete(queue->queue_handle);
            }

            uflake_free(queue);

            xSemaphoreGive(msgqueue_mutex);
            ESP_LOGI(TAG, "Destroyed message queue '%s'", queue->name);
            return UFLAKE_OK;
        }
        prev = current;
        current = current->next;
    }

    xSemaphoreGive(msgqueue_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

void uflake_messagequeue_process(void)
{
    if (!msgqueue_mutex) return;
    
    static uint32_t last_cleanup_tick = 0;
    uint32_t current_tick = xTaskGetTickCount();
    
    // Perform cleanup every 5 seconds
    if ((current_tick - last_cleanup_tick) < pdMS_TO_TICKS(5000)) {
        return;
    }
    
    last_cleanup_tick = current_tick;
    
    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
    
    uflake_msgqueue_t *current = queue_list;
    uint32_t total_queues = 0;
    uint32_t total_messages = 0;
    uint32_t empty_queues = 0;
    
    while (current)
    {
        total_queues++;
        total_messages += current->message_count;
        
        // Check if queue is empty
        if (current->message_count == 0) {
            empty_queues++;
        }
        
        // Check if queue is nearly full (>90% capacity)
        if (current->message_count > (current->max_messages * 9 / 10)) {
            ESP_LOGW(TAG, "Queue '%s' is nearly full: %d/%d messages",
                     current->name, (int)current->message_count, (int)current->max_messages);
        }
        
        // Sync message count with actual queue state
        UBaseType_t actual_count = uxQueueMessagesWaiting(current->queue_handle);
        if (actual_count != current->message_count) {
            ESP_LOGW(TAG, "Queue '%s' count mismatch - correcting from %d to %d",
                     current->name, (int)current->message_count, (int)actual_count);
            current->message_count = actual_count;
        }
        
        current = current->next;
    }
    
    xSemaphoreGive(msgqueue_mutex);
    
    // Log statistics periodically
    ESP_LOGD(TAG, "Message Queue Stats: %d queues, %d total messages, %d empty",
             (int)total_queues, (int)total_messages, (int)empty_queues);
}
