#ifndef UFLAKE_MEMORY_MANAGER_H
#define UFLAKE_MEMORY_MANAGER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Memory types
    typedef enum
    {
        UFLAKE_MEM_INTERNAL,
        UFLAKE_MEM_SPIRAM,
        UFLAKE_MEM_DMA
    } uflake_mem_type_t;

    // Memory statistics
    typedef struct
    {
        size_t total_size;
        size_t free_size;
        size_t used_size;
        size_t largest_free_block;
        uint32_t allocations;
        uint32_t deallocations;
    } uflake_mem_stats_t;

    // Memory manager functions
    uflake_result_t uflake_memory_init(void);
    void *uflake_malloc(size_t size, uflake_mem_type_t type);
    void *uflake_calloc(size_t count, size_t size, uflake_mem_type_t type);
    void *uflake_realloc(void *ptr, size_t size);
    void uflake_free(void *ptr);
    uflake_result_t uflake_memory_get_stats(uflake_mem_type_t type, uflake_mem_stats_t *stats);
    size_t uflake_memory_get_free_size(uflake_mem_type_t type);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_MEMORY_MANAGER_H
