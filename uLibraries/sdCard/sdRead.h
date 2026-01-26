#if !defined(SD_READ)
#define SD_READ

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    static bool sd_open(void *ctx, const char *path);
    static void sd_close(void *ctx);
    static size_t sd_read(void *ctx, void *dst, size_t len);
    static bool sd_seek(void *ctx, size_t offset);

#ifdef __cplusplus
}
#endif

#endif // SD_READ
