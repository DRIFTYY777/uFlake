#ifndef UFLAKE_KERNEL_H
#define UFLAKE_KERNEL_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

//  Mark as defined BEFORE the typedef
#define UFLAKE_RESULT_T_DEFINED

    // Kernel return codes (define BEFORE includes)
    typedef enum
    {
        UFLAKE_OK = 0,
        UFLAKE_ERROR = -1,
        UFLAKE_ERROR_MEMORY = -2,
        UFLAKE_ERROR_INVALID_PARAM = -3,
        UFLAKE_ERROR_TIMEOUT = -4,
        UFLAKE_ERROR_NOT_FOUND = -5
    } uflake_result_t;

    // Forward declarations (BEFORE includes to break circular deps)
    typedef struct uflake_process_t uflake_process_t;
    typedef struct uflake_thread_t uflake_thread_t;

//  Move subsystem includes AFTER forward declarations
#include "memory/memory_manager.h"
#include "scheduler/scheduler.h"
#include "sync/synchronization.h"
#include "crypto/crypto_engine.h"
#include "logging/logger.h"
#include "panic/panic_handler.h"
#include "buffer/buffer_manager.h"
#include "timer/timer_manager.h"
#include "ipc/message_queue.h"
#include "watchdog/watchdog_manager.h"
#include "event/event_manager.h"
#include "resource/resource_manager.h"

// Kernel configuration
#define UFLAKE_MAX_PROCESSES 16
#define UFLAKE_MAX_THREADS_PER_PROCESS 8
#define UFLAKE_KERNEL_STACK_SIZE 4096
#define UFLAKE_KERNEL_PRIORITY 24

    // Kernel state
    typedef enum
    {
        KERNEL_STATE_UNINITIALIZED,
        KERNEL_STATE_INITIALIZING,
        KERNEL_STATE_RUNNING,
        KERNEL_STATE_PANIC
    } kernel_state_t;

    // Kernel context structure
    typedef struct
    {
        kernel_state_t state;
        uint32_t tick_count;
        uflake_process_t *current_process;
        TaskHandle_t kernel_task;
        SemaphoreHandle_t kernel_mutex;
    } uflake_kernel_t;

    // Core kernel functions
    uflake_result_t uflake_kernel_init(void);
    uflake_result_t uflake_kernel_start(void);
    uflake_result_t uflake_kernel_shutdown(void);
    kernel_state_t uflake_kernel_get_state(void);
    uint32_t uflake_kernel_get_tick_count(void);

    // ISR context detection (CRITICAL for hardware interrupts)
    static inline bool uflake_kernel_is_in_isr(void)
    {
        return xPortInIsrContext() != 0;
    }

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_KERNEL_H
