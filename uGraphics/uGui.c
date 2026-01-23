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

#include "ST7789.h"
#include "lvgl.h"

static const char *TAG = "uGUI";

#define LV_TICK_PERIOD_MS 10

// LVGL display and buffers
static st7789_driver_t display;
static lv_display_t *lv_disp = NULL;
static lv_color_t *lv_buf1 = NULL;
static lv_color_t *lv_buf2 = NULL;
static uflake_mutex_t *gui_mutex = NULL;
static uint32_t lvgl_tick_timer_id = 0;

// Forward declarations
static void lv_tick_timer_cb(void *arg);
static void gui_task(void *arg);
static void create_lvgl_demo_ui(void);
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

void config_and_init_display()
{
    ESP_LOGI(TAG, "Configuring display...");

    // Configure display structure
    display.pin_cs = GPIO_NUM_10;
    display.pin_reset = GPIO_NUM_46;
    display.pin_dc = GPIO_NUM_14;

    display.display_width = 240;
    display.display_height = 320;
    display.orientation = 0;
    display.spi_host = USPI_HOST_SPI3;
    display.spi_speed = USPI_FREQ_40MHZ;
    display.buffer_size = 240 * 20; // 20 lines buffer

    // make gpio 3 output for backlight (In Future we have to use I2C to control backlight brightness)
    gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_3, 1); // turn on backlight

    // Initialize display
    if (!ST7789_init(&display))
    {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    ESP_LOGI(TAG, "Display initialized successfully");
}

// LVGL tick timer callback
static void lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

// LVGL flush callback - sends pixel data to display
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    st7789_driver_t *driver = (st7789_driver_t *)lv_display_get_user_data(disp);

    if (!driver || !px_map)
    {
        ESP_LOGE(TAG, "Invalid flush parameters");
        lv_display_flush_ready(disp);
        return;
    }

    // Calculate buffer size
    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);

    // Set drawing window
    ST7789_set_window(driver, area->x1, area->y1, area->x2, area->y2);

    // Use driver's buffer management
    driver->current_buffer = (st7789_color_t *)px_map;
    driver->buffer_size = size;

    // Send data using driver's swap buffers mechanism
    ST7789_swap_buffers(driver);

    // Inform LVGL that flushing is done
    lv_display_flush_ready(disp);
}

void uGui_init(void)
{
    ESP_LOGI(TAG, "uGUI initialized");

    config_and_init_display();

    // Initialize LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // Allocate LVGL draw buffers using kernel memory manager (DMA-capable)
    size_t buf_size = display.display_width * 30; // 30 lines buffer
    lv_buf1 = (lv_color_t *)uflake_malloc(buf_size * sizeof(lv_color_t), UFLAKE_MEM_DMA);
    lv_buf2 = (lv_color_t *)uflake_malloc(buf_size * sizeof(lv_color_t), UFLAKE_MEM_DMA);

    if (!lv_buf1 || !lv_buf2)
    {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        if (lv_buf1)
            uflake_free(lv_buf1);
        if (lv_buf2)
            uflake_free(lv_buf2);
        return;
    }

    ESP_LOGI(TAG, "LVGL buffers allocated: %zu bytes each", buf_size * sizeof(lv_color_t));

    // Create LVGL display
    lv_disp = lv_display_create(display.display_width, display.display_height);
    if (!lv_disp)
    {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        uflake_free(lv_buf1);
        uflake_free(lv_buf2);
        return;
    }

    // Configure LVGL display with double buffering
    lv_display_set_buffers(lv_disp, lv_buf1, lv_buf2,
                           buf_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_set_user_data(lv_disp, &display);

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

// GUI task - handles LVGL with semaphore protection
static void gui_task(void *arg)
{
    (void)arg;

    // Small delay to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(100));

    // Create the demo UI after LVGL task is running
    create_lvgl_demo_ui();

    ESP_LOGI(TAG, "GUI task entering main loop");

    while (1)
    {
        if (gui_mutex != NULL)
        {
            if (uflake_mutex_lock(gui_mutex, UINT32_MAX) == UFLAKE_OK)
            {
                lv_timer_handler();
                uflake_mutex_unlock(gui_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay for better task yielding
    }
}