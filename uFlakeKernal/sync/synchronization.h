#ifndef UFLAKE_SYNCHRONIZATION_H
#define UFLAKE_SYNCHRONIZATION_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // uFlake mutex handle
    typedef struct
    {
        SemaphoreHandle_t handle;
        uint32_t owner_pid;
        uint32_t lock_count;
    } uflake_mutex_t;

    // uFlake semaphore handle
    typedef struct
    {
        SemaphoreHandle_t handle;
        uint32_t max_count;
        uint32_t current_count;
    } uflake_semaphore_t;

    uflake_result_t uflake_sync_init(void);

    // Mutex operations
    uflake_result_t uflake_mutex_create(uflake_mutex_t **mutex);
    uflake_result_t uflake_mutex_lock(uflake_mutex_t *mutex, uint32_t timeout_ms);
    uflake_result_t uflake_mutex_unlock(uflake_mutex_t *mutex);
    uflake_result_t uflake_mutex_destroy(uflake_mutex_t *mutex);

    // Semaphore operations (ISR-safe)
    uflake_result_t uflake_semaphore_create(uflake_semaphore_t **semaphore, uint32_t initial_count, uint32_t max_count);
    uflake_result_t uflake_semaphore_take(uflake_semaphore_t *semaphore, uint32_t timeout_ms);
    uflake_result_t uflake_semaphore_give(uflake_semaphore_t *semaphore); // ISR-safe: auto-detects context
    uflake_result_t uflake_semaphore_destroy(uflake_semaphore_t *semaphore);

    // Note: Mutexes CANNOT be used in ISR context - use semaphores instead!

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_SYNCHRONIZATION_H
