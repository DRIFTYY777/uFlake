#ifndef UFLAKE_TIMER_MANAGER_H
#define UFLAKE_TIMER_MANAGER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*timer_callback_t)(void* args);

typedef struct {
    uint32_t timer_id;
    uint32_t interval_ms;
    uint32_t next_trigger;
    timer_callback_t callback;
    void* args;
    bool is_periodic;
    bool is_active;
} uflake_timer_t;

uflake_result_t uflake_timer_init(void);
uflake_result_t uflake_timer_create(uint32_t* timer_id, uint32_t interval_ms, 
                                   timer_callback_t callback, void* args, bool periodic);
uflake_result_t uflake_timer_start(uint32_t timer_id);
uflake_result_t uflake_timer_stop(uint32_t timer_id);
uflake_result_t uflake_timer_delete(uint32_t timer_id);
void uflake_timer_process(void);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_TIMER_MANAGER_H
