#ifndef UFLAKE_RESOURCE_MANAGER_H
#define UFLAKE_RESOURCE_MANAGER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        RESOURCE_TYPE_FILE = 0,
        RESOURCE_TYPE_SOCKET,
        RESOURCE_TYPE_MEMORY_REGION,
        RESOURCE_TYPE_HARDWARE_PERIPHERAL,
        RESOURCE_TYPE_MUTEX,
        RESOURCE_TYPE_SEMAPHORE
    } resource_type_t;

    typedef struct
    {
        uint32_t resource_id;
        resource_type_t type;
        uint32_t owner_pid;
        void *resource_ptr;
        char name[32];
        uint32_t ref_count;
        bool is_shareable;
        uint32_t created_time;
    } uflake_resource_t;

    uflake_result_t uflake_resource_init(void);
    uflake_result_t uflake_resource_register(const char *name, resource_type_t type,
                                             void *resource_ptr, bool is_shareable, uint32_t *resource_id);
    uflake_result_t uflake_resource_acquire(uint32_t resource_id, uint32_t requesting_pid);
    uflake_result_t uflake_resource_release(uint32_t resource_id, uint32_t releasing_pid);
    uflake_result_t uflake_resource_find_by_name(const char *name, uint32_t *resource_id);
    uflake_result_t uflake_resource_cleanup_for_process(uint32_t pid);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_RESOURCE_MANAGER_H
