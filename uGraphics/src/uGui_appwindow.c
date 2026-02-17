/**
 * @file uGui_appwindow.c
 * @brief App Window Manager Implementation
 */

#include "uGui_appwindow.h"
#include "uGui_focus.h"
#include "uGui_notification.h"
#include "logger.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "uGUI_AppWin";

// ============================================================================
// ANIMATION CALLBACKS
// ============================================================================

static void anim_set_opa_cb(void *obj, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

// ============================================================================
// APP WINDOW MANAGER STATE
// ============================================================================

#define MAX_APP_WINDOWS 8

typedef struct
{
    bool initialized;
    ugui_appwin_t windows[MAX_APP_WINDOWS];
    ugui_appwin_t *active_window;
    uflake_mutex_t *mutex;
} appwindow_manager_t;

static appwindow_manager_t g_appwin_mgr = {0};

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static ugui_appwin_t *find_free_window(void)
{
    for (int i = 0; i < MAX_APP_WINDOWS; i++)
    {
        if (!g_appwin_mgr.windows[i].is_active && !g_appwin_mgr.windows[i].container)
        {
            return &g_appwin_mgr.windows[i];
        }
    }
    return NULL;
}

static ugui_appwin_t *find_window_by_app_id(uint32_t app_id)
{
    for (int i = 0; i < MAX_APP_WINDOWS; i++)
    {
        if (g_appwin_mgr.windows[i].container && g_appwin_mgr.windows[i].app_id == app_id)
        {
            return &g_appwin_mgr.windows[i];
        }
    }
    return NULL;
}

static void apply_default_config(ugui_appwin_config_t *config)
{
    if (!config->app_name)
    {
        config->app_name = "App";
    }
    if (config->width == 0)
    {
        config->width = UGUI_APPWINDOW_WIDTH;
    }
    if (config->height == 0)
    {
        config->height = UGUI_APPWINDOW_HEIGHT;
    }
    if (config->bg_opacity == 0)
    {
        config->bg_opacity = 255; // Opaque by default
    }
}

static void update_window_geometry(ugui_appwin_t *window)
{
    if (!window || !window->container)
    {
        return;
    }

    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t width = window->config.width;
    uint16_t height = window->config.height;

    if (window->config.flags & UGUI_APPWIN_FLAG_FULLSCREEN)
    {
        // Fullscreen - hide notification bar
        ugui_notification_hide();
        x = 0;
        y = 0;
        width = UGUI_DISPLAY_WIDTH;
        height = UGUI_DISPLAY_HEIGHT;
    }
    else
    {
        // Normal - below horizontal notification bar at top
        ugui_notification_show();
        x = UGUI_APPWINDOW_X_OFFSET;
        y = UGUI_APPWINDOW_Y_OFFSET; // Start below notification bar (top)
        width = UGUI_APPWINDOW_WIDTH;
        height = UGUI_APPWINDOW_HEIGHT;
    }

    lv_obj_set_size(window->container, width, height);
    lv_obj_set_pos(window->container, x, y);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t ugui_appwindow_init(void)
{
    if (g_appwin_mgr.initialized)
    {
        UFLAKE_LOGW(TAG, "App window manager already initialized");
        return UFLAKE_OK;
    }

    memset(&g_appwin_mgr, 0, sizeof(appwindow_manager_t));

    // Create mutex
    if (uflake_mutex_create(&g_appwin_mgr.mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create app window mutex");
        return UFLAKE_ERROR;
    }

    g_appwin_mgr.initialized = true;

    UFLAKE_LOGI(TAG, "App window manager initialized");

    return UFLAKE_OK;
}

// ============================================================================
// APP WINDOW LIFECYCLE
// ============================================================================

ugui_appwin_t *ugui_appwindow_create(const ugui_appwin_config_t *config, uint32_t app_id)
{
    if (!g_appwin_mgr.initialized)
    {
        UFLAKE_LOGE(TAG, "App window manager not initialized");
        return NULL;
    }

    uflake_mutex_lock(g_appwin_mgr.mutex, 100);

    // Check if window already exists for this app
    ugui_appwin_t *existing = find_window_by_app_id(app_id);
    if (existing)
    {
        UFLAKE_LOGW(TAG, "Window already exists for app ID %lu", app_id);
        uflake_mutex_unlock(g_appwin_mgr.mutex);
        return existing;
    }

    // Find free window slot
    ugui_appwin_t *window = find_free_window();
    if (!window)
    {
        UFLAKE_LOGE(TAG, "No free window slots (max %d)", MAX_APP_WINDOWS);
        uflake_mutex_unlock(g_appwin_mgr.mutex);
        return NULL;
    }

    // Initialize window structure
    memset(window, 0, sizeof(ugui_appwin_t));
    window->app_id = app_id;

    // Apply configuration
    if (config)
    {
        window->config = *config;
    }
    else
    {
        // Default config
        window->config.app_name = "App";
        window->config.width = 0;
        window->config.height = 0;
        window->config.flags = UGUI_APPWIN_FLAG_NONE;
        window->config.bg_color = lv_color_hex(0x000000);
        window->config.bg_opacity = 255;
    }
    apply_default_config(&window->config);

    // Create container
    window->container = lv_obj_create(lv_scr_act());
    if (!window->container)
    {
        UFLAKE_LOGE(TAG, "Failed to create window container");
        memset(window, 0, sizeof(ugui_appwin_t));
        uflake_mutex_unlock(g_appwin_mgr.mutex);
        return NULL;
    }

    // Configure container style
    lv_obj_set_style_bg_color(window->container, window->config.bg_color, 0);
    lv_obj_set_style_bg_opa(window->container, window->config.bg_opacity, 0);
    lv_obj_set_style_border_width(window->container, 0, 0);
    lv_obj_set_style_pad_all(window->container, 0, 0);

    // Apply scrolling flag
    if (window->config.flags & UGUI_APPWIN_FLAG_NO_SCROLL)
    {
        lv_obj_clear_flag(window->container, LV_OBJ_FLAG_SCROLLABLE);
    }
    else
    {
        lv_obj_set_scrollbar_mode(window->container, LV_SCROLLBAR_MODE_AUTO);
    }

    // Create content area (child of container)
    window->content = lv_obj_create(window->container);
    if (!window->content)
    {
        UFLAKE_LOGE(TAG, "Failed to create content container");
        lv_obj_del(window->container);
        memset(window, 0, sizeof(ugui_appwin_t));
        uflake_mutex_unlock(g_appwin_mgr.mutex);
        return NULL;
    }

    // Content fills container
    lv_obj_set_size(window->content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(window->content, 0, 0);
    lv_obj_set_style_bg_opa(window->content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(window->content, 0, 0);
    lv_obj_set_style_pad_all(window->content, 4, 0);

    // Update geometry based on config
    update_window_geometry(window);

    // Register with focus manager (app window layer, auto-focus)
    window->focus.focused_obj = window->container;
    window->focus.layer = UGUI_LAYER_APP_WINDOW;
    window->focus.input_enabled = true;

    ugui_focus_ctx_t *focus_ctx = ugui_focus_register(window->container,
                                                      UGUI_LAYER_APP_WINDOW,
                                                      true);
    if (!focus_ctx)
    {
        UFLAKE_LOGE(TAG, "Failed to register window with focus manager");
        lv_obj_del(window->container);
        memset(window, 0, sizeof(ugui_appwin_t));
        uflake_mutex_unlock(g_appwin_mgr.mutex);
        return NULL;
    }

    // Copy focus context
    window->focus = *focus_ctx;

    // Mark as active
    window->is_active = true;
    g_appwin_mgr.active_window = window;

    // Show app name in notification bar
    ugui_notification_show_app_name(window->config.app_name, 3000); // 3 seconds

    UFLAKE_LOGI(TAG, "Created app window for app ID %lu (%s)",
                app_id, window->config.app_name);

    uflake_mutex_unlock(g_appwin_mgr.mutex);

    return window;
}

uflake_result_t ugui_appwindow_destroy(ugui_appwin_t *window)
{
    if (!g_appwin_mgr.initialized || !window)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_appwin_mgr.mutex, 100);

    UFLAKE_LOGI(TAG, "Destroying app window for app ID %lu", window->app_id);

    // Unregister from focus manager - CRITICAL for safe deletion
    ugui_focus_ctx_t *ctx = &window->focus;
    ugui_focus_unregister(ctx);

    // Delete container (and all children)
    if (window->container)
    {
        lv_obj_del(window->container);
        window->container = NULL;
        window->content = NULL;
    }

    // Clear active window if this was it
    if (g_appwin_mgr.active_window == window)
    {
        g_appwin_mgr.active_window = NULL;

        // Restore notification bar to system info
        ugui_notification_clear_app_name();
        ugui_notification_show();
    }

    // Clear window structure
    memset(window, 0, sizeof(ugui_appwin_t));

    UFLAKE_LOGI(TAG, "App window destroyed successfully");

    uflake_mutex_unlock(g_appwin_mgr.mutex);

    return UFLAKE_OK;
}

lv_obj_t *ugui_appwindow_get_content(ugui_appwin_t *window)
{
    if (!window)
    {
        return NULL;
    }
    return window->content;
}

// ============================================================================
// FOCUS MANAGEMENT
// ============================================================================

uflake_result_t ugui_appwindow_add_focusable(ugui_appwin_t *window, lv_obj_t *obj)
{
    if (!window || !obj)
    {
        return UFLAKE_ERROR;
    }

    return ugui_focus_add_to_group(&window->focus, obj);
}

uflake_result_t ugui_appwindow_remove_focusable(ugui_appwin_t *window, lv_obj_t *obj)
{
    if (!window || !obj)
    {
        return UFLAKE_ERROR;
    }

    return ugui_focus_remove_from_group(&window->focus, obj);
}

uflake_result_t ugui_appwindow_focus_next(ugui_appwin_t *window)
{
    if (!window)
    {
        return UFLAKE_ERROR;
    }

    return ugui_focus_next(&window->focus);
}

uflake_result_t ugui_appwindow_focus_prev(ugui_appwin_t *window)
{
    if (!window)
    {
        return UFLAKE_ERROR;
    }

    return ugui_focus_prev(&window->focus);
}

// ============================================================================
// WINDOW STATE
// ============================================================================

uflake_result_t ugui_appwindow_activate(ugui_appwin_t *window)
{
    if (!g_appwin_mgr.initialized || !window)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_appwin_mgr.mutex, 100);

    // Request focus
    ugui_focus_request(&window->focus);

    // Bring to front
    if (window->container)
    {
        lv_obj_move_foreground(window->container);
    }

    // Mark as active
    window->is_active = true;
    g_appwin_mgr.active_window = window;

    // Show app name
    ugui_notification_show_app_name(window->config.app_name, 2000);

    UFLAKE_LOGI(TAG, "Activated app window %lu", window->app_id);

    uflake_mutex_unlock(g_appwin_mgr.mutex);

    return UFLAKE_OK;
}

uflake_result_t ugui_appwindow_deactivate(ugui_appwin_t *window)
{
    if (!g_appwin_mgr.initialized || !window)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_appwin_mgr.mutex, 100);

    // Release focus
    ugui_focus_release(&window->focus);

    // Mark as inactive
    window->is_active = false;

    if (g_appwin_mgr.active_window == window)
    {
        g_appwin_mgr.active_window = NULL;
    }

    UFLAKE_LOGI(TAG, "Deactivated app window %lu", window->app_id);

    uflake_mutex_unlock(g_appwin_mgr.mutex);

    return UFLAKE_OK;
}

bool ugui_appwindow_is_active(ugui_appwin_t *window)
{
    if (!window)
    {
        return false;
    }
    return window->is_active;
}

// ============================================================================
// WINDOW CONFIGURATION
// ============================================================================

uflake_result_t ugui_appwindow_set_fullscreen(ugui_appwin_t *window, bool fullscreen)
{
    if (!window)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_appwin_mgr.mutex, 100);

    if (fullscreen)
    {
        window->config.flags |= UGUI_APPWIN_FLAG_FULLSCREEN;
    }
    else
    {
        window->config.flags &= ~UGUI_APPWIN_FLAG_FULLSCREEN;
    }

    update_window_geometry(window);

    uflake_mutex_unlock(g_appwin_mgr.mutex);

    return UFLAKE_OK;
}

uflake_result_t ugui_appwindow_set_size(ugui_appwin_t *window, uint16_t width, uint16_t height)
{
    if (!window)
    {
        return UFLAKE_ERROR;
    }

    uflake_mutex_lock(g_appwin_mgr.mutex, 100);

    if (width > 0 && height > 0)
    {
        window->config.flags |= UGUI_APPWIN_FLAG_CUSTOM_SIZE;
        window->config.width = width;
        window->config.height = height;
    }
    else
    {
        window->config.flags &= ~UGUI_APPWIN_FLAG_CUSTOM_SIZE;
        window->config.width = UGUI_APPWINDOW_WIDTH;
        window->config.height = UGUI_APPWINDOW_HEIGHT;
    }

    update_window_geometry(window);

    uflake_mutex_unlock(g_appwin_mgr.mutex);

    return UFLAKE_OK;
}

uflake_result_t ugui_appwindow_set_background(ugui_appwin_t *window, lv_color_t color, uint8_t opacity)
{
    if (!window || !window->container)
    {
        return UFLAKE_ERROR;
    }

    window->config.bg_color = color;
    window->config.bg_opacity = opacity;

    lv_obj_set_style_bg_color(window->container, color, 0);
    lv_obj_set_style_bg_opa(window->container, opacity, 0);

    return UFLAKE_OK;
}

uflake_result_t ugui_appwindow_set_visible(ugui_appwin_t *window, bool visible)
{
    if (!window || !window->container)
    {
        return UFLAKE_ERROR;
    }

    if (visible)
    {
        lv_obj_clear_flag(window->container, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(window->container, LV_OBJ_FLAG_HIDDEN);
    }

    return UFLAKE_OK;
}

// ============================================================================
// ANIMATION SUPPORT
// ============================================================================

uflake_result_t ugui_appwindow_fade_in(ugui_appwin_t *window, uint32_t duration_ms)
{
    if (!window || !window->container)
    {
        return UFLAKE_ERROR;
    }

    lv_obj_set_style_opa(window->container, LV_OPA_TRANSP, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, window->container);
    lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&anim, duration_ms);
    lv_anim_set_exec_cb(&anim, anim_set_opa_cb);
    lv_anim_start(&anim);

    return UFLAKE_OK;
}

uflake_result_t ugui_appwindow_fade_out(ugui_appwin_t *window, uint32_t duration_ms)
{
    if (!window || !window->container)
    {
        return UFLAKE_ERROR;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, window->container);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&anim, duration_ms);
    lv_anim_set_exec_cb(&anim, anim_set_opa_cb);
    lv_anim_start(&anim);

    return UFLAKE_OK;
}

// ============================================================================
// SAFE CLEANUP HELPERS
// ============================================================================

uflake_result_t ugui_appwindow_clear_content(ugui_appwin_t *window)
{
    if (!window || !window->content)
    {
        return UFLAKE_ERROR;
    }

    // Safely delete all children
    return ugui_focus_safe_delete_children(window->content);
}

uflake_result_t ugui_appwindow_delete_object(ugui_appwin_t *window, lv_obj_t *obj)
{
    if (!window || !obj)
    {
        return UFLAKE_ERROR;
    }

    // Safely delete object
    return ugui_focus_safe_delete(NULL, obj);
}

// ============================================================================
// INTEGRATION WITH APP LOADER
// ============================================================================

ugui_appwin_t *ugui_appwindow_get_by_app_id(uint32_t app_id)
{
    if (!g_appwin_mgr.initialized)
    {
        return NULL;
    }

    return find_window_by_app_id(app_id);
}

ugui_appwin_t *ugui_appwindow_get_active(void)
{
    return g_appwin_mgr.active_window;
}

// ============================================================================
// DEBUG
// ============================================================================

void ugui_appwindow_debug_print(ugui_appwin_t *window)
{
    if (!window)
    {
        UFLAKE_LOGE(TAG, "Invalid window");
        return;
    }

    UFLAKE_LOGI(TAG, "=== App Window Debug ===");
    UFLAKE_LOGI(TAG, "App ID: %lu", window->app_id);
    UFLAKE_LOGI(TAG, "App Name: %s", window->config.app_name);
    UFLAKE_LOGI(TAG, "Active: %d", window->is_active);
    UFLAKE_LOGI(TAG, "Size: %dx%d", window->config.width, window->config.height);
    UFLAKE_LOGI(TAG, "Flags: 0x%X", window->config.flags);
    UFLAKE_LOGI(TAG, "Container: %p", window->container);
    UFLAKE_LOGI(TAG, "Content: %p", window->content);
}
