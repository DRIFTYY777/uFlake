#include "imageCodec.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "logger.h"
#include "kernel.h"

/* ESP NEW JPEG (Correct API from esp_new_jpeg) */
#include "esp_jpeg_dec.h"
#include "esp_jpeg_enc.h"

/* LVGL (optional, for screenshots) */
#ifdef CONFIG_USE_LVGL
#include "lvgl.h"
#endif

static const char *TAG = "IMG_CODEC";

/* Memory allocation helpers from esp_new_jpeg */
#define jpeg_malloc malloc
#define jpeg_calloc_align(size, align) heap_caps_aligned_alloc(align, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define jpeg_free_align(ptr) heap_caps_free(ptr)

/* ============================================================================
 *  INTERNAL HELPERS
 * ========================================================================== */

static img_format_t detect_format(const img_reader_t *r)
{
    uint8_t sig[8];
    r->seek(r->user_ctx, 0);
    r->read(r->user_ctx, sig, sizeof(sig));
    r->seek(r->user_ctx, 0);

    /* JPEG signature */
    if (sig[0] == 0xFF && sig[1] == 0xD8)
        return IMG_FMT_JPEG;

    /* PNG signature */
    if (!memcmp(sig, "\x89PNG\r\n\x1A\n", 8))
        return IMG_FMT_PNG;

    return IMG_FMT_AUTO;
}

/* ============================================================================
 *  JPEG DECODER (esp_new_jpeg correct API)
 * ========================================================================== */

static bool decode_jpeg(const img_reader_t *r,
                        const img_decode_opts_t *opts,
                        img_rgb565_t *out)
{
    jpeg_error_t ret = JPEG_ERR_OK;
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;
    uint8_t *jpg_buf = NULL;
    uint8_t *out_buf = NULL;

    size_t jpg_size = r->size(r->user_ctx);
    UFLAKE_LOGI(TAG, "Allocating %zu bytes for JPEG input", jpg_size);

    // Try INTERNAL memory first for input buffer
    jpg_buf = (uint8_t *)uflake_malloc(jpg_size, UFLAKE_MEM_SPIRAM);
    if (!jpg_buf)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate JPEG input buffer (%zu bytes)", jpg_size);
        // uflake_mem_print_stats();
        return false;
    }

    r->seek(r->user_ctx, 0);
    size_t read_size = r->read(r->user_ctx, jpg_buf, jpg_size);
    if (read_size != jpg_size)
    {
        UFLAKE_LOGE(TAG, "Failed to read JPEG file");
        uflake_free(jpg_buf);
        return false;
    }

    /* Configure decoder */
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
    config.rotate = JPEG_ROTATE_0D;

    /* Apply HW rotation if requested */
    if (opts)
    {
        switch (opts->rotate)
        {
        case IMG_ROTATE_90:
            config.rotate = JPEG_ROTATE_90D;
            break;
        case IMG_ROTATE_180:
            config.rotate = JPEG_ROTATE_180D;
            break;
        case IMG_ROTATE_270:
            config.rotate = JPEG_ROTATE_270D;
            break;
        default:
            config.rotate = JPEG_ROTATE_0D;
            break;
        }
    }

    /* Apply HW scaling if requested */
    if (opts && opts->scale != IMG_SCALE_NONE)
    {
        // Get image dimensions first to calculate scale
        jpeg_dec_handle_t temp_dec = NULL;
        jpeg_dec_io_t temp_io = {0};
        jpeg_dec_header_info_t temp_info = {0};

        jpeg_dec_config_t temp_cfg = DEFAULT_JPEG_DEC_CONFIG();
        if (jpeg_dec_open(&temp_cfg, &temp_dec) == JPEG_ERR_OK)
        {
            temp_io.inbuf = jpg_buf;
            temp_io.inbuf_len = jpg_size;

            if (jpeg_dec_parse_header(temp_dec, &temp_io, &temp_info) == JPEG_ERR_OK)
            {
                switch (opts->scale)
                {
                case IMG_SCALE_1_2:
                    config.scale.width = temp_info.width / 2;
                    config.scale.height = temp_info.height / 2;
                    break;
                case IMG_SCALE_1_4:
                    config.scale.width = temp_info.width / 4;
                    config.scale.height = temp_info.height / 4;
                    break;
                case IMG_SCALE_1_8:
                    config.scale.width = temp_info.width / 8;
                    config.scale.height = temp_info.height / 8;
                    break;
                default:
                    break;
                }
            }
            jpeg_dec_close(temp_dec);
        }
    }

    /* Open decoder */
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK)
    {
        UFLAKE_LOGE(TAG, "JPEG decoder init failed: %d", ret);
        uflake_free(jpg_buf);
        return false;
    }

    /* Allocate IO structure */
    jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (!jpeg_io)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate IO structure");
        ret = JPEG_ERR_NO_MEM;
        goto cleanup;
    }

    /* Allocate output info structure */
    out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (!out_info)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate output info");
        ret = JPEG_ERR_NO_MEM;
        goto cleanup;
    }

    /* Set input buffer */
    jpeg_io->inbuf = jpg_buf;
    jpeg_io->inbuf_len = jpg_size;

    /* Parse header */
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret != JPEG_ERR_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to parse JPEG header: %d", ret);
        goto cleanup;
    }

    /* Calculate output size (RGB565 = 2 bytes per pixel) */
    int out_len = out_info->width * out_info->height * 2;

    /* Allocate output buffer (aligned for DMA) */
    out_buf = jpeg_calloc_align(out_len, 16);
    if (!out_buf)
    {
        out_buf = malloc(out_len);
    }

    if (!out_buf)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate output buffer");
        ret = JPEG_ERR_NO_MEM;
        goto cleanup;
    }

    jpeg_io->outbuf = out_buf;

    /* Decode */
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK)
    {
        UFLAKE_LOGE(TAG, "JPEG decode failed: %d", ret);
        goto cleanup;
    }

    /* Convert from big-endian (BE) to little-endian (LE) RGB565 for LVGL/ST7789 */
    uint16_t *pixels_u16 = (uint16_t *)out_buf;
    size_t pixel_count = out_info->width * out_info->height;
    for (size_t i = 0; i < pixel_count; i++)
    {
        uint16_t pixel = pixels_u16[i];
        pixels_u16[i] = (pixel >> 8) | (pixel << 8); // Swap bytes
    }

    /* Fill output structure */
    out->width = out_info->width;
    out->height = out_info->height;
    out->stride = out_info->width * 2;
    out->pixels = out_buf;
    out->size = out_len;

    /* Success - don't free out_buf */
    out_buf = NULL;

cleanup:
    jpeg_dec_close(jpeg_dec);
    if (jpeg_io)
        free(jpeg_io);
    if (out_info)
        free(out_info);
    if (jpg_buf)
        uflake_free(jpg_buf);
    if (out_buf)
    {
        if ((uintptr_t)out_buf % 16 == 0)
        {
            jpeg_free_align(out_buf);
        }
        else
        {
            free(out_buf);
        }
    }

    return (ret == JPEG_ERR_OK);
}

/* ============================================================================
 *  PNG DECODER (Stub)
 * ========================================================================== */

static bool decode_png(const img_reader_t *r,
                       const img_decode_opts_t *opts,
                       img_rgb565_t *out)
{
    (void)r;
    (void)opts;
    (void)out;
    UFLAKE_LOGE(TAG, "PNG decode not implemented");
    return false;
}

/* ============================================================================
 *  IMAGE TRANSFORMS (Software)
 * ========================================================================== */

static bool resize_rgb565(img_rgb565_t *img, uint16_t new_w, uint16_t new_h)
{
    if (img->width == new_w && img->height == new_h)
        return true;

    uint8_t *dst = heap_caps_malloc(new_w * new_h * 2,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dst)
        dst = malloc(new_w * new_h * 2);

    if (!dst)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate resize buffer");
        return false;
    }

    uint16_t src_w = img->width;
    uint16_t src_h = img->height;

    /* Nearest neighbor resize */
    for (uint16_t y = 0; y < new_h; y++)
    {
        uint16_t src_y = (y * src_h) / new_h;
        for (uint16_t x = 0; x < new_w; x++)
        {
            uint16_t src_x = (x * src_w) / new_w;

            uint16_t *s = (uint16_t *)(img->pixels +
                                       (src_y * img->stride) + src_x * 2);
            uint16_t *d = (uint16_t *)(dst + (y * new_w + x) * 2);
            *d = *s;
        }
    }

    free(img->pixels);
    img->pixels = dst;
    img->width = new_w;
    img->height = new_h;
    img->stride = new_w * 2;
    img->size = new_w * new_h * 2;

    return true;
}

static bool rotate_rgb565_sw(img_rgb565_t *img, img_rotate_t rot)
{
    if (rot == IMG_ROTATE_0)
        return true;

    uint16_t new_w = img->width;
    uint16_t new_h = img->height;

    if (rot == IMG_ROTATE_90 || rot == IMG_ROTATE_270)
    {
        new_w = img->height;
        new_h = img->width;
    }

    uint8_t *dst = heap_caps_malloc(new_w * new_h * 2,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dst)
        dst = malloc(new_w * new_h * 2);

    if (!dst)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate rotation buffer");
        return false;
    }

    for (uint16_t y = 0; y < img->height; y++)
    {
        for (uint16_t x = 0; x < img->width; x++)
        {
            uint16_t *src = (uint16_t *)(img->pixels +
                                         y * img->stride + x * 2);
            uint16_t dx = 0, dy = 0;

            switch (rot)
            {
            case IMG_ROTATE_90:
                dx = img->height - 1 - y;
                dy = x;
                break;
            case IMG_ROTATE_180:
                dx = img->width - 1 - x;
                dy = img->height - 1 - y;
                break;
            case IMG_ROTATE_270:
                dx = y;
                dy = img->width - 1 - x;
                break;
            default:
                break;
            }

            uint16_t *dstp = (uint16_t *)(dst + (dy * new_w + dx) * 2);
            *dstp = *src;
        }
    }

    free(img->pixels);
    img->pixels = dst;
    img->width = new_w;
    img->height = new_h;
    img->stride = new_w * 2;
    img->size = new_w * new_h * 2;

    return true;
}

/* ============================================================================
 *  JPEG ENCODER (esp_new_jpeg correct API)
 * ========================================================================== */

bool img_encode_jpeg_ex(const img_rgb565_t *img,
                        const char *path,
                        const img_writer_t *writer,
                        const img_encode_opts_t *opts)
{
    if (!img || !img->pixels || !path || !writer)
    {
        UFLAKE_LOGE(TAG, "Invalid encode parameters");
        return false;
    }

    uint8_t quality = opts ? opts->quality : 85;
    if (quality < 1)
        quality = 1;
    if (quality > 100)
        quality = 100;

    /* Configure encoder */
    jpeg_enc_config_t config = DEFAULT_JPEG_ENC_CONFIG();
    config.width = img->width;
    config.height = img->height;
    config.src_type = JPEG_PIXEL_FORMAT_RGB565_BE;
    config.subsampling = JPEG_SUBSAMPLE_420;
    config.quality = quality;
    config.rotate = JPEG_ROTATE_0D;
    config.task_enable = false;

    jpeg_error_t ret = JPEG_ERR_OK;
    jpeg_enc_handle_t jpeg_enc = NULL;
    uint8_t *outbuf = NULL;
    int outbuf_size = img->width * img->height * 2; // Worst case
    int out_len = 0;

    /* Open encoder */
    ret = jpeg_enc_open(&config, &jpeg_enc);
    if (ret != JPEG_ERR_OK)
    {
        UFLAKE_LOGE(TAG, "JPEG encoder init failed: %d", ret);
        return false;
    }

    /* Allocate output buffer */
    outbuf = calloc(1, outbuf_size);
    if (!outbuf)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate encode buffer");
        jpeg_enc_close(jpeg_enc);
        return false;
    }

    /* Encode */
    ret = jpeg_enc_process(jpeg_enc, img->pixels, img->size,
                           outbuf, outbuf_size, &out_len);

    jpeg_enc_close(jpeg_enc);

    if (ret != JPEG_ERR_OK)
    {
        UFLAKE_LOGE(TAG, "JPEG encode failed: %d", ret);
        free(outbuf);
        return false;
    }

    /* Write to file */
    bool ok = false;
    if (writer->open(writer->user_ctx, path))
    {
        size_t written = writer->write(writer->user_ctx, outbuf, out_len);
        if (writer->flush)
            writer->flush(writer->user_ctx);
        writer->close(writer->user_ctx);

        ok = (written == (size_t)out_len);
    }

    free(outbuf);

    if (!ok)
    {
        UFLAKE_LOGE(TAG, "Failed to write JPEG file");
    }

    return ok;
}

bool img_encode_jpeg(const img_rgb565_t *img,
                     const char *path,
                     const img_writer_t *writer)
{
    img_encode_opts_t opts = {.quality = 85};
    return img_encode_jpeg_ex(img, path, writer, &opts);
}

/* ============================================================================
 *  SCREENSHOT (LVGL Integration)
 * ========================================================================== */

#ifdef CONFIG_USE_LVGL

bool img_screenshot_lvgl(img_rgb565_t *out)
{
    lv_obj_t *scr = lv_scr_act();
    if (!scr)
    {
        UFLAKE_LOGE(TAG, "No active LVGL screen");
        return false;
    }

    lv_display_t *disp = lv_display_get_default();
    if (!disp)
    {
        UFLAKE_LOGE(TAG, "No default display");
        return false;
    }

    uint16_t w = lv_display_get_horizontal_resolution(disp);
    uint16_t h = lv_display_get_vertical_resolution(disp);

    size_t size = w * h * 2;
    uint8_t *buf = heap_caps_malloc(size,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf)
        buf = malloc(size);

    if (!buf)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate screenshot buffer");
        return false;
    }

    /* Get the active framebuffer */
    void *fb = lv_display_get_buf_active(disp);

    if (fb)
    {
        memcpy(buf, fb, size);
    }
    else
    {
        UFLAKE_LOGW(TAG, "No active framebuffer");
        memset(buf, 0, size);
    }

    out->width = w;
    out->height = h;
    out->stride = w * 2;
    out->pixels = buf;
    out->size = size;

    return true;
}

bool img_screenshot_lvgl_to_jpeg(const char *path,
                                 const img_writer_t *writer,
                                 uint8_t quality)
{
    img_rgb565_t screen;
    if (!img_screenshot_lvgl(&screen))
    {
        return false;
    }

    img_encode_opts_t opts = {.quality = quality};
    bool ok = img_encode_jpeg_ex(&screen, path, writer, &opts);

    img_free(&screen);
    return ok;
}

#else

bool img_screenshot_lvgl(img_rgb565_t *out)
{
    (void)out;
    UFLAKE_LOGE(TAG, "LVGL not enabled");
    return false;
}

bool img_screenshot_lvgl_to_jpeg(const char *path,
                                 const img_writer_t *writer,
                                 uint8_t quality)
{
    (void)path;
    (void)writer;
    (void)quality;
    UFLAKE_LOGE(TAG, "LVGL not enabled");
    return false;
}

#endif

/* ============================================================================
 *  PUBLIC DECODER API
 * ========================================================================== */

bool img_decode_rgb565_ex(const char *path,
                          img_format_t fmt,
                          const img_reader_t *reader,
                          const img_decode_opts_t *opts,
                          img_rgb565_t *out)
{
    if (!path || !reader || !out)
    {
        UFLAKE_LOGE(TAG, "Invalid decode parameters");
        return false;
    }

    memset(out, 0, sizeof(*out));

    if (!reader->open(reader->user_ctx, path))
    {
        UFLAKE_LOGE(TAG, "Failed to open file: %s", path);
        return false;
    }

    if (fmt == IMG_FMT_AUTO)
        fmt = detect_format(reader);

    bool ok = false;

    switch (fmt)
    {
    case IMG_FMT_JPEG:
        ok = decode_jpeg(reader, opts, out);
        break;
    case IMG_FMT_PNG:
        ok = decode_png(reader, opts, out);
        break;
    default:
        UFLAKE_LOGE(TAG, "Unsupported format");
        ok = false;
    }

    reader->close(reader->user_ctx);

    if (!ok)
        return false;

    /* Apply software transforms if needed */
    if (opts)
    {
        /* Software resize (if not using HW scale) */
        if (opts->resize && opts->new_width && opts->new_height)
        {
            if (!resize_rgb565(out, opts->new_width, opts->new_height))
            {
                img_free(out);
                return false;
            }
        }

        /* Software rotation (if HW rotation not applied) */
        if (opts->rotate != IMG_ROTATE_0 && fmt != IMG_FMT_JPEG)
        {
            if (!rotate_rgb565_sw(out, opts->rotate))
            {
                img_free(out);
                return false;
            }
        }
    }

    return true;
}

bool img_decode_rgb565(const char *path,
                       img_format_t fmt,
                       const img_reader_t *reader,
                       img_rgb565_t *out)
{
    return img_decode_rgb565_ex(path, fmt, reader, NULL, out);
}

bool img_get_info(const char *path,
                  const img_reader_t *reader,
                  uint16_t *width,
                  uint16_t *height)
{
    if (!path || !reader || !width || !height)
    {
        return false;
    }

    if (!reader->open(reader->user_ctx, path))
    {
        return false;
    }

    size_t file_size = reader->size(reader->user_ctx);
    uint8_t *buf = malloc(file_size);
    if (!buf)
    {
        reader->close(reader->user_ctx);
        return false;
    }

    reader->seek(reader->user_ctx, 0);
    reader->read(reader->user_ctx, buf, file_size);
    reader->close(reader->user_ctx);

    /* Parse header only */
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t jpeg_io = {0};
    jpeg_dec_header_info_t out_info = {0};

    jpeg_error_t ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret == JPEG_ERR_OK)
    {
        jpeg_io.inbuf = buf;
        jpeg_io.inbuf_len = file_size;

        ret = jpeg_dec_parse_header(jpeg_dec, &jpeg_io, &out_info);
        if (ret == JPEG_ERR_OK)
        {
            *width = out_info.width;
            *height = out_info.height;
        }

        jpeg_dec_close(jpeg_dec);
    }

    free(buf);
    return (ret == JPEG_ERR_OK);
}

void img_free(img_rgb565_t *img)
{
    if (img && img->pixels)
    {
        free(img->pixels);
        img->pixels = NULL;
        img->size = 0;
    }
}