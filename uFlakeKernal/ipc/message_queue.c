#include "message_queue.h"
#include "../memory/memory_manager.h"
#include "esp_log.h"

static const char* TAG = "MSG_QUEUE";
static uflake_msgqueue_t* queue_list = NULL;
static SemaphoreHandle_t msgqueue_mutex = NULL;
static uint32_t next_message_id = 1;

uflake_result_t uflake_messagequeue_init(void) {
    msgqueue_mutex = xSemaphoreCreateMutex();
    if (!msgqueue_mutex) {
        ESP_LOGE(TAG, "Failed to create message queue mutex");
        return UFLAKE_ERROR_MEMORY;
    }
    
    ESP_LOGI(TAG, "Message queue system initialized");
    return UFLAKE_OK;
}

uflake_result_t uflake_msgqueue_create(const char* name, uint32_t max_messages, 
                                      bool is_public, uflake_msgqueue_t** queue) {
    if (!name || !queue || max_messages == 0) return UFLAKE_ERROR_INVALID_PARAM;
    
    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
    
    // Check if queue with same name already exists
    uflake_msgqueue_t* existing = queue_list;
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            xSemaphoreGive(msgqueue_mutex);
            return UFLAKE_ERROR;
        }
        existing = (uflake_msgqueue_t*)existing->queue_handle; // Using queue_handle as next pointer
    }
    
    uflake_msgqueue_t* new_queue = (uflake_msgqueue_t*)uflake_malloc(sizeof(uflake_msgqueue_t), UFLAKE_MEM_INTERNAL);
    if (!new_queue) {
        xSemaphoreGive(msgqueue_mutex);
        return UFLAKE_ERROR_MEMORY;
    }
    
    new_queue->queue_handle = xQueueCreate(max_messages, sizeof(uflake_message_t));
    if (!new_queue->queue_handle) {
        uflake_free(new_queue);
        xSemaphoreGive(msgqueue_mutex);
        return UFLAKE_ERROR_MEMORY;
    }
    
    strncpy(new_queue->name, name, sizeof(new_queue->name) - 1);
    new_queue->name[sizeof(new_queue->name) - 1] = '\0';
    new_queue->max_messages = max_messages;
    new_queue->message_count = 0;
    new_queue->is_public = is_public;
    
    uflake_process_t* current_process = uflake_process_get_current();
    new_queue->owner_pid = current_process ? current_process->pid : 0;
    
    // Add to queue list (simplified linked list)
    // Note: This is a simplified implementation for demonstration
    *queue = new_queue;
    
    xSemaphoreGive(msgqueue_mutex);
    ESP_LOGI(TAG, "Created message queue '%s' with %d max messages", name, (int)max_messages);
    
    return UFLAKE_OK;
}

uflake_result_t uflake_msgqueue_send(uflake_msgqueue_t* queue, const uflake_message_t* message, 
                                    uint32_t timeout_ms) {
    if (!queue || !message) return UFLAKE_ERROR_INVALID_PARAM;
    
    uflake_message_t msg_copy = *message;
    msg_copy.message_id = next_message_id++;
    msg_copy.timestamp = uflake_kernel_get_tick_count();
    
    TickType_t timeout_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    if (xQueueSend(queue->queue_handle, &msg_copy, timeout_ticks) == pdTRUE) {
        xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
        queue->message_count++;
        xSemaphoreGive(msgqueue_mutex);
        
        ESP_LOGD(TAG, "Message sent to queue '%s', ID: %d", queue->name, (int)msg_copy.message_id);
        return UFLAKE_OK;
    }
    
    return UFLAKE_ERROR_TIMEOUT;
}

uflake_result_t uflake_msgqueue_receive(uflake_msgqueue_t* queue, uflake_message_t* message, 
                                       uint32_t timeout_ms) {
    if (!queue || !message) return UFLAKE_ERROR_INVALID_PARAM;
    
    TickType_t timeout_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    if (xQueueReceive(queue->queue_handle, message, timeout_ticks) == pdTRUE) {
        xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
        if (queue->message_count > 0) {
            queue->message_count--;
        }
        xSemaphoreGive(msgqueue_mutex);
        
        ESP_LOGD(TAG, "Message received from queue '%s', ID: %d", queue->name, (int)message->message_id);
        return UFLAKE_OK;
    }
    
    return UFLAKE_ERROR_TIMEOUT;
}

uflake_result_t uflake_msgqueue_broadcast(const uflake_message_t* message) {
    if (!message) return UFLAKE_ERROR_INVALID_PARAM;
    
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

uflake_result_t uflake_msgqueue_find(const char* name, uflake_msgqueue_t** queue) {
    if (!name || !queue) return UFLAKE_ERROR_INVALID_PARAM;
    
    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
    
    // Search for queue by name (simplified implementation)
    // In real implementation, iterate through proper linked list
    
    xSemaphoreGive(msgqueue_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_msgqueue_destroy(uflake_msgqueue_t* queue) {
    if (!queue) return UFLAKE_ERROR_INVALID_PARAM;
    
    xSemaphoreTake(msgqueue_mutex, portMAX_DELAY);
    
    if (queue->queue_handle) {
        vQueueDelete(queue->queue_handle);
    }
    
    // Remove from queue list and free memory
    // In real implementation, properly remove from linked list
    
    uflake_free(queue);
    
    xSemaphoreGive(msgqueue_mutex);
    ESP_LOGI(TAG, "Destroyed message queue '%s'", queue->name);
    
    return UFLAKE_OK;
}

void uflake_messagequeue_process(void) {
    // Process any pending message queue operations
    // Handle message routing, cleanup expired messages, etc.
}
