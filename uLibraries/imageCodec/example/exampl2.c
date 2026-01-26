//////////////////////////////////////////////////////////////////////
typedef struct
{
    FILE *fp;
    char path[256];
} sd_reader_ctx_t;

static bool sd_reader_open(void *user_ctx, const char *path)
{
    sd_reader_ctx_t *c = (sd_reader_ctx_t *)user_ctx;
    if (!c)
        return false;
    const char *full = path;
    if (path[0] != '/')
    {
        snprintf(c->path, sizeof(c->path), "/sd/%s", path);
        full = c->path;
    }
    c->fp = fopen(full, "rb");
    if (!c->fp)
    {
        ESP_LOGE(TAG, "fopen failed: %s", full);
        return false;
    }
    return true;
}

static void sd_reader_close(void *user_ctx)
{
    sd_reader_ctx_t *c = (sd_reader_ctx_t *)user_ctx;
    if (!c)
        return;
    if (c->fp)
    {
        fclose(c->fp);
        c->fp = NULL;
    }
}

static size_t sd_reader_read(void *user_ctx, void *dst, size_t len)
{
    sd_reader_ctx_t *c = (sd_reader_ctx_t *)user_ctx;
    if (!c || !c->fp)
        return 0;
    return fread(dst, 1, len, c->fp);
}

static bool sd_reader_seek(void *user_ctx, size_t offset)
{
    sd_reader_ctx_t *c = (sd_reader_ctx_t *)user_ctx;
    if (!c || !c->fp)
        return false;
    return fseek(c->fp, offset, SEEK_SET) == 0;
}

static size_t sd_reader_size(void *user_ctx)
{
    sd_reader_ctx_t *c = (sd_reader_ctx_t *)user_ctx;
    if (!c || !c->fp)
        return 0;
    long pos = ftell(c->fp);
    fseek(c->fp, 0, SEEK_END);
    long sz = ftell(c->fp);
    fseek(c->fp, pos, SEEK_SET);
    return (size_t)sz;
}

static void sd_reader_destroy(img_reader_t *r)
{
    if (!r)
        return;
    sd_reader_ctx_t *c = (sd_reader_ctx_t *)r->user_ctx;
    if (c)
    {
        if (c->fp)
            fclose(c->fp);
        free(c);
    }
}

img_reader_t img_reader_sd_create(void)
{
    sd_reader_ctx_t *ctx = calloc(1, sizeof(sd_reader_ctx_t));
    img_reader_t r = {0};
    r.user_ctx = ctx;
    r.open = sd_reader_open;
    r.read = sd_reader_read;
    r.seek = sd_reader_seek;
    r.size = sd_reader_size;
    r.close = sd_reader_close;
    return r;
}

lv_obj_t *img_display_from_sd(const char *path)
{
    img_reader_t reader = img_reader_sd_create();
    img_rgb565_t img;
    memset(&img, 0, sizeof(img));

    if (!reader.open(reader.user_ctx, path))
    {
        ESP_LOGE(TAG, "Failed to open %s", path);
        sd_reader_destroy(&reader);
        return NULL;
    }

    bool ok = img_decode_rgb565(path, IMG_FMT_AUTO, &reader, &img);
    if (!ok)
    {
        ESP_LOGE(TAG, "Decode failed: %s", path);
        reader.close(reader.user_ctx);
        sd_reader_destroy(&reader);
        return NULL;
    }

    reader.close(reader.user_ctx);
    sd_reader_destroy(&reader);

    /* Create LVGL image descriptor that points to decoded pixels */
    lv_img_dsc_t *dsc = uflake_malloc(sizeof(lv_img_dsc_t), UFLAKE_MEM_SPIRAM);
    if (!dsc)
    {
        ESP_LOGE(TAG, "dsc alloc failed");
        img_free(&img);
        return NULL;
    }
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.w = img.width;
    dsc->header.h = img.height;
    dsc->data_size = img.size;

    /* LVGL color format: assume 16-bit RGB565 true color */
    dsc->header.cf = LV_COLOR_FORMAT_NATIVE; /* may be LV_IMG_CF_TRUE_COLOR depending on LVGL version */
    dsc->data = img.pixels;

    lv_obj_t *imgobj = lv_img_create(lv_scr_act());
    lv_img_set_src(imgobj, (const void *)dsc);
    lv_obj_center(imgobj);

    /* Keep descriptor and pixel buffer alive by storing pointer in object user data */
    lv_obj_set_user_data(imgobj, dsc);

    return imgobj;
}
///////////////////////////////////////////////////////////////////////