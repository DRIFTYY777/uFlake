#include "sdRead.h"
#include <stdio.h>
#include "common.h"

// typedef struct
// {
//     FILE *fp;
// } sd_ctx_t;

static bool sd_open(void *ctx, const char *path)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    sd->fp = fopen(path, "rb");
    return (sd->fp != NULL);
}

static size_t sd_read(void *ctx, void *dst, size_t len)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fread(dst, 1, len, sd->fp);
}

static bool sd_seek(void *ctx, size_t offset)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fseek(sd->fp, offset, SEEK_SET) == 0;
}

static void sd_close(void *ctx)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    if (sd->fp)
    {
        fclose(sd->fp);
        sd->fp = NULL;
    }
}