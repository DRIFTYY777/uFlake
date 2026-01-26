#if !defined(SD_WRITE_H)
#define SD_WRITE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif
    static bool sd_write_open(void *ctx, const char *path);
    static size_t sd_write_data(void *ctx, const void *src, size_t len);
    static bool sd_flush(void *ctx);

#ifdef __cplusplus
}
#endif

#endif // SD_WRITE_H
