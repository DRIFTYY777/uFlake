/**
 * Complete usage example for imageCodec library
 * Demonstrates: Decoding, Encoding, Screenshots, Transforms
 */

#include "imageCodec.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "lvgl.h"

static const char *TAG = "IMG_EXAMPLE";

/* ============================================================================
 *  SD CARD READER IMPLEMENTATION
 * ========================================================================== */

typedef struct
{
    FILE *fp;
} sd_ctx_t;

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

static size_t sd_size(void *ctx)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    long pos = ftell(sd->fp);
    fseek(sd->fp, 0, SEEK_END);
    size_t size = ftell(sd->fp);
    fseek(sd->fp, pos, SEEK_SET);
    return size;
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

static img_reader_t sd_reader = {
    .open = sd_open,
    .read = sd_read,
    .seek = sd_seek,
    .size = sd_size,
    .close = sd_close,
};

/* ============================================================================
 *  SD CARD WRITER IMPLEMENTATION
 * ========================================================================== */

static bool sd_write_open(void *ctx, const char *path)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    sd->fp = fopen(path, "wb");
    return (sd->fp != NULL);
}

static size_t sd_write_data(void *ctx, const void *src, size_t len)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fwrite(src, 1, len, sd->fp);
}

static bool sd_flush(void *ctx)
{
    sd_ctx_t *sd = (sd_ctx_t *)ctx;
    return fflush(sd->fp) == 0;
}

static img_writer_t sd_writer = {
    .open = sd_write_open,
    .write = sd_write_data,
    .flush = sd_flush,
    .close = sd_close,
};

/* ============================================================================
 *  EXAMPLE 1: Simple JPEG Decode
 * ========================================================================== */

void example_simple_decode(void)
{
    sd_ctx_t ctx = {0};
    sd_reader.user_ctx = &ctx;

    img_rgb565_t img;

    if (img_decode_rgb565("/sdcard/photo.jpg", IMG_FMT_AUTO,
                          &sd_reader, &img))
    {
        ESP_LOGI(TAG, "Decoded: %dx%d, %zu bytes",
                 img.width, img.height, img.size);

        // Use image with LVGL
        lv_img_dsc_t dsc = {
            .header.w = img.width,
            .header.h = img.height,
            .header.cf = LV_IMG_CF_TRUE_COLOR,
            .data = img.pixels,
            .data_size = img.size,
        };

        lv_obj_t *img_obj = lv_img_create(lv_scr_act());
        lv_img_set_src(img_obj, &dsc);
        lv_obj_center(img_obj);

        // Don't free yet - LVGL is using it!
        // img_free(&img); // Free later when done
    }
}

/* ============================================================================
 *  EXAMPLE 2: Decode with Resize and Rotation
 * ========================================================================== */

void example_decode_with_transforms(void)
{
    sd_ctx_t ctx = {0};
    sd_reader.user_ctx = &ctx;

    img_decode_opts_t opts = {
        .resize = true,
        .new_width = 320,
        .new_height = 240,
        .rotate = IMG_ROTATE_90,
        .scale = IMG_SCALE_NONE, // Use software resize
    };

    img_rgb565_t img;

    if (img_decode_rgb565_ex("/sdcard/large.jpg", IMG_FMT_AUTO,
                             &sd_reader, &opts, &img))
    {
        ESP_LOGI(TAG, "Resized & rotated: %dx%d", img.width, img.height);

        // Use with LVGL...

        img_free(&img);
    }
}

/* ============================================================================
 *  EXAMPLE 3: Hardware-Accelerated Scaling (JPEG Only)
 * ========================================================================== */

void example_hw_scale(void)
{
    sd_ctx_t ctx = {0};
    sd_reader.user_ctx = &ctx;

    // Decode at 1/4 scale using JPEG hardware - MUCH faster!
    img_decode_opts_t opts = {
        .scale = IMG_SCALE_1_4, // HW accelerated
        .rotate = IMG_ROTATE_0,
    };

    img_rgb565_t img;

    if (img_decode_rgb565_ex("/sdcard/huge.jpg", IMG_FMT_JPEG,
                             &sd_reader, &opts, &img))
    {
        ESP_LOGI(TAG, "HW scaled to 1/4: %dx%d", img.width, img.height);
        img_free(&img);
    }
}

/* ============================================================================
 *  EXAMPLE 4: Get Image Info Without Full Decode
 * ========================================================================== */

void example_get_info(void)
{
    sd_ctx_t ctx = {0};
    sd_reader.user_ctx = &ctx;

    uint16_t width, height;

    if (img_get_info("/sdcard/photo.jpg", &sd_reader, &width, &height))
    {
        ESP_LOGI(TAG, "Image dimensions: %dx%d", width, height);

        // Now decide if you want to decode it
        if (width <= 800 && height <= 600)
        {
            // Decode full size
        }
        else
        {
            // Use HW scaling
        }
    }
}

/* ============================================================================
 *  EXAMPLE 5: Encode RGB565 to JPEG
 * ========================================================================== */

void example_encode(void)
{
    sd_ctx_t ctx = {0};
    sd_writer.user_ctx = &ctx;

    // Create a test image (normally you'd have actual pixel data)
    img_rgb565_t img = {
        .width = 320,
        .height = 240,
        .stride = 320 * 2,
        .pixels = malloc(320 * 240 * 2),
        .size = 320 * 240 * 2,
    };

    // Fill with test pattern
    memset(img.pixels, 0x1F, img.size); // Red

    img_encode_opts_t opts = {
        .quality = 90,
    };

    if (img_encode_jpeg_ex(&img, "/sdcard/output.jpg",
                           &sd_writer, &opts))
    {
        ESP_LOGI(TAG, "Encoded successfully");
    }

    free(img.pixels);
}

/* ============================================================================
 *  EXAMPLE 6: Screenshot LVGL Screen to JPEG
 * ========================================================================== */

void example_screenshot(void)
{
    sd_ctx_t ctx = {0};
    sd_writer.user_ctx = &ctx;

    // Direct screenshot to file
    if (img_screenshot_lvgl_to_jpeg("/sdcard/screenshot.jpg",
                                    &sd_writer, 85))
    {
        ESP_LOGI(TAG, "Screenshot saved!");
    }
}

/* ============================================================================
 *  EXAMPLE 7: Screenshot to Buffer (for processing)
 * ========================================================================== */

void example_screenshot_buffer(void)
{
    img_rgb565_t screen;

    if (img_screenshot_lvgl(&screen))
    {
        ESP_LOGI(TAG, "Captured screen: %dx%d",
                 screen.width, screen.height);

        // Process the buffer (apply filters, etc)
        // ...

        // Now encode or use it
        sd_ctx_t ctx = {0};
        sd_writer.user_ctx = &ctx;
        img_encode_jpeg(&screen, "/sdcard/processed.jpg", &sd_writer);

        img_free(&screen);
    }
}

/* ============================================================================
 *  EXAMPLE 8: Image Viewer Pattern
 * ========================================================================== */

typedef struct
{
    img_rgb565_t current;
    lv_obj_t *img_obj;
    lv_img_dsc_t img_dsc;
} viewer_t;

static viewer_t viewer = {0};

void viewer_load_image(const char *path, bool fit_screen)
{
    // Free previous
    if (viewer.current.pixels)
    {
        img_free(&viewer.current);
    }

    sd_ctx_t ctx = {0};
    sd_reader.user_ctx = &ctx;

    img_decode_opts_t opts = {0};

    if (fit_screen)
    {
        // Get screen size
        lv_obj_t *scr = lv_scr_act();
        uint16_t scr_w = lv_obj_get_width(scr);
        uint16_t scr_h = lv_obj_get_height(scr);

        // Get image info
        uint16_t img_w, img_h;
        if (img_get_info(path, &sd_reader, &img_w, &img_h))
        {
            // Calculate scale to fit
            float scale_w = (float)scr_w / img_w;
            float scale_h = (float)scr_h / img_h;
            float scale = (scale_w < scale_h) ? scale_w : scale_h;

            if (scale < 1.0f)
            {
                // Need to downscale
                if (scale <= 0.125f)
                {
                    opts.scale = IMG_SCALE_1_8; // HW
                }
                else if (scale <= 0.25f)
                {
                    opts.scale = IMG_SCALE_1_4; // HW
                }
                else if (scale <= 0.5f)
                {
                    opts.scale = IMG_SCALE_1_2; // HW
                }
                else
                {
                    // Software resize
                    opts.resize = true;
                    opts.new_width = img_w * scale;
                    opts.new_height = img_h * scale;
                }
            }
        }
    }

    if (img_decode_rgb565_ex(path, IMG_FMT_AUTO, &sd_reader,
                             &opts, &viewer.current))
    {
        // Update LVGL image
        viewer.img_dsc.header.w = viewer.current.width;
        viewer.img_dsc.header.h = viewer.current.height;
        viewer.img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        viewer.img_dsc.data = viewer.current.pixels;
        viewer.img_dsc.data_size = viewer.current.size;

        if (!viewer.img_obj)
        {
            viewer.img_obj = lv_img_create(lv_scr_act());
        }

        lv_img_set_src(viewer.img_obj, &viewer.img_dsc);
        lv_obj_center(viewer.img_obj);

        ESP_LOGI(TAG, "Loaded: %s (%dx%d)", path,
                 viewer.current.width, viewer.current.height);
    }
}

/* ============================================================================
 *  EXAMPLE 9: Thumbnail Generator
 * ========================================================================== */

void example_generate_thumbnails(void)
{
    const char *images[] = {
        "/sdcard/img1.jpg",
        "/sdcard/img2.jpg",
        "/sdcard/img3.jpg",
    };

    sd_ctx_t read_ctx = {0};
    sd_ctx_t write_ctx = {0};
    sd_reader.user_ctx = &read_ctx;
    sd_writer.user_ctx = &write_ctx;

    img_decode_opts_t opts = {
        .scale = IMG_SCALE_1_4, // Generate 1/4 size thumbnails
    };

    for (int i = 0; i < 3; i++)
    {
        img_rgb565_t thumb;

        if (img_decode_rgb565_ex(images[i], IMG_FMT_JPEG,
                                 &sd_reader, &opts, &thumb))
        {

            char thumb_path[64];
            snprintf(thumb_path, sizeof(thumb_path),
                     "/sdcard/thumb_%d.jpg", i);

            img_encode_opts_t enc_opts = {.quality = 75};
            img_encode_jpeg_ex(&thumb, thumb_path, &sd_writer, &enc_opts);

            img_free(&thumb);

            ESP_LOGI(TAG, "Generated thumbnail: %s", thumb_path);
        }
    }
}

/* ============================================================================
 *  MAIN - Run Examples
 * ========================================================================== */

void app_main(void)
{
    // Initialize SD card, LVGL, etc...

    ESP_LOGI(TAG, "Running image codec examples...");

    // Run examples
    example_simple_decode();
    example_decode_with_transforms();
    example_hw_scale();
    example_get_info();
    example_encode();
    example_screenshot();
    example_generate_thumbnails();

    // Image viewer pattern
    viewer_load_image("/sdcard/photo.jpg", true);
}