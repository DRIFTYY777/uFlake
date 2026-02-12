/**
 * @file appLoader.c
 * @brief uFlake Application Loader - Core Implementation
 *
 * This module handles app registration and management.
 * App lifecycle is handled by appLifecycle module.
 * Services are handled by appService module.
 * Manifest parsing is handled by appManifest module.
 */

#include "appLoader.h"
#include "appLifecycle.h"
#include "appManifest.h"
#include "appService.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "APP_LOADER";

// Global state
static app_descriptor_t app_registry[MAX_APPS];
static uint32_t app_count = 0;
static uint32_t next_app_id = 1;
static uflake_mutex_t *app_loader_mutex = NULL;
static bool initialized = false;

// Current app tracking
static uint32_t current_app_id = 0;
static uint32_t launcher_app_id = 0;

// Force exit tracking
static bool force_exit_buttons_pressed = false;
static uint64_t force_exit_press_time = 0;

// ============================================================================
// PRIVATE HELPERS (exposed to submodules)
// ============================================================================

app_descriptor_t *app_loader_find_app_by_id(uint32_t app_id)
{
    for (uint32_t i = 0; i < app_count; i++)
    {
        if (app_registry[i].app_id == app_id)
            return &app_registry[i];
    }
    return NULL;
}

// ============================================================================
// APP LOADER INITIALIZATION
// ============================================================================

uflake_result_t app_loader_init(void)
{
    if (initialized)
    {
        ESP_LOGW(TAG, "App loader already initialized");
        return UFLAKE_OK;
    }

    // Create mutex using uFlake API
    if (uflake_mutex_create(&app_loader_mutex) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    app_count = 0;
    next_app_id = 1;
    current_app_id = 0;
    launcher_app_id = 0;

    // Initialize sub-modules
    uflake_result_t result;

    result = app_lifecycle_init();
    if (result != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize lifecycle manager");
        return result;
    }

    result = service_manager_init();
    if (result != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize service manager");
        return result;
    }

    initialized = true;
    ESP_LOGI(TAG, "App loader initialized");
    return UFLAKE_OK;
}

uflake_result_t app_loader_scan_external_apps(void)
{
    ESP_LOGI(TAG, "Scanning external apps from SD card: %s", EXTERNAL_APPS_FOLDER);

    // TODO: Implement .fap file scanning and loading from SD card
    // This WOULD work for external apps since they're files on storage

    ESP_LOGI(TAG, "External app scan complete (feature not yet implemented)");
    return UFLAKE_OK;
}

// ============================================================================
// APP REGISTRATION
// ============================================================================
uint32_t app_loader_register(const app_bundle_t *app_bundle)
{
    if (!app_bundle || !app_bundle->manifest || !app_bundle->entry_point)
    {
        ESP_LOGE(TAG, "Invalid app bundle");
        return 0;
    }

    return app_loader_register_internal(
        app_bundle->manifest,
        app_bundle->entry_point,
        app_bundle->is_launcher);
}

uint32_t app_loader_register_internal(const app_manifest_t *manifest,
                                      app_entry_fn entry_point,
                                      bool is_launcher)
{
    if (!initialized || !manifest || !entry_point)
    {
        ESP_LOGE(TAG, "Invalid parameters for app registration");
        return 0;
    }

    if (app_count >= MAX_APPS)
    {
        ESP_LOGE(TAG, "App registry full (max %d apps)", MAX_APPS);
        return 0;
    }

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);

    app_descriptor_t *app = &app_registry[app_count];
    memset(app, 0, sizeof(app_descriptor_t));

    app->app_id = next_app_id++;
    memcpy(&app->manifest, manifest, sizeof(app_manifest_t));
    app->location = APP_LOCATION_INTERNAL;
    app->entry_point = (void *)entry_point;
    app->elf_handle = NULL;
    app->state = APP_STATE_STOPPED;
    app->task_handle = NULL;
    app->is_launcher = is_launcher;
    app->launch_count = 0;
    app->last_run_time = 0;

    if (is_launcher)
    {
        launcher_app_id = app->app_id;
        ESP_LOGI(TAG, "Registered launcher: %s (ID: %lu)", manifest->name, app->app_id);
    }
    else
    {
        ESP_LOGI(TAG, "Registered app: %s v%s (ID: %lu)",
                 manifest->name, manifest->version, app->app_id);
    }

    app_count++;
    uflake_mutex_unlock(app_loader_mutex);

    return app->app_id;
}

uint32_t app_loader_register_external(const char *fap_path)
{
    ESP_LOGI(TAG, "External app loading not yet implemented: %s", fap_path);
    // TODO: Load .fap file, parse manifest, register
    return 0;
}

// ============================================================================
// APP LIFECYCLE (Delegated to appLifecycle module)
// ============================================================================

uflake_result_t app_loader_launch(uint32_t app_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);

    app_descriptor_t *app = app_loader_find_app_by_id(app_id);
    if (!app)
    {
        uflake_mutex_unlock(app_loader_mutex);
        ESP_LOGE(TAG, "App ID %lu not found", app_id);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    uflake_result_t result = app_lifecycle_launch(app, launcher_app_id, &current_app_id);
    uflake_mutex_unlock(app_loader_mutex);

    return result;
}

uflake_result_t app_loader_terminate(uint32_t app_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);

    app_descriptor_t *app = app_loader_find_app_by_id(app_id);
    if (!app)
    {
        uflake_mutex_unlock(app_loader_mutex);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    uflake_result_t result = app_lifecycle_terminate(app, launcher_app_id, &current_app_id);
    uflake_mutex_unlock(app_loader_mutex);

    return result;
}

uflake_result_t app_loader_pause(uint32_t app_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);

    app_descriptor_t *app = app_loader_find_app_by_id(app_id);
    if (!app)
    {
        uflake_mutex_unlock(app_loader_mutex);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    uflake_result_t result = app_lifecycle_pause(app);
    uflake_mutex_unlock(app_loader_mutex);

    return result;
}

uflake_result_t app_loader_resume(uint32_t app_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);

    app_descriptor_t *app = app_loader_find_app_by_id(app_id);
    if (!app)
    {
        uflake_mutex_unlock(app_loader_mutex);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    uflake_result_t result = app_lifecycle_resume(app, &current_app_id);
    uflake_mutex_unlock(app_loader_mutex);

    return result;
}

// ============================================================================
// APP QUERY
// ============================================================================

uflake_result_t app_loader_get_apps(app_descriptor_t **apps, uint32_t *count)
{
    if (!initialized || !apps || !count)
        return UFLAKE_ERROR_INVALID_PARAM;

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);

    *apps = app_registry;
    *count = app_count;

    uflake_mutex_unlock(app_loader_mutex);
    return UFLAKE_OK;
}

app_descriptor_t *app_loader_get_app(uint32_t app_id)
{
    if (!initialized)
        return NULL;

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);
    app_descriptor_t *app = app_loader_find_app_by_id(app_id);
    uflake_mutex_unlock(app_loader_mutex);

    return app;
}

uint32_t app_loader_find_by_name(const char *name)
{
    if (!initialized || !name)
        return 0;

    uflake_mutex_lock(app_loader_mutex, UINT32_MAX);

    for (uint32_t i = 0; i < app_count; i++)
    {
        if (strcmp(app_registry[i].manifest.name, name) == 0)
        {
            uint32_t app_id = app_registry[i].app_id;
            uflake_mutex_unlock(app_loader_mutex);
            return app_id;
        }
    }

    uflake_mutex_unlock(app_loader_mutex);
    return 0;
}

uint32_t app_loader_get_current(void)
{
    return current_app_id;
}

uint32_t app_loader_get_launcher(void)
{
    return launcher_app_id;
}

// ============================================================================
// FORCE EXIT (Button Combo Detection)
// ============================================================================

void app_loader_check_force_exit(bool right_pressed, bool back_pressed)
{
    bool both_pressed = right_pressed && back_pressed;

    if (both_pressed && !force_exit_buttons_pressed)
    {
        // Buttons just pressed
        force_exit_buttons_pressed = true;
        force_exit_press_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        ESP_LOGI(TAG, "Force exit combo detected, hold for %d ms", FORCE_EXIT_HOLD_TIME_MS);
    }
    else if (!both_pressed && force_exit_buttons_pressed)
    {
        // Buttons released before timeout
        force_exit_buttons_pressed = false;
        ESP_LOGI(TAG, "Force exit cancelled");
    }
    else if (both_pressed && force_exit_buttons_pressed)
    {
        // Check if held long enough
        uint64_t now = esp_timer_get_time() / 1000;
        if ((now - force_exit_press_time) >= FORCE_EXIT_HOLD_TIME_MS)
        {
            force_exit_buttons_pressed = false;
            ESP_LOGI(TAG, "Force exit triggered!");

            // Terminate current app
            if (current_app_id != 0)
            {
                app_loader_terminate(current_app_id);
            }
        }
    }
}
