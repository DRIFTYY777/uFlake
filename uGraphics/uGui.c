#include "uGui.h"

#include "esp_log.h"
#include <stdio.h>

#include "sdkconfig.h"

#include "kernel.h"

#include "lvgl.h"

#include "imageCodec.h"
#include "sdCard.h"
#include "../Apps/input_service/input.h"

static const char *TAG = "uGUI";

// LVGL display and buffers
static lv_display_t *lv_disp = NULL;
static lv_color_t *lv_buf1 = NULL;
static lv_color_t *lv_buf2 = NULL;
static uflake_mutex_t *gui_mutex = NULL;
static uint32_t lvgl_tick_timer_id = 0;

static lv_indev_t *keypad_indev;

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

static void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{

    InputKey key;

    input_get_key_event(&key);

    if (key == InputKeyUp)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyUp;
        ESP_LOGI(TAG, "Keypad event: UP");
    }
    else if (key == InputKeyDown)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyDown;
        ESP_LOGI(TAG, "Keypad event: DOWN");
    }
    else if (key == InputKeyRight)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyRight;
        ESP_LOGI(TAG, "Keypad event: RIGHT");
    }
    else if (key == InputKeyLeft)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyLeft;
        ESP_LOGI(TAG, "Keypad event: LEFT");
    }
    else if (key == InputKeyOk)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyOk;
        ESP_LOGI(TAG, "Keypad event: OK");
    }
    else if (key == InputKeyBack)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyBack;
        ESP_LOGI(TAG, "Keypad event: BACK");
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = InputKeyNone;
    }
}

void keypad_init(void)
{
    // Create LVGL input device for keypad
    keypad_indev = lv_indev_create();
    if (keypad_indev)
    {
        lv_indev_set_type(keypad_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(keypad_indev, keypad_read_cb);
        ESP_LOGI(TAG, "Keypad input device created");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create keypad input device");
    }
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

    keypad_init();

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

// GUI task - handles LVGL with semaphore protection
static void gui_task(void *arg)
{
    (void)arg;

    // Small delay to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(100));

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