#include "uGUI.h"

#include <stdio.h>
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "ST7789.h"

const char *TAG = "uGUI";

static lv_display_t *lv_disp = nullptr;
static lv_color_t *lv_buf1 = nullptr;
static lv_color_t *lv_buf2 = nullptr;
static uint32_t lvgl_tick_timer_id = 0;
static uflake_mutex_t *gui_mutex = nullptr;

static uint32_t watchdog_id; // Add a member to store the watchdog ID

void uGUI::lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

void uGUI::gui_task(void *arg)
{
    while (true)
    {
        if (gui_mutex != nullptr)
        {
            if (uflake_mutex_lock(gui_mutex, portMAX_DELAY))
            {

                uflake_mutex_lock(gui_mutex, portMAX_DELAY);
                lv_task_handler();
                uflake_mutex_unlock(gui_mutex);
            }
        }
        uflake_watchdog_feed_by_id(watchdog_id); // Replace with actual watchdog ID if used
        vTaskDelay(pdMS_TO_TICKS(10));           // Sleep for 10ms to reduce CPU usage
    }
}

void uGUI::initialize(uint16_t display_width, uint16_t display_height)
{
    UFLAKE_LOGI(TAG, "Initializing uGUI...");

    // Initialize LVGL first
    lv_init();
    UFLAKE_LOGI(TAG, "LVGL initialized");

    // 10240  32
    // 5120  16
    // 7200  microByte Arduino

    // Allocate LVGL draw buffers using kernel memory manager
    // Buffer size must fit within DMA max transfer size (32KB)
    // 32 lines × display_width × 2 bytes per pixel
#define LVGL_BUF_LINES 16
    size_t buf_size = display_width * LVGL_BUF_LINES;
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
#define LVGL_BUF_LINES 8 // Smaller fallback
        buf_size = display_width * LVGL_BUF_LINES;
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

    // Create LVGL display
    lv_disp = lv_display_create(display_width, display_height);
    if (!lv_disp)
    {
        UFLAKE_LOGE(TAG, "Failed to create LVGL display");
        uflake_free(lv_buf1);
        uflake_free(lv_buf2);
        return;
    }

    // Configure LVGL display with double buffering
    lv_display_set_buffers(lv_disp, lv_buf1, lv_buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_set_user_data(lv_disp, this);
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);

    UFLAKE_LOGI(TAG, "LVGL display configured with double buffering");

    // Create mutex using kernel for LVGL thread safety
    if (uflake_mutex_create(&gui_mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create GUI mutex");
        return;
    }

    UFLAKE_LOGI(TAG, "GUI mutex created successfully");

    // Create kernel timer for LVGL ticks
    if (uflake_timer_create(&lvgl_tick_timer_id, LV_TICK_PERIOD_MS, lv_tick_timer_cb, NULL, true) != UFLAKE_OK)
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

    // Create the GUI task
    uint32_t gui_pid;
    if (uflake_process_create("uGUI Task", [](void *arg)
                              { gui_task(arg); }, NULL, 1024 * 8, PROCESS_PRIORITY_NORMAL, &gui_pid) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create GUI process");
        return;
    }

    UFLAKE_LOGI(TAG, "GUI process created (PID: %lu)", gui_pid);
}