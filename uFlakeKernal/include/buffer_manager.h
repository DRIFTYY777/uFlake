#ifndef UFLAKE_BUFFER_MANAGER_H
#define UFLAKE_BUFFER_MANAGER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        void *data;
        size_t size;
        size_t capacity;
        uint32_t ref_count;
        bool is_allocated;
    } uflake_buffer_t;

    uflake_result_t uflake_buffer_init(void);
    uflake_result_t uflake_buffer_create(uflake_buffer_t **buffer, size_t capacity);
    uflake_result_t uflake_buffer_write(uflake_buffer_t *buffer, const void *data, size_t size);
    uflake_result_t uflake_buffer_read(uflake_buffer_t *buffer, void *data, size_t size);
    uflake_result_t uflake_buffer_resize(uflake_buffer_t *buffer, size_t new_capacity);
    uflake_result_t uflake_buffer_destroy(uflake_buffer_t *buffer);

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_BUFFER_MANAGER_H
