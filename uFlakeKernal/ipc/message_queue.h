#ifndef UFLAKE_MESSAGE_QUEUE_H
#define UFLAKE_MESSAGE_QUEUE_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UFLAKE_MAX_MESSAGE_SIZE 256
#define UFLAKE_MAX_QUEUE_NAME 32

// Message types
typedef enum {
    MSG_TYPE_DATA = 0,
    MSG_TYPE_COMMAND,
    MSG_TYPE_EVENT,
    MSG_TYPE_RESPONSE,
    MSG_TYPE_BROADCAST,
    MSG_TYPE_SYSTEM
} uflake_msg_type_t;

// Message priority
typedef enum {
    MSG_PRIORITY_LOW = 0,
    MSG_PRIORITY_NORMAL = 1,
    MSG_PRIORITY_HIGH = 2,
    MSG_PRIORITY_CRITICAL = 3
} uflake_msg_priority_t;

// Message structure
typedef struct {
    uint32_t sender_pid;
    uint32_t receiver_pid;
    uflake_msg_type_t type;
    uflake_msg_priority_t priority;
    uint32_t message_id;
    uint32_t timestamp;
    size_t data_size;
    uint8_t data[UFLAKE_MAX_MESSAGE_SIZE];
} uflake_message_t;

// Message queue handle
typedef struct {
    char name[UFLAKE_MAX_QUEUE_NAME];
    QueueHandle_t queue_handle;
    uint32_t max_messages;
    uint32_t message_count;
    uint32_t owner_pid;
    bool is_public;
} uflake_msgqueue_t;

// Message queue functions
uflake_result_t uflake_messagequeue_init(void);
uflake_result_t uflake_msgqueue_create(const char* name, uint32_t max_messages, 
                                      bool is_public, uflake_msgqueue_t** queue);
uflake_result_t uflake_msgqueue_send(uflake_msgqueue_t* queue, const uflake_message_t* message, 
                                    uint32_t timeout_ms);
uflake_result_t uflake_msgqueue_receive(uflake_msgqueue_t* queue, uflake_message_t* message, 
                                       uint32_t timeout_ms);
uflake_result_t uflake_msgqueue_broadcast(const uflake_message_t* message);
uflake_result_t uflake_msgqueue_find(const char* name, uflake_msgqueue_t** queue);
uflake_result_t uflake_msgqueue_destroy(uflake_msgqueue_t* queue);
void uflake_messagequeue_process(void);

// Helper macros
#define UFLAKE_MSG_SEND(queue, type, data, size) \
    do { \
        uflake_message_t msg = {0}; \
        msg.sender_pid = uflake_process_get_current() ? uflake_process_get_current()->pid : 0; \
        msg.type = type; \
        msg.priority = MSG_PRIORITY_NORMAL; \
        msg.timestamp = uflake_kernel_get_tick_count(); \
        msg.data_size = (size > UFLAKE_MAX_MESSAGE_SIZE) ? UFLAKE_MAX_MESSAGE_SIZE : size; \
        memcpy(msg.data, data, msg.data_size); \
        uflake_msgqueue_send(queue, &msg, 1000); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_MESSAGE_QUEUE_H
