/**
 * @file uGui_simple.cpp
 * @brief Simplified GUI System Implementation
 *
 * Core GUI system with minimal complexity:
 * - Direct LVGL wrapper
 * - Simple input routing
 * - App lifecycle management
 * - Memory tracking
 */

#include "uGui_simple.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "kernel.h"
#include <stdio.h>
#include <string.h>

#include "ST7789.h"

static const char *TAG = "uGUI_SIMPLE";

// ============================================================================
// GLOBAL STATE
// ============================================================================

static ugui_state_t gui_state = {
    .state = UGUI_APP_STATE_HOME,
    .mem_pressure = UGUI_MEM_NORMAL,
    .free_ram = 0,
    .current_app_id = 0,
    .notification_enabled = true,
    .screen = NULL,
    .content_container = NULL};

static bool gui_initialized = false;
static uflake_mutex_t *gui_mutex = NULL;

// Input device
static lv_indev_t *input_device = NULL;

// Input handlers (one per button)
static ugui_input_callback_t input_handlers[UGUI_BTN_COUNT] = {NULL};
static void *input_handler_userdata[UGUI_BTN_COUNT] = {NULL};

// App exit callback
static ugui_app_exit_callback_t app_exit_callback = NULL;
static void *app_exit_userdata = NULL;

// Memory pressure callback
static ugui_memory_pressure_cb_t memory_pressure_callback = NULL;
static void *memory_pressure_userdata = NULL;

// Theme (minimal default)
static ugui_theme_t current_theme = {
    .bg_primary = LV_COLOR_MAKE(0x11, 0x11, 0x11),      // Dark gray
    .bg_secondary = LV_COLOR_MAKE(0x22, 0x22, 0x22),    // Lighter gray
    .text_primary = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),    // White
    .text_secondary = LV_COLOR_MAKE(0xAA, 0xAA, 0xAA),  // Light gray
    .accent = LV_COLOR_MAKE(0x00, 0xAA, 0xFF),          // Cyan
    .notification_bg = LV_COLOR_MAKE(0x00, 0x00, 0x00), // Black
    .notification_fg = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF)  // White
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void gui_task_internal(void *arg);
static void input_read_callback(lv_indev_t *indev, lv_indev_data_t *data);
static void update_memory_stats(void);
static void check_memory_pressure(void);

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t uGui_init(void)
{
    if (gui_initialized)
    {
        ESP_LOGW(TAG, "GUI already initialized");
        return UFLAKE_OK;
    }

    // Create mutex
    if (uflake_mutex_create(&gui_mutex) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create GUI mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    // Initialize LVGL (must be done before any LVGL calls)
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // Link display drivers with lvgl lvgl_flush_cb

    // Create LVGL display (ST7789 or other driver configured elsewhere)
    // TODO: This should be configured by display driver
    // For now, assume display is already created and set as default
    lv_display_t *disp = lv_display_get_default();

    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    if (disp == NULL)
    {
        ESP_LOGE(TAG, "No LVGL display configured. Display driver must initialize LVGL display first");
        return UFLAKE_ERROR;
    }
    ESP_LOGI(TAG, "LVGL display found: %p", (void *)disp);

    // Get screen (created automatically by LVGL)
    gui_state.screen = lv_scr_act();
    if (gui_state.screen == NULL)
    {
        ESP_LOGE(TAG, "Failed to get active screen");
        return UFLAKE_ERROR;
    }
    ESP_LOGI(TAG, "Screen object: %p", (void *)gui_state.screen);

    // Create content container (below notification bar if used)
    gui_state.content_container = lv_obj_create(gui_state.screen);
    if (gui_state.content_container == NULL)
    {
        ESP_LOGE(TAG, "Failed to create content container");
        return UFLAKE_ERROR_MEMORY;
    }

    // Setup content container
    lv_obj_set_pos(gui_state.content_container, 0, UGUI_NOTIFICATION_HEIGHT);
    lv_obj_set_size(gui_state.content_container, UGUI_DISPLAY_WIDTH, UGUI_CONTENT_HEIGHT);
    lv_obj_set_style_bg_color(gui_state.content_container, current_theme.bg_primary, 0);
    lv_obj_set_style_bg_opa(gui_state.content_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(gui_state.content_container, 0, 0);
    lv_obj_set_style_pad_all(gui_state.content_container, 0, 0);
    lv_obj_clear_flag(gui_state.content_container, LV_OBJ_FLAG_SCROLLABLE);
    ESP_LOGI(TAG, "Content container created: %p", (void *)gui_state.content_container);

    // Create input device (keypad)
    input_device = lv_indev_create();
    if (input_device == NULL)
    {
        ESP_LOGE(TAG, "Failed to create input device");
        return UFLAKE_ERROR_MEMORY;
    }
    lv_indev_set_type(input_device, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(input_device, input_read_callback);
    ESP_LOGI(TAG, "Input device created: %p", (void *)input_device);

    // Initialize state
    gui_state.state = UGUI_APP_STATE_HOME;
    gui_state.current_app_id = 0;

    // Create GUI task
    uint32_t gui_pid;
    if (uflake_process_create("GUI_Task", gui_task_internal, NULL, 4096, PROCESS_PRIORITY_HIGH, &gui_pid) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create GUI task");
        return UFLAKE_ERROR;
    }
    ESP_LOGI(TAG, "GUI task created (PID: %lu)", gui_pid);

    gui_initialized = true;
    ESP_LOGI(TAG, "GUI system initialized successfully");

    return UFLAKE_OK;
}

void uGui_task(void)
{
    if (!gui_initialized)
        return;

    if (uflake_mutex_lock(gui_mutex, 10) == UFLAKE_OK)
    {
        // Update memory statistics
        update_memory_stats();
        check_memory_pressure();

        // Handle LVGL
        uint32_t sleep_time = lv_timer_handler();

        uflake_mutex_unlock(gui_mutex);

        // Sleep for the optimal time (returns milliseconds until next timer)
        if (sleep_time > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(sleep_time > 50 ? 50 : sleep_time));
        }
    }
}

// ============================================================================
// INTERNAL TASK
// ============================================================================

static void gui_task_internal(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "GUI task started");

    // Main loop - process GUI events
    while (1)
    {
        uGui_task();
        vTaskDelay(pdMS_TO_TICKS(10)); // Minimum delay
    }
}

// ============================================================================
// STATE GETTERS
// ============================================================================

ugui_state_t *uGui_get_state(void)
{
    return &gui_state;
}

ugui_theme_t *uGui_get_theme(void)
{
    return &current_theme;
}

lv_obj_t *uGui_get_content_container(void)
{
    return gui_state.content_container;
}

lv_display_t *uGui_get_display(void)
{
    return lv_display_get_default();
}

lv_obj_t *uGui_get_screen(void)
{
    return gui_state.screen;
}

// ============================================================================
// APP LIFECYCLE
// ============================================================================

lv_obj_t *uGui_start_app(uint32_t app_id)
{
    if (!gui_initialized || gui_state.content_container == NULL)
        return NULL;

    if (uflake_mutex_lock(gui_mutex, 100) != UFLAKE_OK)
        return NULL;

    // Clear previous content
    lv_obj_clean(gui_state.content_container);

    gui_state.state = UGUI_APP_STATE_RUNNING;
    gui_state.current_app_id = app_id;

    ESP_LOGI(TAG, "App started (ID: %lu)", app_id);

    uflake_mutex_unlock(gui_mutex);

    return gui_state.content_container;
}

void uGui_exit_app(void)
{
    if (!gui_initialized)
        return;

    if (uflake_mutex_lock(gui_mutex, 100) != UFLAKE_OK)
        return;

    // Clear app content
    if (gui_state.content_container != NULL)
    {
        lv_obj_clean(gui_state.content_container);
    }

    gui_state.state = UGUI_APP_STATE_HOME;
    gui_state.current_app_id = 0;

    ESP_LOGI(TAG, "App exited, returning to home");

    uflake_mutex_unlock(gui_mutex);

    // Call exit callback if registered
    if (app_exit_callback != NULL)
    {
        app_exit_callback(app_exit_userdata);
    }
}

bool uGui_is_home_screen(void)
{
    return gui_state.state == UGUI_APP_STATE_HOME;
}

void uGui_set_app_exit_callback(ugui_app_exit_callback_t callback, void *userdata)
{
    app_exit_callback = callback;
    app_exit_userdata = userdata;
}

// ============================================================================
// SCREEN MANAGEMENT
// ============================================================================

void uGui_clear_screen(void)
{
    if (gui_state.screen == NULL)
        return;

    if (uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
    {
        lv_obj_clean(gui_state.screen);
        uflake_mutex_unlock(gui_mutex);
    }
}

// ============================================================================
// HOME SCREEN / MENU
// ============================================================================

void uGui_show_home_screen(void)
{
    uGui_exit_app(); // Clears content and resets state
}

void uGui_refresh_home_screen(void)
{
    // Redraw home screen if visible
    if (uGui_is_home_screen() && gui_state.content_container != NULL)
    {
        if (uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
        {
            lv_obj_clean(gui_state.content_container);
            // App loader will redraw the home screen
            uflake_mutex_unlock(gui_mutex);
        }
    }
}

// ============================================================================
// NOTIFICATION BAR
// ============================================================================

void uGui_notification_show(void)
{
    gui_state.notification_enabled = true;
}

void uGui_notification_hide(void)
{
    gui_state.notification_enabled = false;
}

void uGui_notification_set_text(const char *text, uint32_t timeout_ms)
{
    // TODO: Implement notification display
    (void)text;
    (void)timeout_ms;
}

void uGui_notification_set_app_name(const char *app_name)
{
    // TODO: Update notification bar with app name
    (void)app_name;
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void uGui_input_button(ugui_button_t button, bool pressed)
{
    if (button >= UGUI_BTN_COUNT)
        return;

    if (!pressed)
        return; // Only handle press, not release

    // BACK key exits app
    if (button == UGUI_BTN_BACK && gui_state.state == UGUI_APP_STATE_RUNNING)
    {
        uGui_exit_app();
        return;
    }

    // Route to app's input handler if registered
    if (input_handlers[button] != NULL)
    {
        input_handlers[button](button, pressed, input_handler_userdata[button]);
    }
}

void uGui_register_input_handler(ugui_button_t button,
                                 ugui_input_callback_t callback,
                                 void *userdata)
{
    if (button < UGUI_BTN_COUNT)
    {
        input_handlers[button] = callback;
        input_handler_userdata[button] = userdata;
    }
}

void uGui_unregister_input_handler(ugui_button_t button)
{
    if (button < UGUI_BTN_COUNT)
    {
        input_handlers[button] = NULL;
        input_handler_userdata[button] = NULL;
    }
}

// LVGL input device read callback (called by LVGL)
static void input_read_callback(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    // This will be called by LVGL when it needs input
    // In our simplified system, input is handled externally
    // So just return no key pressed
    data->key = 0;
    data->state = LV_INDEV_STATE_REL;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

uint32_t uGui_get_free_ram(void)
{
    return gui_state.free_ram;
}

ugui_mem_pressure_t uGui_get_memory_pressure(void)
{
    return gui_state.mem_pressure;
}

bool uGui_has_memory(uint32_t required_bytes)
{
    return gui_state.free_ram >= required_bytes;
}

void uGui_register_memory_callback(ugui_memory_pressure_cb_t callback, void *userdata)
{
    memory_pressure_callback = callback;
    memory_pressure_userdata = userdata;
}

static void update_memory_stats(void)
{
    // Get free heap from FreeRTOS
    gui_state.free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

static void check_memory_pressure(void)
{
    uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    uint32_t free_heap = gui_state.free_ram;
    uint32_t used_percent = ((total_heap - free_heap) * 100) / total_heap;

    ugui_mem_pressure_t new_pressure;

    if (used_percent > 80) // >80% used = critical
        new_pressure = UGUI_MEM_CRITICAL;
    else if (used_percent > 50) // >50% used = low
        new_pressure = UGUI_MEM_LOW;
    else
        new_pressure = UGUI_MEM_NORMAL;

    if (new_pressure != gui_state.mem_pressure)
    {
        gui_state.mem_pressure = new_pressure;

        ESP_LOGW(TAG, "Memory pressure changed: %d%% used (%lu / %lu bytes)",
                 used_percent, total_heap - free_heap, total_heap);

        if (memory_pressure_callback != NULL)
        {
            memory_pressure_callback(new_pressure, memory_pressure_userdata);
        }
    }
}

// ============================================================================
// THEME
// ============================================================================

void uGui_set_theme(ugui_theme_t *theme)
{
    if (theme == NULL || !gui_initialized)
        return;

    if (uflake_mutex_lock(gui_mutex, 100) == UFLAKE_OK)
    {
        memcpy(&current_theme, theme, sizeof(ugui_theme_t));

        // Apply theme colors
        if (gui_state.content_container != NULL)
        {
            lv_obj_set_style_bg_color(gui_state.content_container, current_theme.bg_primary, 0);
        }

        uflake_mutex_unlock(gui_mutex);
    }
}
