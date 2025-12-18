#ifndef UFLAKE_SCHEDULER_H
#define UFLAKE_SCHEDULER_H

#include "../kernel.h"

#ifdef __cplusplus 
extern "C" {
#endif

// Process states
typedef enum {
    PROCESS_STATE_CREATED,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} process_state_t;

// Process priority levels
typedef enum {
    PROCESS_PRIORITY_IDLE = 0,
    PROCESS_PRIORITY_LOW = 1,
    PROCESS_PRIORITY_NORMAL = 2,
    PROCESS_PRIORITY_HIGH = 3,
    PROCESS_PRIORITY_CRITICAL = 4
} process_priority_t;

// Process entry point function
typedef void (*process_entry_t)(void* args);

// Process control block
struct uflake_process_t {
    uint32_t pid;
    char name[32];
    process_state_t state;
    process_priority_t priority;
    TaskHandle_t task_handle;
    void* stack_ptr;
    size_t stack_size;
    uint32_t cpu_time;
    struct uflake_process_t* next;
};

// Scheduler functions
uflake_result_t uflake_scheduler_init(void);
uflake_result_t uflake_process_create(const char* name, process_entry_t entry, void* args, 
                                     size_t stack_size, process_priority_t priority, uint32_t* pid);
uflake_result_t uflake_process_terminate(uint32_t pid);
uflake_result_t uflake_process_suspend(uint32_t pid);
uflake_result_t uflake_process_resume(uint32_t pid);
void uflake_scheduler_tick(void);
uflake_process_t* uflake_process_get_current(void);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_SCHEDULER_H
