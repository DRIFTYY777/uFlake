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

#include "gui_types.h"
#include "uInputs.h"
#include "uGui_theme.h"
#include "uGui_notification.h"

#include "lvgl.h"

#include "ST7789.h"

static const char *TAG = "uGUI";

// LVGL display and buffers
static uflake_mutex_t *gui_mutex = NULL;
static uint32_t lvgl_tick_timer_id = 0;
static lv_obj_t *content_container = NULL; // Container for app content (below notification)

// Forward declarations
static void lv_tick_timer_cb(void *arg);
static void gui_task(void *arg);

// LVGL tick timer callback
static void lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

// Event handler for button clicks
static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED)
    {
        // Get button label to identify which button was clicked
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        const char *text = lv_label_get_text(label);
        ESP_LOGI(TAG, "Button clicked: %s", text);

        // Toggle button color on click
        static uint8_t toggle = 0;
        toggle = !toggle;
        if (toggle)
        {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA00), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
        }
    }
    else if (code == LV_EVENT_FOCUSED)
    {
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        const char *text = lv_label_get_text(label);
        ESP_LOGI(TAG, "Button FOCUSED: %s", text);
        // Visual feedback when focused
        lv_obj_set_style_outline_width(btn, 4, LV_PART_MAIN);
        lv_obj_set_style_outline_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    }
    else if (code == LV_EVENT_DEFOCUSED)
    {
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        const char *text = lv_label_get_text(label);
        ESP_LOGI(TAG, "Button DEFOCUSED: %s", text);
        // Remove outline when not focused
        lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
    }
}

void lv_example_get_started_2(void)
{
    // Get the group for adding interactive objects
    lv_group_t *group = uGui_get_group();

    // Get the content container - apps create UI inside this
    lv_obj_t *container = uGui_get_content_container();

    // Button 1 - create inside content container (Y=0 is relative to container)
    lv_obj_t *btn1 = lv_button_create(container);
    lv_obj_set_pos(btn1, 10, 5);
    lv_obj_set_size(btn1, 140, 50);
    lv_obj_add_event_cb(btn1, btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *label1 = lv_label_create(btn1);
    lv_label_set_text(label1, "Button 1");
    lv_obj_center(label1);

    // Add to group for keyboard navigation
    lv_group_add_obj(group, btn1);

    // Button 2
    lv_obj_t *btn2 = lv_button_create(container);
    lv_obj_set_pos(btn2, 10, 60);
    lv_obj_set_size(btn2, 140, 50);
    lv_obj_add_event_cb(btn2, btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *label2 = lv_label_create(btn2);
    lv_label_set_text(label2, "Button 2");
    lv_obj_center(label2);

    lv_group_add_obj(group, btn2);

    // Button 3
    lv_obj_t *btn3 = lv_button_create(container);
    lv_obj_set_pos(btn3, 10, 115);
    lv_obj_set_size(btn3, 140, 50);
    lv_obj_add_event_cb(btn3, btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *label3 = lv_label_create(btn3);
    lv_label_set_text(label3, "Button 3");
    lv_obj_center(label3);

    lv_group_add_obj(group, btn3);

    // Focus on the first button
    lv_group_focus_obj(btn1);

    ESP_LOGI(TAG, "Created 3 interactive buttons with keyboard navigation");
}

void GUI_frontend()
{
    ESP_LOGI(TAG, "Initializing GUI frontend for multi-window support");

    /* Create keypad input device */
    kb_indev = lv_indev_create();

    /* Set input device type */
    lv_indev_set_type(kb_indev, LV_INDEV_TYPE_KEYPAD);

    /* Set read callback */
    lv_indev_set_read_cb(kb_indev, uInput);

    /* Create a group for interactive objects - shared across all windows */
    group_interact = lv_group_create();

    /* Attach the input device to the group */
    lv_indev_set_group(kb_indev, group_interact);

    /* Make it the default group for new objects */
    lv_group_set_default(group_interact);

    /* Enable wrap navigation - pressing up on first item goes to last */
    lv_group_set_wrap(group_interact, true);

    ESP_LOGI(TAG, "Input device=%p, group=%p initialized for multi-window management", kb_indev, group_interact);
}

void uGui_init(st7789_driver_t *drv)
{
    ESP_LOGI(TAG, "uGUI initialized");

    // Initialize LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // Allocate LVGL draw buffers using kernel memory manager
    // Buffer size must fit within DMA max transfer size (32KB)
    // 32 lines × 240 pixels × 2 bytes = 15360 bytes (safe margin under 32KB)
#define LVGL_BUF_LINES 32
    size_t buf_size = drv->display_width * LVGL_BUF_LINES;
    size_t buf_bytes = buf_size * sizeof(lv_color_t);

    ESP_LOGI(TAG, "Allocating LVGL buffers: %zu pixels (%zu bytes each)", buf_size, buf_bytes);

    // Allocate DMA-capable buffers (required for SPI DMA transfers)
    static lv_color_t *lv_buf1 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);
    static lv_color_t *lv_buf2 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_DMA);

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
        buf_size = drv->display_width * LVGL_BUF_LINES;
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
    static lv_display_t *lv_disp = lv_display_create(drv->display_width, drv->display_height);
    if (!lv_disp)
    {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        uflake_free(lv_buf1);
        uflake_free(lv_buf2);
        return;
    }

    // Configure LVGL display with double buffering
    lv_display_set_buffers(lv_disp, lv_buf1, lv_buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_set_user_data(lv_disp, drv);

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

    // Initialize input device and group for multi-window management
    GUI_frontend();
    ESP_LOGI(TAG, "GUI frontend initialized");

    // Initialize theme (sets screen background color/image)
    if (ugui_theme_init() != UFLAKE_OK)
    {
        ESP_LOGW(TAG, "Failed to initialize theme manager");
    }
    else
    {
        ESP_LOGI(TAG, "Theme manager initialized");
    }

    ugui_theme_set_bg_image_sdcard("/sd/car.jpeg");

    // Create content container for app UI (positioned below notification bar)
    // Apps create their UI inside this container - no overlapping with notification
    content_container = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(content_container, 0, UGUI_NOTIFICATION_HEIGHT);
    lv_obj_set_size(content_container, UGUI_DISPLAY_WIDTH, UGUI_APPWINDOW_HEIGHT);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0); // Transparent - shows theme bg through
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_radius(content_container, 0, 0);
    lv_obj_clear_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
    ESP_LOGI(TAG, "Content container created");

    // Initialize notification bar (at Y=0, no overlapping with content)
    if (ugui_notification_init() != UFLAKE_OK)
    {
        ESP_LOGW(TAG, "Failed to initialize notification bar");
    }
    else
    {
        ESP_LOGI(TAG, "Notification bar initialized");
        ugui_notification_show();
    }

    // Create example UI inside content container
    lv_example_get_started_2();

    // Create GUI task using kernel process manager
    uint32_t gui_pid;
    uflake_process_create("GUI_Task", gui_task, NULL, 1024 * 8, PROCESS_PRIORITY_HIGH, &gui_pid);
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

// Accessor functions for multi-window management
lv_indev_t *uGui_get_input_device(void)
{
    return kb_indev;
}

lv_group_t *uGui_get_group(void)
{
    return group_interact;
}

uflake_mutex_t *uGui_get_mutex(void)
{
    return gui_mutex;
}

lv_obj_t *uGui_get_content_container(void)
{
    return content_container;
}

// Clear all objects from the group (before switching windows)
void uGui_clear_group(void)
{
    if (group_interact != NULL)
    {
        lv_group_remove_all_objs(group_interact);
        ESP_LOGI(TAG, "Group cleared");
    }
}

// Reset group for a new window - clears and prepares for new focusable objects
void uGui_reset_group(void)
{
    uGui_clear_group();

    // Make sure group is still set as default
    if (group_interact != NULL)
    {
        lv_group_set_default(group_interact);
        ESP_LOGI(TAG, "Group reset and ready for new window");
    }
}