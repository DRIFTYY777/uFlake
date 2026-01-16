#ifndef UFLAKE_PANIC_HANDLER_H
#define UFLAKE_PANIC_HANDLER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        PANIC_REASON_STACK_OVERFLOW,
        PANIC_REASON_MEMORY_CORRUPTION,
        PANIC_REASON_WATCHDOG_TIMEOUT,
        PANIC_REASON_ASSERTION_FAILED,
        PANIC_REASON_USER_ABORT,
        PANIC_REASON_UNKNOWN
    } panic_reason_t;

    typedef struct
    {
        panic_reason_t reason;
        uint32_t timestamp;
        uint32_t task_handle;
        char task_name[16];
        void *stack_pointer;
        char message[64];
    } panic_info_t;

    uflake_result_t uflake_panic_init(void);
    void uflake_panic_trigger(panic_reason_t reason, const char *message);
    void uflake_panic_check(void);
    uflake_result_t uflake_panic_get_last_info(panic_info_t *info);

#define UFLAKE_ASSERT(condition, message)                                 \
    do                                                                    \
    {                                                                     \
        if (!(condition))                                                 \
        {                                                                 \
            uflake_panic_trigger(PANIC_REASON_ASSERTION_FAILED, message); \
        }                                                                 \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_PANIC_HANDLER_H
