#include "appLifecycle.h"
#include "appLoader.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_LIFECYCLE";

// External references to app registry (accessed from appLoader.c)
extern app_descriptor_t *app_loader_find_app_by_id(uint32_t app_id);
extern uflake_result_t app_loader_resume(uint32_t app_id);

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t app_lifecycle_init(void)
{
    ESP_LOGI(TAG, "App lifecycle manager initialized");
    return UFLAKE_OK;
}

// ============================================================================
// APP TASK WRAPPER
// ============================================================================

void app_task_wrapper(void *arg)
{
    uint32_t app_id = (uint32_t)(uintptr_t)arg;
    app_descriptor_t *app = app_loader_find_app_by_id(app_id);

    if (!app || !app->entry_point)
    {
        ESP_LOGE(TAG, "Invalid app or entry point for ID %lu", app_id);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Starting app: %s", app->manifest.name);

    app_entry_fn entry = (app_entry_fn)app->entry_point;
    entry(); // Call app_main()

    ESP_LOGI(TAG, "App %s exited", app->manifest.name);

    // Clean up - mark as stopped
    app->state = APP_STATE_STOPPED;
    app->task_handle = NULL;

    // If this was not the launcher, return to launcher
    if (!app->is_launcher)
    {
        uint32_t launcher_id = app_loader_get_launcher();
        if (launcher_id != 0)
        {
            // Resume launcher
            app_loader_resume(launcher_id);
        }
    }

    vTaskDelete(NULL);
}

// ============================================================================
// APP LIFECYCLE FUNCTIONS
// ============================================================================

uflake_result_t app_lifecycle_launch(app_descriptor_t *app,
                                     uint32_t launcher_id,
                                     uint32_t *current_app_id)
{
    if (!app || !current_app_id)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (app->state == APP_STATE_RUNNING)
    {
        ESP_LOGW(TAG, "App %s already running", app->manifest.name);
        return UFLAKE_OK;
    }

    // If launching a non-launcher app, pause the launcher
    if (!app->is_launcher && launcher_id != 0 && launcher_id != app->app_id)
    {
        app_descriptor_t *launcher = app_loader_find_app_by_id(launcher_id);
        if (launcher && launcher->state == APP_STATE_RUNNING)
        {
            ESP_LOGI(TAG, "Pausing launcher");
            launcher->state = APP_STATE_PAUSED;
            if (launcher->task_handle)
            {
                vTaskSuspend(launcher->task_handle);
            }
        }
    }

    // Create task for app using uFlake kernel
    uint32_t stack_size = app->manifest.stack_size > 0 ? app->manifest.stack_size : 4096;

    // Map app priority to kernel priority
    process_priority_t kernel_priority = PROCESS_PRIORITY_NORMAL;
    if (app->manifest.priority >= 8)
        kernel_priority = PROCESS_PRIORITY_HIGH;
    else if (app->manifest.priority >= 5)
        kernel_priority = PROCESS_PRIORITY_NORMAL;
    else
        kernel_priority = PROCESS_PRIORITY_LOW;

    uint32_t pid;
    uflake_result_t result = uflake_process_create(
        app->manifest.name,
        app_task_wrapper,
        (void *)(uintptr_t)app->app_id,
        stack_size,
        kernel_priority,
        &pid);

    if (result != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create task for app %s", app->manifest.name);
        return result;
    }

    // Get the task handle from FreeRTOS
    app->task_handle = xTaskGetHandle(app->manifest.name);

    app->state = APP_STATE_RUNNING;
    app->launch_count++;
    app->last_run_time = (uint32_t)(esp_timer_get_time() / 1000000);
    *current_app_id = app->app_id;

    ESP_LOGI(TAG, "Launched app: %s (ID: %lu)", app->manifest.name, app->app_id);
    return UFLAKE_OK;
}

uflake_result_t app_lifecycle_terminate(app_descriptor_t *app,
                                        uint32_t launcher_id,
                                        uint32_t *current_app_id)
{
    if (!app || !current_app_id)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (app->state != APP_STATE_RUNNING && app->state != APP_STATE_PAUSED)
    {
        return UFLAKE_OK; // Already stopped
    }

    ESP_LOGI(TAG, "Terminating app: %s", app->manifest.name);

    if (app->task_handle)
    {
        // Use uFlake kernel to terminate (it will clean up properly)
        vTaskDelete(app->task_handle);
        app->task_handle = NULL;
    }

    app->state = APP_STATE_STOPPED;

    if (*current_app_id == app->app_id)
        *current_app_id = 0;

    // If this was not the launcher, return to launcher
    if (!app->is_launcher && launcher_id != 0)
    {
        app_loader_resume(launcher_id);
    }

    return UFLAKE_OK;
}

uflake_result_t app_lifecycle_pause(app_descriptor_t *app)
{
    if (!app)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (app->state != APP_STATE_RUNNING)
    {
        return UFLAKE_OK;
    }

    ESP_LOGI(TAG, "Pausing app: %s", app->manifest.name);

    if (app->task_handle)
    {
        vTaskSuspend(app->task_handle);
    }

    app->state = APP_STATE_PAUSED;

    return UFLAKE_OK;
}

uflake_result_t app_lifecycle_resume(app_descriptor_t *app,
                                     uint32_t *current_app_id)
{
    if (!app || !current_app_id)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (app->state != APP_STATE_PAUSED)
    {
        return UFLAKE_OK;
    }

    ESP_LOGI(TAG, "Resuming app: %s", app->manifest.name);

    if (app->task_handle)
    {
        vTaskResume(app->task_handle);
    }

    app->state = APP_STATE_RUNNING;
    *current_app_id = app->app_id;

    return UFLAKE_OK;
}
