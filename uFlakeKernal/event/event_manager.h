#ifndef UFLAKE_EVENT_MANAGER_H
#define UFLAKE_EVENT_MANAGER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UFLAKE_MAX_EVENT_NAME 32
#define UFLAKE_MAX_EVENT_DATA 64

    typedef enum
    {
        EVENT_TYPE_SYSTEM = 0,
        EVENT_TYPE_HARDWARE,
        EVENT_TYPE_USER,
        EVENT_TYPE_TIMER,
        EVENT_TYPE_ERROR
    } event_type_t;

    typedef void (*event_callback_t)(const void *event_data);

    typedef struct
    {
        char name[UFLAKE_MAX_EVENT_NAME];
        event_type_t type;
        uint32_t timestamp;
        size_t data_size;
        uint8_t data[UFLAKE_MAX_EVENT_DATA];
    } uflake_event_t;

    typedef struct
    {
        uint32_t subscription_id;
        char event_name[UFLAKE_MAX_EVENT_NAME];
        event_callback_t callback;
        uint32_t subscriber_pid;
    } event_subscription_t;

    uflake_result_t uflake_event_init(void);
    uflake_result_t uflake_event_publish(const char *event_name, event_type_t type,
                                         const void *data, size_t data_size);
    uflake_result_t uflake_event_subscribe(const char *event_name, event_callback_t callback,
                                           uint32_t *subscription_id);
    uflake_result_t uflake_event_unsubscribe(uint32_t subscription_id);
    void uflake_event_process(void);

// Pre-defined system events
#define UFLAKE_EVENT_PROCESS_CREATED "proc.created"
#define UFLAKE_EVENT_PROCESS_TERMINATED "proc.terminated"
#define UFLAKE_EVENT_MEMORY_LOW "mem.low"
#define UFLAKE_EVENT_SYSTEM_PANIC "sys.panic"

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_EVENT_MANAGER_H
