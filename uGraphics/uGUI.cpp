#include "uGui.h"

#include "esp_log.h"
#include <stdio.h>
#include <string.h>
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
#include "appLoader.h"

#include "lvgl.h"

#include "ST7789.h"

static const char *TAG = "uGUI";

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================
#define GUI_MUTEX_LOCK_TIMEOUT_MS 100 // Prevent deadlock (instead of UINT32_MAX)

// ============================================================================
// GLOBAL VARIABLES - Input device and focus group (declared in gui_types.h as extern)
// ============================================================================
lv_indev_t *kb_indev = NULL;       // Global keypad input device
lv_group_t *group_interact = NULL; // Global focus group for navigation

// LVGL display and buffers
static uflake_mutex_t *gui_mutex = NULL;
static uint32_t lvgl_tick_timer_id = 0;
static lv_obj_t *content_container = NULL;    // Container for app content (below notification)
static lv_obj_t *screen_handle = NULL;        // Handle to screen for cleanup
static bool gui_initialized = false;          // Track initialization state
static bool gui_screen_handler_added = false; // Track if event handler added (fix #9)

// App loader integration state (PROTECTED BY gui_mutex - fix #1)
static uint32_t current_gui_app_id = 0;
static uGui_app_exit_cb_t app_exit_callback = NULL;
static void (*launcher_fn)(void) = NULL;
static bool launcher_fn_valid = false; // Validation for launcher (fix #10)

// Deferred app launch - context passed to async handler
typedef struct
{
    app_entry_fn entry_fn;
    uint32_t app_id;
    char app_name[64];
    void *ctx_ref; // Self-reference for cleanup (fix #2,#3)
} gui_app_context_t;

// Track active contexts to prevent UAF (fix #3)
static gui_app_context_t *active_app_ctx = NULL;

// Forward declarations
static void lv_tick_timer_cb(void *arg);
static void gui_task(void *arg);
static void global_key_event_cb(lv_event_t *e);

// Global key event handler - intercepts ESC key for app exit
static void global_key_event_cb(lv_event_t *e)
{
    // Validate handler still registered (fix #9)
    if (!gui_screen_handler_added)
        return;

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

// ============================================================================
// ASYNC APP LAUNCH - Safe callback for LVGL async execution
// ============================================================================

static void gui_app_async_launch(void *user_data)
{
    if (user_data == NULL)
    {
        ESP_LOGE(TAG, "Invalid async app launch parameter");
        return;
    }

    gui_app_context_t *ctx = (gui_app_context_t *)user_data;

    if (ctx->entry_fn == NULL)
    {
        ESP_LOGE(TAG, "Invalid app entry function");
        uflake_free(ctx);
        return;
    }

    // Validate GUI still initialized (fix #4)
    if (!gui_initialized)
    {
        ESP_LOGE(TAG, "GUI not initialized during app launch");
        uflake_free(ctx);
        return;
    }

    // SAFE: Don't free context until after app runs (fix #3)
    active_app_ctx = ctx; // Track active context

    // Now we're in the LVGL event loop context
    // Only clear once, not twice (fix #8 - removed duplicate uGui_clear_app_content in gui_app_async_launch)
    if (uflake_mutex_lock(gui_mutex, GUI_MUTEX_LOCK_TIMEOUT_MS) == UFLAKE_OK)
    {
        current_gui_app_id = ctx->app_id; // Protected write
        uflake_mutex_unlock(gui_mutex);
    }

    // Clear content and group (once only)
    uGui_clear_app_content();
    uGui_reset_group();

    // Show app name in notification
    if (ctx->app_name[0] != '\0')
    {
        ugui_notification_show_app_name(ctx->app_name, 2000);
    }

    ESP_LOGI(TAG, "Executing app entry for %s (ID: %lu) in LVGL event loop",
             ctx->app_name, ctx->app_id);

    // Execute app entry function
    app_entry_fn entry = ctx->entry_fn;
    entry(); // Call app main - context stays valid

    // NOTE: Context freed by uGui_exit_current_app when app ends, not here (fix #3)
}

void GUI_frontend()
{
    ESP_LOGI(TAG, "Initializing GUI frontend for multi-window support");

    /* Create keypad input device */
    kb_indev = lv_indev_create();
    if (kb_indev == NULL)
    {
        ESP_LOGE(TAG, "Failed to create input device");
        return;
    }
    ESP_LOGI(TAG, "Input device created: %p", (void *)kb_indev);

    /* Set input device type */
    lv_indev_set_type(kb_indev, LV_INDEV_TYPE_KEYPAD);

    /* Set read callback */
    lv_indev_set_read_cb(kb_indev, uInput);

    /* Create a group for interactive objects - shared across all windows */
    group_interact = lv_group_create();
    if (group_interact == NULL)
    {
        ESP_LOGE(TAG, "Failed to create group");
        return;
    }
    ESP_LOGI(TAG, "Group created: %p", (void *)group_interact);

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

    // Allocate LVGL draw buffers using kernel memory manager (fix #5)
    // Buffer size must fit within DMA max transfer size (32KB)
    // Start with 16 lines (7680 bytes) to reduce memory pressure
    // 16 lines × 240 pixels × 2 bytes = 7680 bytes (efficient balance)
    size_t buf_bytes;
    static lv_color_t *lv_buf1 = NULL;
    static lv_color_t *lv_buf2 = NULL;
    size_t buf_size;

    // Try 16-line buffers first (lower memory pressure)
    buf_size = drv->display_width * 16;
    buf_bytes = buf_size * sizeof(lv_color_t);

    ESP_LOGI(TAG, "Allocating LVGL buffers (16 lines): %zu pixels (%zu bytes each)", buf_size, buf_bytes);

    lv_buf1 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_INTERNAL);
    lv_buf2 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_INTERNAL);

    // Validate BOTH allocations succeeded before proceeding (fix #5)
    if (!lv_buf1 || !lv_buf2)
    {
        // If 16-line buffers fail, free both and try 8-line buffers
        ESP_LOGW(TAG, "16-line buffer allocation failed, trying 8-line buffers");
        uflake_free(lv_buf1); // Safe even if NULL
        uflake_free(lv_buf2); // Safe even if NULL
        lv_buf1 = NULL;
        lv_buf2 = NULL;

        buf_size = drv->display_width * 8;
        buf_bytes = buf_size * sizeof(lv_color_t);

        lv_buf1 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_INTERNAL);
        lv_buf2 = (lv_color_t *)uflake_malloc(buf_bytes, UFLAKE_MEM_INTERNAL);
    }

    // Final validation - both must succeed (fix #5)
    if (!lv_buf1 || !lv_buf2)
    {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers (8-line fallback also failed)");
        uflake_free(lv_buf1);
        uflake_free(lv_buf2);
        return;
    }

    ESP_LOGI(TAG, "LVGL buffers allocated: %zu bytes each (LVGL+ST7789 total ~40KB RAM)", buf_bytes);

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
        // Only load SD card image if theme system ready (fix #6)
        if (ugui_theme_set_bg_image_sdcard("/sd/car.jpeg") != UFLAKE_OK)
        {
            ESP_LOGW(TAG, "Failed to load background image from SD card");
        }
    }
    // ugui_theme_apply_dark();
    // ugui_theme_apply_blue();

    // Create content container for app UI (positioned below notification bar)
    // Apps create their UI inside this container - no overlapping with notification
    content_container = lv_obj_create(lv_scr_act());
    if (content_container == NULL)
    {
        ESP_LOGE(TAG, "Failed to create content container");
        return;
    }

    screen_handle = lv_scr_act(); // Store for cleanup (fix #7)
    lv_obj_set_pos(content_container, 0, UGUI_NOTIFICATION_HEIGHT);
    lv_obj_set_size(content_container, UGUI_DISPLAY_WIDTH, UGUI_APPWINDOW_HEIGHT);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0); // Transparent - shows theme bg through
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_radius(content_container, 0, 0);
    lv_obj_clear_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
    ESP_LOGI(TAG, "Content container created");

    // Add global key event handler to screen for ESC handling (fix #9 - track with flag)
    lv_obj_add_event_cb(lv_scr_act(), global_key_event_cb, LV_EVENT_KEY, NULL);
    gui_screen_handler_added = true;
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

    // Mark GUI as initialized (fix #1, #4)
    gui_initialized = true;
    ESP_LOGI(TAG, "GUI fully initialized and ready");

    // NOTE: Don't create any UI here - the launcher app will handle that
    // The launcher gets started by app_loader after uGui_init

    // Create GUI task using kernel process manager
    uint32_t gui_pid;
    uflake_process_create("GUI_Task", gui_task, NULL, 1024 * 12, PROCESS_PRIORITY_HIGH, &gui_pid);
}

// GUI task - handles LVGL with dynamic event-driven timing
static void gui_task(void *arg)
{
    (void)arg;

    // Small delay to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "GUI task entering main loop with dynamic LVGL timing");

    // Health monitoring variables (diagnose performance issues)
    uint32_t iteration_count = 0;
    uint32_t mutex_timeout_count = 0;
    uint32_t total_sleep_time = 0;
    uint32_t max_handler_time = 0;

    while (1)
    {
        if (gui_mutex != NULL)
        {
            if (uflake_mutex_lock(gui_mutex, GUI_MUTEX_LOCK_TIMEOUT_MS) == UFLAKE_OK)
            {
                uflake_mutex_unlock(gui_mutex);
            }
        }

        // Diagnostic logging every 100 iterations (~1-3 seconds depending on load)
        iteration_count++;
        total_sleep_time += sleep_time;

        if (iteration_count % 100 == 0)
        {
            uint32_t avg_sleep = total_sleep_time / 100;
            uint32_t calls_per_sec = 1000 / avg_sleep;

            ESP_LOGI(TAG,
                     "GUI Health: iter=%lu, avg_sleep=%lums (%lu calls/sec), "
                     "timeouts=%lu, max_handler=%lums",
                     iteration_count, avg_sleep, calls_per_sec,
                     mutex_timeout_count, max_handler_time);

            // Reset counters for next interval
            total_sleep_time = 0;
            mutex_timeout_count = 0;
            max_handler_time = 0;
        }

        // ✅ DYNAMIC sleep based on LVGL's actual needs
        // This replaces the fixed GUI_TASK_YIELD_MS approach
        uflake_process_yield(sleep_time);
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

// ============================================================================
// AUTO-FOCUS HELPER - Ensure first focusable object gets focus
// ============================================================================

/**
 * @brief Auto-focus helper - deferred execution to focus first object
 *
 * This is scheduled via lv_async_call() to run after UI is fully created.
 * By that time, the group is stable and focus operations work reliably.
 */
static void auto_focus_first_focusable(void *user_data)
{
    lv_obj_t *first_obj = (lv_obj_t *)user_data;
    if (first_obj == NULL)
        return;

    lv_group_t *group = uGui_get_group();
    if (group == NULL)
    {
        ESP_LOGW(TAG, "Auto-focus: no group found");
        return;
    }

    // Focus the provided object (passed from launcher)
    lv_group_focus_obj(first_obj);
    lv_obj_t *focused = lv_group_get_focused(group);

    if (focused == first_obj)
    {
        ESP_LOGI(TAG, "Auto-focus: Successfully focused first object: %p", (void *)first_obj);
    }
    else
    {
        ESP_LOGW(TAG, "Auto-focus: Failed to focus first object (got %p instead)", (void *)focused);
    }
}

/**
 * @brief Request auto-focus of a specific object
 *
 * Call this after creating all UI elements. The object will be focused
 * on the next LVGL cycle, ensuring proper initialization.
 *
 * @param obj The object to focus (usually the first button)
 */
void uGui_auto_focus_object(lv_obj_t *obj)
{
    if (obj == NULL)
    {
        ESP_LOGW(TAG, "Auto-focus: NULL object");
        return;
    }
    // Defer focus to next LVGL cycle for stability
    lv_async_call(auto_focus_first_focusable, obj);
}

// Clear all objects from the group (before switching windows)
void uGui_clear_group(void)
{
    if (group_interact != NULL)
    {
        // Simply remove all objects from group
        // LVGL handles clearing internal focus state automatically
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
        ESP_LOGI(TAG, "Added object %p to group %p", (void *)obj, (void *)group_interact);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to add to group - obj=%p, group=%p", (void *)obj, (void *)group_interact);
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
    // Validate launcher before storing (fix #10)
    if (fn != NULL)
    {
        launcher_fn = fn;
        launcher_fn_valid = true;
        ESP_LOGI(TAG, "Launcher function registered");
    }
    else
    {
        launcher_fn = NULL;
        launcher_fn_valid = false;
        ESP_LOGW(TAG, "NULL launcher function provided");
    }
}

void uGui_launch_gui_app(uint32_t app_id, const char *app_name, void (*entry_fn)(void))
{
    // Validate GUI system (fix #4)
    if (!gui_initialized)
    {
        ESP_LOGE(TAG, "Cannot launch app - GUI not initialized");
        return;
    }

    if (entry_fn == NULL)
    {
        ESP_LOGE(TAG, "Cannot launch app with NULL entry function");
        return;
    }

    // Allocate context for async launch
    gui_app_context_t *ctx = (gui_app_context_t *)uflake_malloc(sizeof(gui_app_context_t), UFLAKE_MEM_INTERNAL);
    if (ctx == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate context for app launch");
        return;
    }

    // Fill context (fix #4,#3)
    ctx->entry_fn = entry_fn;
    ctx->app_id = app_id;
    ctx->ctx_ref = ctx; // Self-reference for validation

    if (app_name != NULL)
    {
        // Safe copy with length validation (fix perf issue - don't copy full buffer) (fix #4)
        size_t name_len = strlen(app_name);
        if (name_len >= sizeof(ctx->app_name))
        {
            ESP_LOGW(TAG, "App name too long (%zu), truncating to %zu", name_len, sizeof(ctx->app_name) - 1);
        }
        strncpy(ctx->app_name, app_name, sizeof(ctx->app_name) - 1);
        ctx->app_name[sizeof(ctx->app_name) - 1] = '\0';
    }
    else
    {
        ctx->app_name[0] = '\0';
    }

    ESP_LOGI(TAG, "Scheduling app %s for launch via LVGL async call (ID: %lu)",
             app_name ? app_name : "unknown", app_id);

    // Schedule the async call - LVGL will call gui_app_async_launch in the LVGL task context
    lv_async_call(gui_app_async_launch, ctx);
}

void uGui_exit_current_app(void)
{
    // Protected access to global state (fix #1)
    uint32_t exiting_app_id = 0;

    if (uflake_mutex_lock(gui_mutex, GUI_MUTEX_LOCK_TIMEOUT_MS) == UFLAKE_OK)
    {
        exiting_app_id = current_gui_app_id;
        current_gui_app_id = 0;
        uflake_mutex_unlock(gui_mutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to acquire mutex for app exit");
        return;
    }

    if (exiting_app_id == 0)
    {
        ESP_LOGW(TAG, "No app to exit");
        return;
    }

    ESP_LOGI(TAG, "Exiting GUI app ID: %lu", exiting_app_id);

    // Clear active context (fix #3)
    if (active_app_ctx != NULL)
    {
        uflake_free(active_app_ctx);
        active_app_ctx = NULL;
    }

    // Clear content and group (only once, here)
    uGui_clear_app_content();
    uGui_reset_group();

    // Notify appLoader that app exited
    if (app_exit_callback != NULL)
    {
        app_exit_callback(exiting_app_id);
    }

    // Return to launcher/home screen (fix #10 - validate launcher)
    if (launcher_fn_valid && launcher_fn != NULL)
    {
        ESP_LOGI(TAG, "Returning to launcher");
        launcher_fn();
    }
    else
    {
        ESP_LOGW(TAG, "No launcher function registered or invalid");
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