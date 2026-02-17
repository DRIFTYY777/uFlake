#include "uGui.h"

#include "gui_input.h"

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "kernel.h"

#include "lvgl.h"

#include "imageCodec.h"
#include "sdCard.h"

static const char *TAG = "uGUI";

// Global initialization state
static bool g_ugui_initialized = false;

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
    const TickType_t spi_timeout = pdMS_TO_TICKS(500); // 500ms per transaction

    while (remaining > 0)
    {
        size_t chunk_size = (remaining > max_chunk) ? max_chunk : remaining;

        // Simple SPI transaction like ST7789_write_pixels does
        spi_transaction_t trans;
        memset(&trans, 0, sizeof(trans));
        trans.tx_buffer = data_ptr;
        trans.length = chunk_size * 8; // bits
        trans.rxlength = 0;

        // ✅ FIX: Use bounded timeout instead of portMAX_DELAY
        esp_err_t ret = uspi_queue_trans(driver->spi, &trans, spi_timeout);
        if (ret != ESP_OK)
        {
            UFLAKE_LOGE(TAG, "SPI queue failed: %d, aborting flush", ret);
            lv_display_flush_ready(disp);
            return;
        }
        driver->queue_fill++;

        // Wait for transfer to complete with timeout
        spi_transaction_t *rtrans;
        ret = uspi_get_trans_result(driver->spi, &rtrans, spi_timeout);
        if (ret != ESP_OK)
        {
            UFLAKE_LOGE(TAG, "SPI transfer failed: %d, aborting flush", ret);
            driver->queue_fill--;
            lv_display_flush_ready(disp);
            return;
        }
        driver->queue_fill--;

        data_ptr += chunk_size;
        remaining -= chunk_size;
    }

    lv_display_flush_ready(disp);
}

void uGui_init(st7789_driver_t *drv)
{
    if (g_ugui_initialized)
    {
        UFLAKE_LOGW(TAG, "uGUI already initialized");
        return;
    }

    UFLAKE_LOGI(TAG, "=== Initializing uGUI System ===");

    driver = drv;
    // Initialize LVGL
    lv_init();

    UFLAKE_LOGI(TAG, "LVGL initialized");

    // Allocate LVGL draw buffers using kernel memory manager
    // Buffer size must fit within DMA max transfer size (32KB)
    // 32 lines × 240 pixels × 2 bytes = 15360 bytes (safe margin under 32KB)
#define LVGL_BUF_LINES 32
    size_t buf_size = driver->display_width * LVGL_BUF_LINES;
    size_t buf_bytes = buf_size * sizeof(lv_color_t);

    UFLAKE_LOGI(TAG, "Allocating LVGL buffers: %zu pixels (%zu bytes each)", buf_size, buf_bytes);

    // Allocate DMA-capable buffers (required for SPI DMA transfers)
    lv_buf1 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);
    lv_buf2 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);

    if (!lv_buf1 || !lv_buf2)
    {
        // DMA allocation failed, try smaller buffers
        UFLAKE_LOGW(TAG, "DMA allocation failed, trying smaller buffers");
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
        UFLAKE_LOGE(TAG, "Failed to allocate LVGL buffers");
        if (lv_buf1)
            uflake_free(lv_buf1);
        if (lv_buf2)
            uflake_free(lv_buf2);
        return;
    }

    UFLAKE_LOGI(TAG, "LVGL buffers allocated: %zu bytes each", buf_bytes);

    // Create LVGL display with landscape dimensions (driver already configured as 320x240)
    lv_disp = lv_display_create(driver->display_width, driver->display_height);

    if (!lv_disp)
    {
        UFLAKE_LOGE(TAG, "Failed to create LVGL display");
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

    UFLAKE_LOGI(TAG, "LVGL display configured as 320x240 landscape with double buffering");

    // Create mutex using kernel for LVGL thread safety
    if (uflake_mutex_create(&gui_mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create GUI mutex");
        return;
    }

    UFLAKE_LOGI(TAG, "GUI mutex created successfully");

    // Create kernel timer for LVGL ticks
    if (uflake_timer_create(&lvgl_tick_timer_id, LV_TICK_PERIOD_MS,
                            lv_tick_timer_cb, NULL, true) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create LVGL tick timer");
        return;
    }

    if (uflake_timer_start(lvgl_tick_timer_id) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to start LVGL tick timer");
        return;
    }

    UFLAKE_LOGI(TAG, "LVGL tick timer started");

    keypad_init();

    // Create GUI task using kernel process manager
    uint32_t gui_pid;
    if (uflake_process_create("GUI_Task",
                              gui_task,
                              NULL,
                              1024 * 8,
                              PROCESS_PRIORITY_NORMAL, // Lower than kernel priority
                              &gui_pid) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create GUI task");
        return;
    }

    UFLAKE_LOGI(TAG, "GUI task created (PID: %lu)", gui_pid);

    // ========================================================================
    // Initialize uGUI subsystems (NEW ARCHITECTURE)
    // ========================================================================

    UFLAKE_LOGI(TAG, "Initializing uGUI subsystems...");

    // 1. Focus Manager - CRITICAL for crash-free operation
    if (ugui_focus_init() != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize focus manager");
        return;
    }
    UFLAKE_LOGI(TAG, "✓ Focus manager initialized");

    // 2. Theme Manager - Background and colors
    if (ugui_theme_init() != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize theme manager");
        return;
    }
    UFLAKE_LOGI(TAG, "✓ Theme manager initialized");

    // 3. Notification Bar - System status display
    if (ugui_notification_init() != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize notification bar");
        return;
    }
    UFLAKE_LOGI(TAG, "✓ Notification bar initialized");

    // 4. App Window Manager - Safe app containers
    if (ugui_appwindow_init() != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize app window manager");
        return;
    }
    UFLAKE_LOGI(TAG, "✓ App window manager initialized");

    // 5. Navigation System - Keyboard input routing
    if (ugui_navigation_init() != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize navigation system");
        return;
    }
    UFLAKE_LOGI(TAG, "✓ Navigation system initialized");

    // Apply default theme (Flipper-like blue theme)
    ugui_theme_apply_blue();
    UFLAKE_LOGI(TAG, "✓ Default theme applied");

    // Try to load background image from SD card (car.jpeg)
    if (ugui_theme_set_bg_image_sdcard("/sd/car.jpeg") != UFLAKE_OK)
    {
        UFLAKE_LOGI(TAG, "Using fallback color background");
        // Fallback already handled in theme function
    }

    // Set initial system status
    ugui_system_status_t status = {
        .battery_percent = 100,
        .charging = false,
        .wifi_connected = false,
        .bt_connected = false,
        .sdcard_mounted = true, // Assume mounted for now
        .hour = 12,
        .minute = 0};
    ugui_notification_update_status(&status);

    g_ugui_initialized = true;

    UFLAKE_LOGI(TAG, "=== uGUI System Ready ===");
    UFLAKE_LOGI(TAG, "Apps can now use ugui_appwindow_create() for safe UI creation");
}

// ============================================================================
// PUBLIC API
// ============================================================================

uflake_mutex_t *uGui_get_mutex(void)
{
    return gui_mutex;
}

bool uGui_is_initialized(void)
{
    return g_ugui_initialized;
}

// GUI task - handles LVGL with semaphore protection
static void gui_task(void *arg)
{
    (void)arg;

    // Small delay to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1)
    {
        uint32_t sleep_time = LV_DEF_REFR_PERIOD; // Default 30ms if mutex fails

        if (gui_mutex != NULL)
        {
            // Use timeout instead of infinite wait to prevent blocking kernel
            if (uflake_mutex_lock(gui_mutex, 50) == UFLAKE_OK)
            {
                // lv_timer_handler returns time in MS until next timer expires
                // This allows DYNAMIC timing based on actual LVGL workload:
                // - No animations: returns 50-100ms (GUI sleeps longer)
                // - Active animations: returns 5-16ms (fast refresh)
                // - This PREVENTS flooding by adapting to actual needs
                sleep_time = lv_timer_handler();
                uflake_mutex_unlock(gui_mutex);

                // Safety bounds: never sleep less than 5ms or more than 100ms
                if (sleep_time < 5)
                    sleep_time = 5;
                if (sleep_time > 100)
                    sleep_time = 100;
            }
        }

        // Yield with DYNAMIC timing based on LVGL's actual needs
        uflake_process_yield(sleep_time);
    }
}