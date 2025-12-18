#ifndef UFLAKE_WATCHDOG_MANAGER_H
#define UFLAKE_WATCHDOG_MANAGER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WATCHDOG_TYPE_TASK = 0,
    WATCHDOG_TYPE_INTERRUPT,
    WATCHDOG_TYPE_SYSTEM
} watchdog_type_t;

typedef struct {
    uint32_t watchdog_id;
    watchdog_type_t type;
    uint32_t timeout_ms;
    uint32_t last_feed;
    bool is_active;
    char name[32];
} uflake_watchdog_t;

uflake_result_t uflake_watchdog_init(void);
uflake_result_t uflake_watchdog_create(const char* name, watchdog_type_t type, 
                                      uint32_t timeout_ms, uint32_t* watchdog_id);
uflake_result_t uflake_watchdog_feed_by_id(uint32_t watchdog_id);
uflake_result_t uflake_watchdog_delete(uint32_t watchdog_id);
void uflake_watchdog_feed(void);
void uflake_watchdog_check_timeouts(void);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_WATCHDOG_MANAGER_H
