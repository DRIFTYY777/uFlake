#include "sdWrite.h"
#include <stdio.h>
#include "common.h"

// typedef struct
// {
//     FILE *fp;
// } sd_ctx_t;

bool sd_write_open(void *ctx, const char *path)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    sd->fp = fopen(path, "wb");
    return (sd->fp != NULL);
}

size_t sd_write_data(void *ctx, const void *src, size_t len)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fwrite(src, 1, len, sd->fp);
}

bool sd_flush(void *ctx)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fflush(sd->fp) == 0;
}