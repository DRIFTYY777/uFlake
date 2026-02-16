#include "appService.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "SERVICE_MGR";

// Global state
static service_descriptor_t service_registry[MAX_SERVICES];
static uint32_t service_count = 0;
static uint32_t next_service_id = 1;
static uflake_mutex_t *service_mutex = NULL;
static bool initialized = false;

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

static service_descriptor_t *find_service_by_id(uint32_t service_id)
{
    for (uint32_t i = 0; i < service_count; i++)
    {
        if (service_registry[i].service_id == service_id)
            return &service_registry[i];
    }
    return NULL;
}

__attribute__((unused)) static const char *service_state_to_string(service_state_t state)
{
    switch (state)
    {
    case SERVICE_STATE_STOPPED:
        return "STOPPED";
    case SERVICE_STATE_STARTING:
        return "STARTING";
    case SERVICE_STATE_RUNNING:
        return "RUNNING";
    case SERVICE_STATE_STOPPING:
        return "STOPPING";
    case SERVICE_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

// Service task wrapper
static void service_task_wrapper(void *arg)
{
    uint32_t service_id = (uint32_t)(uintptr_t)arg;
    service_descriptor_t *service = find_service_by_id(service_id);

    if (!service)
    {
        UFLAKE_LOGE(TAG, "Invalid service ID %lu", service_id);
        vTaskDelete(NULL);
        return;
    }

    UFLAKE_LOGI(TAG, "Service task started: %s", service->manifest.name);

    // Service runs indefinitely until stopped
    while (service->state == SERVICE_STATE_RUNNING)
    {
        // Service logic runs in its start() callback which may loop
        // This task just keeps the service alive
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    UFLAKE_LOGI(TAG, "Service task exiting: %s", service->manifest.name);
    vTaskDelete(NULL);
}

// Check if service dependencies are met
static bool check_dependencies(const service_descriptor_t *service)
{
    for (uint32_t i = 0; i < MAX_SERVICES && service->manifest.dependencies[i] != 0; i++)
    {
        uint32_t dep_id = service->manifest.dependencies[i];
        service_descriptor_t *dep = find_service_by_id(dep_id);

        if (!dep || dep->state != SERVICE_STATE_RUNNING)
        {
            UFLAKE_LOGW(TAG, "Service %s dependency %lu not running", service->manifest.name, dep_id);
            return false;
        }
    }
    return true;
}

// ============================================================================
// SERVICE MANAGER INITIALIZATION
// ============================================================================

uflake_result_t service_manager_init(void)
{
    if (initialized)
    {
        UFLAKE_LOGW(TAG, "Service manager already initialized");
        return UFLAKE_OK;
    }

    // Create mutex using uFlake API
    if (uflake_mutex_create(&service_mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    service_count = 0;
    next_service_id = 1;
    memset(service_registry, 0, sizeof(service_registry));

    initialized = true;
    UFLAKE_LOGI(TAG, "Service manager initialized");
    return UFLAKE_OK;
}

uflake_result_t service_manager_start_all(void)
{
    if (!initialized)
    {
        UFLAKE_LOGE(TAG, "Service manager not initialized");
        return UFLAKE_ERROR;
    }

    UFLAKE_LOGI(TAG, "Starting all auto-start services");

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    // Start services in dependency order (simple approach: multiple passes)
    bool started_any;
    do
    {
        started_any = false;
        for (uint32_t i = 0; i < service_count; i++)
        {
            service_descriptor_t *service = &service_registry[i];

            if (service->manifest.auto_start &&
                service->state == SERVICE_STATE_STOPPED &&
                check_dependencies(service))
            {
                uflake_mutex_unlock(service_mutex);
                uflake_result_t result = service_start(service->service_id);
                uflake_mutex_lock(service_mutex, UINT32_MAX);

                if (result == UFLAKE_OK)
                {
                    started_any = true;
                    UFLAKE_LOGI(TAG, "Auto-started service: %s", service->manifest.name);
                }
                else if (service->manifest.critical)
                {
                    uflake_mutex_unlock(service_mutex);
                    UFLAKE_LOGE(TAG, "Critical service %s failed to start", service->manifest.name);
                    return UFLAKE_ERROR;
                }
            }
        }
    } while (started_any);

    uflake_mutex_unlock(service_mutex);

    UFLAKE_LOGI(TAG, "Auto-start complete");
    return UFLAKE_OK;
}

uflake_result_t service_manager_stop_all(void)
{
    if (!initialized)
        return UFLAKE_ERROR;

    UFLAKE_LOGI(TAG, "Stopping all services");

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    // Stop in reverse order
    for (int i = service_count - 1; i >= 0; i--)
    {
        if (service_registry[i].state == SERVICE_STATE_RUNNING)
        {
            uflake_mutex_unlock(service_mutex);
            service_stop(service_registry[i].service_id);
            uflake_mutex_lock(service_mutex, UINT32_MAX);
        }
    }

    uflake_mutex_unlock(service_mutex);

    UFLAKE_LOGI(TAG, "All services stopped");
    return UFLAKE_OK;
}

// ============================================================================
// SERVICE REGISTRATION
// ============================================================================

uint32_t service_register(const service_bundle_t *service_bundle)
{
    if (!initialized || !service_bundle || !service_bundle->manifest)
    {
        UFLAKE_LOGE(TAG, "Invalid service bundle");
        return 0;
    }

    if (service_count >= MAX_SERVICES)
    {
        UFLAKE_LOGE(TAG, "Service registry full (max %d services)", MAX_SERVICES);
        return 0;
    }

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    service_descriptor_t *service = &service_registry[service_count];
    memset(service, 0, sizeof(service_descriptor_t));

    service->service_id = next_service_id++;
    memcpy(&service->manifest, service_bundle->manifest, sizeof(service_manifest_t));
    service->state = SERVICE_STATE_STOPPED;
    service->task_handle = NULL;
    service->context = service_bundle->context;
    service->init = service_bundle->init;
    service->start = service_bundle->start;
    service->stop = service_bundle->stop;
    service->deinit = service_bundle->deinit;
    service->start_count = 0;
    service->crash_count = 0;
    service->last_start_time = 0;

    service_count++;

    UFLAKE_LOGI(TAG, "Registered service: %s v%s (ID: %lu, Type: %d)",
                service->manifest.name,
                service->manifest.version,
                service->service_id,
                service->manifest.type);

    uflake_mutex_unlock(service_mutex);
    return service->service_id;
}

uflake_result_t service_unregister(uint32_t service_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    service_descriptor_t *service = find_service_by_id(service_id);
    if (!service)
    {
        uflake_mutex_unlock(service_mutex);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    // Stop service if running
    if (service->state == SERVICE_STATE_RUNNING)
    {
        uflake_mutex_unlock(service_mutex);
        service_stop(service_id);
        uflake_mutex_lock(service_mutex, UINT32_MAX);
    }

    // Remove from registry (shift array)
    uint32_t index = service - service_registry;
    for (uint32_t i = index; i < service_count - 1; i++)
    {
        service_registry[i] = service_registry[i + 1];
    }
    service_count--;

    uflake_mutex_unlock(service_mutex);

    UFLAKE_LOGI(TAG, "Unregistered service ID %lu", service_id);
    return UFLAKE_OK;
}

// ============================================================================
// SERVICE LIFECYCLE
// ============================================================================

uflake_result_t service_start(uint32_t service_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    service_descriptor_t *service = find_service_by_id(service_id);
    if (!service)
    {
        uflake_mutex_unlock(service_mutex);
        UFLAKE_LOGE(TAG, "Service ID %lu not found", service_id);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    if (service->state == SERVICE_STATE_RUNNING)
    {
        uflake_mutex_unlock(service_mutex);
        UFLAKE_LOGW(TAG, "Service %s already running", service->manifest.name);
        return UFLAKE_OK;
    }

    // Check dependencies
    if (!check_dependencies(service))
    {
        uflake_mutex_unlock(service_mutex);
        UFLAKE_LOGE(TAG, "Service %s dependencies not met", service->manifest.name);
        return UFLAKE_ERROR;
    }

    service->state = SERVICE_STATE_STARTING;
    UFLAKE_LOGI(TAG, "Starting service: %s", service->manifest.name);

    uflake_mutex_unlock(service_mutex);

    // Call init callback
    if (service->init)
    {
        uflake_result_t result = service->init();
        if (result != UFLAKE_OK)
        {
            UFLAKE_LOGE(TAG, "Service %s init failed", service->manifest.name);
            uflake_mutex_lock(service_mutex, UINT32_MAX);
            service->state = SERVICE_STATE_ERROR;
            service->crash_count++;
            uflake_mutex_unlock(service_mutex);
            return result;
        }
    }

    // Call start callback
    if (service->start)
    {
        uflake_result_t result = service->start();
        if (result != UFLAKE_OK)
        {
            UFLAKE_LOGE(TAG, "Service %s start failed", service->manifest.name);
            uflake_mutex_lock(service_mutex, UINT32_MAX);
            service->state = SERVICE_STATE_ERROR;
            service->crash_count++;
            uflake_mutex_unlock(service_mutex);
            return result;
        }
    }

    // Only create a task if stack_size > 0
    if (service->manifest.stack_size > 0) {
        uint32_t stack_size = service->manifest.stack_size;
        // Map service priority to kernel priority
        process_priority_t kernel_priority = PROCESS_PRIORITY_NORMAL;
        if (service->manifest.priority >= 8)
            kernel_priority = PROCESS_PRIORITY_HIGH;
        else if (service->manifest.priority >= 5)
            kernel_priority = PROCESS_PRIORITY_NORMAL;
        else
            kernel_priority = PROCESS_PRIORITY_LOW;

        char task_name[32];
        snprintf(task_name, sizeof(task_name), "srv_%.27s", service->manifest.name);

        uint32_t pid;
        uflake_result_t result = uflake_process_create(
            task_name,
            service_task_wrapper,
            (void *)(uintptr_t)service_id,
            stack_size,
            kernel_priority,
            &pid);

        if (result != UFLAKE_OK)
        {
            UFLAKE_LOGE(TAG, "Failed to create task for service %s", service->manifest.name);
            uflake_mutex_lock(service_mutex, UINT32_MAX);
            service->state = SERVICE_STATE_ERROR;
            service->crash_count++;
            uflake_mutex_unlock(service_mutex);
            return result;
        }

        uflake_mutex_lock(service_mutex, UINT32_MAX);
        service->task_handle = xTaskGetHandle(task_name);
        service->state = SERVICE_STATE_RUNNING;
        service->start_count++;
        service->last_start_time = (uint32_t)(esp_timer_get_time() / 1000000);
        uflake_mutex_unlock(service_mutex);
        UFLAKE_LOGI(TAG, "Service %s started successfully", service->manifest.name);
        return UFLAKE_OK;
    } else {
        // No task needed, just mark as running
        uflake_mutex_lock(service_mutex, UINT32_MAX);
        service->task_handle = NULL;
        service->state = SERVICE_STATE_RUNNING;
        service->start_count++;
        service->last_start_time = (uint32_t)(esp_timer_get_time() / 1000000);
        uflake_mutex_unlock(service_mutex);
        UFLAKE_LOGI(TAG, "Service %s started (no task)", service->manifest.name);
        return UFLAKE_OK;
    }
}

uflake_result_t service_stop(uint32_t service_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    service_descriptor_t *service = find_service_by_id(service_id);
    if (!service)
    {
        uflake_mutex_unlock(service_mutex);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    if (service->state != SERVICE_STATE_RUNNING)
    {
        uflake_mutex_unlock(service_mutex);
        return UFLAKE_OK; // Already stopped
    }

    service->state = SERVICE_STATE_STOPPING;
    UFLAKE_LOGI(TAG, "Stopping service: %s", service->manifest.name);

    uflake_mutex_unlock(service_mutex);

    // Call stop callback
    if (service->stop)
    {
        service->stop();
    }

    // Call deinit callback
    if (service->deinit)
    {
        service->deinit();
    }

    // Delete task
    uflake_mutex_lock(service_mutex, UINT32_MAX);

    if (service->task_handle)
    {
        vTaskDelete(service->task_handle);
        service->task_handle = NULL;
    }

    service->state = SERVICE_STATE_STOPPED;

    uflake_mutex_unlock(service_mutex);

    UFLAKE_LOGI(TAG, "Service %s stopped", service->manifest.name);
    return UFLAKE_OK;
}

uflake_result_t service_restart(uint32_t service_id)
{
    if (!initialized)
        return UFLAKE_ERROR;

    UFLAKE_LOGI(TAG, "Restarting service ID %lu", service_id);

    uflake_result_t result = service_stop(service_id);
    if (result != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to stop service ID %lu", service_id);
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay between stop and start

    result = service_start(service_id);
    if (result != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to restart service ID %lu", service_id);
        return result;
    }

    return UFLAKE_OK;
}

// ============================================================================
// SERVICE QUERY
// ============================================================================

uflake_result_t service_get_all(service_descriptor_t **services, uint32_t *count)
{
    if (!initialized || !services || !count)
        return UFLAKE_ERROR_INVALID_PARAM;

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    *services = service_registry;
    *count = service_count;

    uflake_mutex_unlock(service_mutex);
    return UFLAKE_OK;
}

service_descriptor_t *service_get(uint32_t service_id)
{
    if (!initialized)
        return NULL;

    uflake_mutex_lock(service_mutex, UINT32_MAX);
    service_descriptor_t *service = find_service_by_id(service_id);
    uflake_mutex_unlock(service_mutex);

    return service;
}

uint32_t service_find_by_name(const char *name)
{
    if (!initialized || !name)
        return 0;

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    for (uint32_t i = 0; i < service_count; i++)
    {
        if (strcmp(service_registry[i].manifest.name, name) == 0)
        {
            uint32_t service_id = service_registry[i].service_id;
            uflake_mutex_unlock(service_mutex);
            return service_id;
        }
    }

    uflake_mutex_unlock(service_mutex);
    return 0;
}

bool service_is_running(uint32_t service_id)
{
    if (!initialized)
        return false;

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    service_descriptor_t *service = find_service_by_id(service_id);
    bool running = (service && service->state == SERVICE_STATE_RUNNING);

    uflake_mutex_unlock(service_mutex);
    return running;
}

void *service_get_context(uint32_t service_id)
{
    if (!initialized)
        return NULL;

    uflake_mutex_lock(service_mutex, UINT32_MAX);

    service_descriptor_t *service = find_service_by_id(service_id);
    void *context = service ? service->context : NULL;

    uflake_mutex_unlock(service_mutex);
    return context;
}
