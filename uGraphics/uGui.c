#include "uGui.h"

#include "esp_log.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "kernel.h"
#include "timer_manager.h"
#include "synchronization.h"

#include "lvgl.h"

#include "imageCodec.h"
#include "sdCard.h"

// External image declaration from image.c
LV_IMAGE_DECLARE(ui_img_118050);

static const char *TAG = "uGUI";

// LVGL display and buffers
static lv_display_t *lv_disp = NULL;
static lv_color_t *lv_buf1 = NULL;
static lv_color_t *lv_buf2 = NULL;
static uflake_mutex_t *gui_mutex = NULL;
static uint32_t lvgl_tick_timer_id = 0;

st7789_driver_t *driver = NULL;

// Forward declarations
static void lv_tick_timer_cb(void *arg);
static void gui_task(void *arg);
static void create_lvgl_demo_ui(void);
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

// LVGL tick timer callback
static void lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

// Simple LVGL flush callback - matches original ST7789_write_pixels pattern
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    driver = (st7789_driver_t *)lv_display_get_user_data(disp);

    if (!driver || !px_map)
    {
        lv_display_flush_ready(disp);
        return;
    }

    // Set the window for the entire area ONCE (like original driver)
    ST7789_set_window(driver, area->x1, area->y1, area->x2, area->y2);

    // Wait for any previous transfers and set DC high for pixel data
    ST7789_queue_empty(driver);
    gpio_set_level(driver->pin_dc, 1);

    // Calculate total bytes to transfer
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);
    size_t total_bytes = width * height * 2; // RGB565 = 2 bytes per pixel

    // Use driver's buffer_size as max chunk (buffer_size is in pixels)
    // 240*20 = 4800 pixels = 9600 bytes - well under 32KB DMA limit
    size_t max_chunk = driver->buffer_size * 2;

    uint8_t *data_ptr = px_map;
    size_t remaining = total_bytes;

    while (remaining > 0)
    {
        size_t chunk_size = (remaining > max_chunk) ? max_chunk : remaining;

        // Simple SPI transaction like ST7789_write_pixels does
        spi_transaction_t trans;
        memset(&trans, 0, sizeof(trans));
        trans.tx_buffer = data_ptr;
        trans.length = chunk_size * 8; // bits
        trans.rxlength = 0;

        uspi_queue_trans(driver->spi, &trans, portMAX_DELAY);
        driver->queue_fill++;

        // Wait for transfer to complete
        spi_transaction_t *rtrans;
        uspi_get_trans_result(driver->spi, &rtrans, portMAX_DELAY);
        driver->queue_fill--;

        data_ptr += chunk_size;
        remaining -= chunk_size;
    }

    lv_display_flush_ready(disp);
}

void uGui_init(st7789_driver_t *drv)
{
    ESP_LOGI(TAG, "uGUI initialized");

    driver = drv;
    // Initialize LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

// Allocate LVGL draw buffers using kernel memory manager
// Buffer size must fit within DMA max transfer size (32KB)
// 32 lines × 240 pixels × 2 bytes = 15360 bytes (safe margin under 32KB)
#define LVGL_BUF_LINES 32
    size_t buf_size = driver->display_width * LVGL_BUF_LINES;
    size_t buf_bytes = buf_size * sizeof(lv_color_t);

    ESP_LOGI(TAG, "Allocating LVGL buffers: %zu pixels (%zu bytes each)", buf_size, buf_bytes);

    // Allocate DMA-capable buffers (required for SPI DMA transfers)
    lv_buf1 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);
    lv_buf2 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);

    if (!lv_buf1 || !lv_buf2)
    {
        // DMA allocation failed, try smaller buffers
        ESP_LOGW(TAG, "DMA allocation failed, trying smaller buffers");
        if (lv_buf1)
            uflake_free(lv_buf1);
        if (lv_buf2)
            uflake_free(lv_buf2);

#undef LVGL_BUF_LINES
#define LVGL_BUF_LINES 16 // Smaller fallback
        buf_size = driver->display_width * LVGL_BUF_LINES;
        buf_bytes = buf_size * sizeof(lv_color_t);

        lv_buf1 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);
        lv_buf2 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);
    }

    if (!lv_buf1 || !lv_buf2)
    {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        if (lv_buf1)
            uflake_free(lv_buf1);
        if (lv_buf2)
            uflake_free(lv_buf2);
        return;
    }

    ESP_LOGI(TAG, "LVGL buffers allocated: %zu bytes each", buf_bytes);

    // Create LVGL display
    lv_disp = lv_display_create(driver->display_width, driver->display_height);
    if (!lv_disp)
    {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        uflake_free(lv_buf1);
        uflake_free(lv_buf2);
        return;
    }

    // Configure LVGL display with double buffering
    lv_display_set_buffers(lv_disp, lv_buf1, lv_buf2,
                           buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_set_user_data(lv_disp, driver);

    // Use native RGB565 format - ST7789 is configured for little-endian via RAMCTRL (0x00, 0xC8)
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);

    ESP_LOGI(TAG, "LVGL display configured with double buffering");

    // Create mutex using kernel for LVGL thread safety
    if (uflake_mutex_create(&gui_mutex) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create GUI mutex");
        return;
    }

    ESP_LOGI(TAG, "GUI mutex created successfully");

    // Create kernel timer for LVGL ticks
    if (uflake_timer_create(&lvgl_tick_timer_id, LV_TICK_PERIOD_MS,
                            lv_tick_timer_cb, NULL, true) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create LVGL tick timer");
        return;
    }

    if (uflake_timer_start(lvgl_tick_timer_id) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to start LVGL tick timer");
        return;
    }

    ESP_LOGI(TAG, "LVGL tick timer started");

    // Create GUI task using kernel process manager
    uint32_t gui_pid;
    if (uflake_process_create("GUI_Task",
                              gui_task,
                              NULL,
                              1024 * 8,
                              PROCESS_PRIORITY_HIGH,
                              &gui_pid) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create GUI process");
        return;
    }

    ESP_LOGI(TAG, "GUI process created (PID: %lu)", gui_pid);
}

// Create LVGL demo UI
static void create_lvgl_demo_ui(void)
{
    // Lock mutex before creating UI elements
    if (uflake_mutex_lock(gui_mutex, UINT32_MAX) == UFLAKE_OK)
    {
        ESP_LOGI(TAG, "Creating LVGL demo UI...");

        // Create a title label
        lv_obj_t *title = lv_label_create(lv_screen_active());
        lv_label_set_text(title, "uFlake LVGL Demo");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);

        // Create a button
        lv_obj_t *btn = lv_button_create(lv_screen_active());
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, -30);
        lv_obj_set_size(btn, 150, 60);

        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, "Click Me!");
        lv_obj_center(btn_label);

        // Create a slider
        lv_obj_t *slider = lv_slider_create(lv_screen_active());
        lv_obj_align(slider, LV_ALIGN_CENTER, 0, 40);
        lv_obj_set_width(slider, 180);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);

        // Create colored rectangles
        lv_obj_t *rect = lv_obj_create(lv_screen_active());
        lv_obj_set_size(rect, 60, 60);
        lv_obj_align(rect, LV_ALIGN_BOTTOM_LEFT, 20, -20);
        lv_obj_set_style_bg_color(rect, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_border_width(rect, 0, 0);

        lv_obj_t *rect2 = lv_obj_create(lv_screen_active());
        lv_obj_set_size(rect2, 60, 60);
        lv_obj_align(rect2, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
        lv_obj_set_style_bg_color(rect2, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_border_width(rect2, 0, 0);

        ESP_LOGI(TAG, "LVGL demo UI created");

        uflake_mutex_unlock(gui_mutex);
    }
}
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
    lv_img_dsc_t *dsc = malloc(sizeof(lv_img_dsc_t));
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

void lv_example_blur_transparency(void)
{
    /* ================= BACKGROUND ================= */
    lv_obj_t *bg = lv_obj_create(lv_screen_active());
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    /* ================= IMAGE BEHIND GLASS ================= */
    lv_obj_t *img = lv_image_create(bg);
    lv_image_set_src(img, img_display_from_sd("car.jpeg"));
    lv_obj_center(img);

    /* ================= BLUR DIFFUSION LAYERS ================= */
    /* Outer diffusion */
    lv_obj_t *blur_outer = lv_obj_create(bg);
    lv_obj_set_size(blur_outer, 240, 160);
    lv_obj_center(blur_outer);
    lv_obj_set_style_radius(blur_outer, 26, 0);
    lv_obj_set_style_bg_color(blur_outer, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(blur_outer, LV_OPA_10, 0);
    lv_obj_set_style_border_width(blur_outer, 0, 0);
    lv_obj_remove_flag(blur_outer, LV_OBJ_FLAG_SCROLLABLE);

    /* Middle diffusion */
    lv_obj_t *blur_mid = lv_obj_create(bg);
    lv_obj_set_size(blur_mid, 230, 150);
    lv_obj_center(blur_mid);
    lv_obj_set_style_radius(blur_mid, 24, 0);
    lv_obj_set_style_bg_color(blur_mid, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(blur_mid, (lv_opa_t)((LV_OPA_COVER * 25) / 100), 0);
    lv_obj_set_style_border_width(blur_mid, 0, 0);
    lv_obj_remove_flag(blur_mid, LV_OBJ_FLAG_SCROLLABLE);

    /* ================= MAIN GLASS PANEL ================= */
    lv_obj_t *glass = lv_obj_create(bg);
    lv_obj_set_size(glass, 220, 140);
    lv_obj_center(glass);
    lv_obj_remove_flag(glass, LV_OBJ_FLAG_SCROLLABLE);

    /* Glass body */
    lv_obj_set_style_radius(glass, 20, 0);
    lv_obj_set_style_bg_color(glass, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(glass, (lv_opa_t)((LV_OPA_COVER * 25) / 100), 0);

    /* Gradient = fake blur */
    lv_obj_set_style_bg_grad_color(glass, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_grad_dir(glass, LV_GRAD_DIR_VER, 0);

    /* Frosted border */
    lv_obj_set_style_border_width(glass, 1, 0);
    lv_obj_set_style_border_color(glass, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(glass, LV_OPA_40, 0);

    /* Depth shadow */
    lv_obj_set_style_shadow_width(glass, 28, 0);
    lv_obj_set_style_shadow_color(glass, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(glass, LV_OPA_30, 0);
    lv_obj_set_style_shadow_offset_y(glass, 6, 0);

    /* ================= CONTENT ================= */
    lv_obj_t *label = lv_label_create(glass);
    lv_label_set_text(label, "LVGL 9 Glass");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
}

// GUI task - handles LVGL with semaphore protection
static void gui_task(void *arg)
{
    (void)arg;

    // Small delay to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(100));

    // Create the demo UI after LVGL task is running
    // create_lvgl_demo_ui();
    lv_example_blur_transparency();

    ESP_LOGI(TAG, "GUI task entering main loop");

    while (1)
    {
        // Watchdog is automatically fed by the kernel
        if (gui_mutex != NULL)
        {
            if (uflake_mutex_lock(gui_mutex, UINT32_MAX) == UFLAKE_OK)
            {
                lv_timer_handler();
                uflake_mutex_unlock(gui_mutex);
            }
        }
        uflake_process_yield(10); // Yields CPU and feeds watchdog
    }
}