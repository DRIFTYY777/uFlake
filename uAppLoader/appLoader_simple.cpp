/**
 * @file appLoader_simple.cpp
 * @brief Simplified Application Loader Implementation
 *
 * Simple, focused implementation without complex lifecycle management.
 * Direct app launching with memory checks.
 */

#include "appLoader_simple.h"
#include "uGui_simple.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kernel.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "APP_LOADER";

// ============================================================================
// GLOBAL STATE
// ============================================================================

static app_descriptor_t app_registry[MAX_APPS];
static uint32_t app_count = 0;
static uint32_t next_app_id = 1;
static uint32_t launcher_app_id = 0;
static bool initialized = false;
static uflake_mutex_t *loader_mutex = NULL;

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t app_loader_init(void)
{
    if (initialized)
    {
        ESP_LOGW(TAG, "App loader already initialized");
        return UFLAKE_OK;
    }

    // Create mutex for thread safety
    if (uflake_mutex_create(&loader_mutex) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    app_count = 0;
    next_app_id = 1;
    launcher_app_id = 0;

    initialized = true;
    ESP_LOGI(TAG, "App loader initialized");

    return UFLAKE_OK;
}

// ============================================================================
// APP REGISTRATION
// ============================================================================

uint32_t app_loader_register_internal_app(const app_bundle_t *bundle, bool is_launcher)
{
    if (!initialized || app_count >= MAX_APPS || bundle == NULL)
    {
        ESP_LOGE(TAG, "Cannot register app: not initialized or max apps reached");
        return 0;
    }

    if (uflake_mutex_lock(loader_mutex, 100) != UFLAKE_OK)
    {
        return 0;
    }

    uint32_t app_id = next_app_id++;
    app_descriptor_t *app = &app_registry[app_count++];

    // Initialize descriptor
    app->app_id = app_id;
    memcpy(&app->manifest, &bundle->manifest, sizeof(app_manifest_t));
    app->location = APP_LOCATION_INTERNAL;
    app->entry_point = bundle->entry_point;
    app->state = APP_STATE_STOPPED;
    app->task_id = 0;
    app->ram_used = 0;
    app->is_launcher = is_launcher;
    snprintf(app->path, APP_PATH_MAX, "[internal] %s", bundle->manifest.name);

    if (is_launcher)
    {
        launcher_app_id = app_id;
        ESP_LOGI(TAG, "Registered launcher app: %s (ID: %lu)", bundle->manifest.name, app_id);
    }
    else
    {
        ESP_LOGI(TAG, "Registered app: %s (ID: %lu)", bundle->manifest.name, app_id);
    }

    uflake_mutex_unlock(loader_mutex);
    return app_id;
}

uint32_t app_loader_register_external_app(const char *path)
{
    // TODO: Implement external app registration from SD card
    (void)path;
    ESP_LOGW(TAG, "External app registration not yet implemented");
    return 0;
}

// ============================================================================
// APP QUERIES
// ============================================================================

uint32_t app_loader_get_app_count(void)
{
    if (uflake_mutex_lock(loader_mutex, 10) != UFLAKE_OK)
        return app_count;

    uint32_t count = app_count;
    uflake_mutex_unlock(loader_mutex);
    return count;
}

app_descriptor_t* app_loader_get_app(uint32_t app_id)
{
    for (uint32_t i = 0; i < app_count; i++)
    {
        if (app_registry[i].app_id == app_id)
        {
            return &app_registry[i];
        }
    }
    return NULL;
}

uint32_t app_loader_get_app_id_by_name(const char *name)
{
    if (name == NULL)
        return 0;

    if (uflake_mutex_lock(loader_mutex, 10) != UFLAKE_OK)
        return 0;

    for (uint32_t i = 0; i < app_count; i++)
    {
        if (strcmp(app_registry[i].manifest.name, name) == 0)
        {
            uint32_t id = app_registry[i].app_id;
            uflake_mutex_unlock(loader_mutex);
            return id;
        }
    }

    uflake_mutex_unlock(loader_mutex);
    return 0;
}

app_descriptor_t* app_loader_get_app_list(uint32_t *count)
{
    if (count != NULL)
    {
        *count = app_count;
    }
    return app_registry;
}

uint32_t app_loader_get_launcher_id(void)
{
    return launcher_app_id;
}

// ============================================================================
// APP LIFECYCLE
// ============================================================================

uflake_result_t app_loader_launch_app(uint32_t app_id)
{
    if (!initialized)
    {
        ESP_LOGE(TAG, "App loader not initialized");
        return UFLAKE_ERROR;
    }

    if (uflake_mutex_lock(loader_mutex, 100) != UFLAKE_OK)
    {
        return UFLAKE_ERROR;
    }

    app_descriptor_t *app = app_loader_get_app(app_id);
    if (app == NULL)
    {
        uflake_mutex_unlock(loader_mutex);
        ESP_LOGE(TAG, "App not found: ID %lu", app_id);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    // Check memory
    uint32_t free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    uint32_t required_ram = app->manifest.min_ram_bytes > 0 ? 
                           app->manifest.min_ram_bytes : 65536;  // Default 64KB

    if (free_ram < required_ram)
    {
        uflake_mutex_unlock(loader_mutex);
        ESP_LOGE(TAG, "Insufficient memory to launch %s: need %lu bytes, have %lu bytes",
                 app->manifest.name, required_ram, free_ram);
        return UFLAKE_ERROR_MEMORY;
    }

    // Set state
    app->state = APP_STATE_RUNNING;

    // For GUI apps: call entry point directly from GUI context
    if (app->manifest.subtype == APP_SUBTYPE_GUI)
    {
        lv_obj_t *container = uGui_start_app(app_id);
        if (container == NULL)
        {
            app->state = APP_STATE_STOPPED;
            uflake_mutex_unlock(loader_mutex);
            ESP_LOGE(TAG, "GUI context not available");
            return UFLAKE_ERROR;
        }

        uflake_mutex_unlock(loader_mutex);

        // Call app entry point
        if (app->entry_point != NULL)
        {
            ESP_LOGI(TAG, "Launching GUI app: %s", app->manifest.name);
            app->entry_point();
        }
    }
    else
    {
        // For non-GUI apps: create background task
        uint32_t stack_size = app->manifest.stack_size > 0 ? 
                             app->manifest.stack_size : 8192;
        process_priority_t priority = app->manifest.priority > 0 ? 
                                     (process_priority_t)app->manifest.priority : PROCESS_PRIORITY_NORMAL;

        if (app->entry_point != NULL)
        {
            uint32_t task_id;
            if (uflake_process_create(app->manifest.name, 
                                     (void (*)(void*))app->entry_point, 
                                     NULL, stack_size, priority, &task_id) == UFLAKE_OK)
            {
                app->task_id = task_id;
                ESP_LOGI(TAG, "Launched non-GUI app: %s (task %lu)", 
                        app->manifest.name, task_id);
            }
            else
            {
                app->state = APP_STATE_STOPPED;
                uflake_mutex_unlock(loader_mutex);
                ESP_LOGE(TAG, "Failed to create task for app: %s", 
                        app->manifest.name);
                return UFLAKE_ERROR;
            }
        }

        uflake_mutex_unlock(loader_mutex);
    }

    return UFLAKE_OK;
}

uflake_result_t app_loader_launch_app_by_name(const char *name)
{
    uint32_t app_id = app_loader_get_app_id_by_name(name);
    if (app_id == 0)
    {
        ESP_LOGE(TAG, "App not found: %s", name);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    return app_loader_launch_app(app_id);
}

bool app_loader_can_launch(uint32_t app_id)
{
    app_descriptor_t *app = app_loader_get_app(app_id);
    if (app == NULL)
        return false;

    uint32_t free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    uint32_t required_ram = app->manifest.min_ram_bytes > 0 ? 
                           app->manifest.min_ram_bytes : 65536;

    return free_ram >= required_ram;
}

uflake_result_t app_loader_terminate_app(uint32_t app_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    if (uflake_mutex_lock(loader_mutex, 100) != UFLAKE_OK)
        return UFLAKE_ERROR;

    app_descriptor_t *app = app_loader_get_app(app_id);
    if (app == NULL)
    {
        uflake_mutex_unlock(loader_mutex);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    if (app->manifest.subtype == APP_SUBTYPE_GUI)
    {
        uGui_exit_app();
    }
    else if (app->task_id != 0)
    {
        // TODO: Terminate background task
        app->task_id = 0;
    }

    app->state = APP_STATE_STOPPED;
    ESP_LOGI(TAG, "App terminated: %s", app->manifest.name);

    uflake_mutex_unlock(loader_mutex);
    return UFLAKE_OK;
}

void app_loader_terminate_all_apps(void)
{
    if (uflake_mutex_lock(loader_mutex, 100) != UFLAKE_OK)
        return;

    for (uint32_t i = 0; i < app_count; i++)
    {
        if (app_registry[i].state == APP_STATE_RUNNING)
        {
            app_registry[i].state = APP_STATE_STOPPED;
            if (app_registry[i].task_id != 0)
            {
                // TODO: Terminate task
                app_registry[i].task_id = 0;
            }
        }
    }

    uflake_mutex_unlock(loader_mutex);
    uGui_exit_app();
}

// ============================================================================
// DEBUGGING / STATUS
// ============================================================================

void app_loader_print_app_list(void)
{
    if (uflake_mutex_lock(loader_mutex, 10) != UFLAKE_OK)
        return;

    ESP_LOGI(TAG, "=== Registered Apps (%lu) ===", app_count);

    for (uint32_t i = 0; i < app_count; i++)
    {
        app_descriptor_t *app = &app_registry[i];
        const char *type = app->manifest.subtype == APP_SUBTYPE_GUI ? "GUI" : "Non-GUI";
        const char *location = app->location == APP_LOCATION_INTERNAL ? "Internal" : "External";

        ESP_LOGI(TAG, "[%lu] %s (%s, %s) - %s",
                 app->app_id,
                 app->manifest.name,
                 type,
                 location,
                 app->manifest.description);
    }

    uflake_mutex_unlock(loader_mutex);
}

void app_loader_print_app_info(uint32_t app_id)
{
    app_descriptor_t *app = app_loader_get_app(app_id);
    if (app == NULL)
    {
        ESP_LOGW(TAG, "App not found: ID %lu", app_id);
        return;
    }

    ESP_LOGI(TAG, "App: %s", app->manifest.name);
    ESP_LOGI(TAG, "  Version: %s", app->manifest.version);
    ESP_LOGI(TAG, "  Author: %s", app->manifest.author);
    ESP_LOGI(TAG, "  Description: %s", app->manifest.description);
    ESP_LOGI(TAG, "  Category: %u", app->manifest.category);
    ESP_LOGI(TAG, "  Stack size: %lu bytes", app->manifest.stack_size);
    ESP_LOGI(TAG, "  Min RAM: %lu bytes", app->manifest.min_ram_bytes);
    ESP_LOGI(TAG, "  Requires SD: %s", app->manifest.requires_sdcard ? "yes" : "no");
    ESP_LOGI(TAG, "  Current RAM: %lu bytes", app->ram_used);
}

void app_loader_print_memory_usage(void)
{
    uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    uint32_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    uint32_t used_heap = total_heap - free_heap;

    ESP_LOGI(TAG, "=== Memory Usage ===");
    ESP_LOGI(TAG, "Total heap: %lu bytes", total_heap);
    ESP_LOGI(TAG, "Used heap: %lu bytes (%.1f%%)", used_heap, 
             (float)used_heap * 100.0f / total_heap);
    ESP_LOGI(TAG, "Free heap: %lu bytes (%.1f%%)", free_heap,
             (float)free_heap * 100.0f / total_heap);
}
