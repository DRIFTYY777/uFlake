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

// App loader integration state
static uint32_t current_gui_app_id = 0;
static uGui_app_exit_cb_t app_exit_callback = NULL;
static void (*launcher_fn)(void) = NULL;

// Forward declarations
static void lv_tick_timer_cb(void *arg);
static void gui_task(void *arg);
static void global_key_event_cb(lv_event_t *e);

// Global key event handler - intercepts ESC key for app exit
static void global_key_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(e);

        if (key == LV_KEY_ESC)
        {
            // ESC pressed - exit current app if not on home screen
            if (!uGui_is_home_screen())
            {
                ESP_LOGI(TAG, "ESC pressed - exiting current app");
                uGui_exit_current_app();
                // Stop event propagation
                lv_event_stop_bubbling(e);
            }
        }
    }
}

// LVGL tick timer callback
static void lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
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

    // ugui_theme_set_bg_image_sdcard("/sd/car.jpeg");
    // ugui_theme_apply_dark();
    ugui_theme_apply_blue();

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

    // Add global key event handler to screen for ESC handling
    lv_obj_add_event_cb(lv_scr_act(), global_key_event_cb, LV_EVENT_KEY, NULL);
    ESP_LOGI(TAG, "Global key event handler registered");

    // Initialize notification bar (at Y=0, no overlapping with content)
    if (ugui_notification_init() != UFLAKE_OK)
    {
        ESP_LOGW(TAG, "Failed to initialize notification bar");
    }
    else
    {
        ugui_notification_show();
    }

    // NOTE: Don't create any UI here - the launcher app will handle that
    // The launcher gets started by app_loader after uGui_init

    // Create GUI task using kernel process manager
    uint32_t gui_pid;
    uflake_process_create("GUI_Task", gui_task, NULL, 1024 * 10, PROCESS_PRIORITY_HIGH, &gui_pid);
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
    }
}

// ============================================================================
// APP WINDOW MANAGEMENT
// ============================================================================

lv_obj_t *uGui_start_app(const char *app_name)
{
    // First clear any existing content and group safely
    uGui_clear_app_content();
    uGui_reset_group();

    // NOTE: App name notification is handled by uGui_launch_gui_app (via appLoader)
    // Don't call it here to avoid duplicates

    return content_container;
}

void uGui_clear_app_content(void)
{
    if (content_container != NULL)
    {
        // Safely delete all children of content container
        lv_obj_clean(content_container);
    }
}

void uGui_add_to_group(lv_obj_t *obj)
{
    if (obj != NULL && group_interact != NULL)
    {
        lv_group_add_obj(group_interact, obj);
    }
}

// ============================================================================
// APP LOADER INTEGRATION
// ============================================================================

void uGui_set_app_exit_callback(uGui_app_exit_cb_t callback)
{
    app_exit_callback = callback;
}

void uGui_set_launcher(void (*fn)(void))
{
    launcher_fn = fn;
}

void uGui_launch_gui_app(uint32_t app_id, const char *app_name, void (*entry_fn)(void))
{
    if (entry_fn == NULL)
    {
        ESP_LOGE(TAG, "Cannot launch app with NULL entry function");
        return;
    }

    // Store current app ID
    current_gui_app_id = app_id;

    // Clear content and group
    uGui_clear_app_content();
    uGui_reset_group();

    // Show app name in notification (handled by notification panel)
    if (app_name != NULL)
    {
        ugui_notification_show_app_name(app_name, 2000);
    }

    // Call the app's entry function - it sets up UI and returns
    // No separate task needed - LVGL handles everything
    entry_fn();
}

void uGui_exit_current_app(void)
{
    if (current_gui_app_id == 0)
    {
        ESP_LOGW(TAG, "No app to exit");
        return;
    }

    uint32_t exiting_app_id = current_gui_app_id;
    current_gui_app_id = 0;

    ESP_LOGI(TAG, "Exiting GUI app ID: %lu", exiting_app_id);

    // Clear content and group
    uGui_clear_app_content();
    uGui_reset_group();

    // Notify appLoader that app exited
    if (app_exit_callback != NULL)
    {
        app_exit_callback(exiting_app_id);
    }

    // Return to launcher/home screen
    if (launcher_fn != NULL)
    {
        ESP_LOGI(TAG, "Returning to launcher");
        launcher_fn();
    }
    else
    {
        ESP_LOGW(TAG, "No launcher function registered");
    }
}

uint32_t uGui_get_current_app_id(void)
{
    return current_gui_app_id;
}

bool uGui_is_home_screen(void)
{
    return (current_gui_app_id == 0);
}