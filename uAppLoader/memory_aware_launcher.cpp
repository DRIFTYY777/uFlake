/**
 * @file memory_aware_launcher.cpp
 * @brief Memory-Aware App Launcher - Implementation
 *
 * Handles RAM-aware app launching with intelligent memory reclamation.
 */

#include "memory_aware_launcher.h"
#include "memory_manager.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "MEM_LAUNCHER";

// ============================================================================
// MEMORY PRESSURE CONFIGURATION
// ============================================================================

typedef struct
{
    mem_pressure_config_t config;
    uflake_mutex_t *mutex;
    mem_launcher_cb_t pressure_callback;
    mem_pressure_level_t last_pressure;
} mem_launcher_state_t;

static mem_launcher_state_t launcher_state = {
    .config = {
        .critical_threshold = 64 * 1024,   // 64KB
        .low_threshold = 256 * 1024,       // 256KB
    },
    .mutex = NULL,
    .pressure_callback = NULL,
    .last_pressure = MEM_PRESSURE_NORMAL
};

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t mem_launcher_init(const mem_pressure_config_t *config)
{
    if (launcher_state.mutex != NULL)
    {
        UFLAKE_LOGW(TAG, "Memory launcher already initialized");
        return UFLAKE_OK;
    }

    // Create mutex
    if (uflake_mutex_create(&launcher_state.mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create launcher mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    // Use provided config or defaults
    if (config != NULL)
    {
        launcher_state.config = *config;
    }

    UFLAKE_LOGI(TAG, "Memory launcher initialized");
    UFLAKE_LOGI(TAG, "  Critical: %u bytes, Low: %u bytes",
               launcher_state.config.critical_threshold,
               launcher_state.config.low_threshold);

    return UFLAKE_OK;
}

// ============================================================================
// MEMORY PRESSURE DETECTION
// ============================================================================

static void update_memory_pressure(void)
{
    uint32_t free_ram = mem_launcher_get_free_ram();
    mem_pressure_level_t new_pressure = MEM_PRESSURE_NORMAL;

    if (free_ram < launcher_state.config.critical_threshold)
    {
        new_pressure = MEM_PRESSURE_CRITICAL;
    }
    else if (free_ram < launcher_state.config.low_threshold)
    {
        new_pressure = MEM_PRESSURE_LOW;
    }

    if (new_pressure != launcher_state.last_pressure)
    {
        launcher_state.last_pressure = new_pressure;

        UFLAKE_LOGI(TAG, "Memory pressure changed: %d (free: %u bytes)",
                   new_pressure, free_ram);

        if (launcher_state.pressure_callback != NULL)
        {
            launcher_state.pressure_callback(new_pressure);
        }
    }
}

mem_pressure_level_t mem_launcher_get_pressure(void)
{
    if (launcher_state.mutex != NULL)
    {
        uflake_mutex_lock(launcher_state.mutex, 50);
        update_memory_pressure();
        mem_pressure_level_t pressure = launcher_state.last_pressure;
        uflake_mutex_unlock(launcher_state.mutex);
        return pressure;
    }

    // Fallback if not initialized
    uint32_t free_ram = mem_launcher_get_free_ram();
    if (free_ram < launcher_state.config.critical_threshold)
        return MEM_PRESSURE_CRITICAL;
    if (free_ram < launcher_state.config.low_threshold)
        return MEM_PRESSURE_LOW;
    return MEM_PRESSURE_NORMAL;
}

// ============================================================================
// RAM AVAILABILITY CHECKS
// ============================================================================

uint32_t mem_launcher_estimate_ram_needed(const app_descriptor_t *app)
{
    if (app == NULL)
        return 0;

    uint32_t estimated = 0;

    // Manifest specifies minimum
    if (app->manifest.min_ram_bytes > 0)
    {
        estimated = app->manifest.min_ram_bytes;
    }

    // Stack size
    if (app->manifest.stack_size > 0)
    {
        estimated += app->manifest.stack_size;
    }
    else
    {
        // Default stack: 4KB
        estimated += 4096;
    }

    // GUI overhead (if needed)
    if (app->manifest.requires_gui)
    {
        // Rough estimate: 8KB for LVGL window + buffers
        estimated += 8192;
    }

    // Context overhead: ~512 bytes
    estimated += 512;

    // Safety margin: +10%
    estimated = (estimated * 11) / 10;

    return estimated;
}

bool mem_launcher_has_sufficient_ram(const app_descriptor_t *app, uint32_t extra_headroom)
{
    if (app == NULL)
        return false;

    uint32_t needed = mem_launcher_estimate_ram_needed(app) + extra_headroom;
    uint32_t available = mem_launcher_get_free_ram();

    bool sufficient = (available >= needed);

    if (!sufficient)
    {
        UFLAKE_LOGW(TAG, "Insufficient RAM: need %u, have %u", needed, available);
    }

    return sufficient;
}

// ============================================================================
// APP LAUNCH/RESUME/PAUSE/TERMINATE
// ============================================================================

uflake_result_t mem_launcher_launch_app(uint32_t app_id, bool allow_pause_others)
{
    app_descriptor_t *app = app_loader_get_app(app_id);
    if (app == NULL)
    {
        UFLAKE_LOGE(TAG, "App ID %u not found", app_id);
        return UFLAKE_ERROR_NOT_FOUND;
    }

    // Check if we have sufficient RAM
    if (!mem_launcher_has_sufficient_ram(app, 0))
    {
        if (allow_pause_others)
        {
            // Try to free RAM by pausing apps
            uint32_t needed = mem_launcher_estimate_ram_needed(app);
            uint32_t freed = mem_launcher_free_ram(needed);

            UFLAKE_LOGI(TAG, "Freed %u bytes, need %u bytes", freed, needed);

            // Check again
            if (!mem_launcher_has_sufficient_ram(app, 0))
            {
                UFLAKE_LOGE(TAG, "Still insufficient RAM after freeing");
                return UFLAKE_ERROR_MEMORY;
            }
        }
        else
        {
            return UFLAKE_ERROR_MEMORY;
        }
    }

    // Allocate context
    app_context_t *ctx = app_context_allocate(app_id);
    if (ctx == NULL)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate app context");
        return UFLAKE_ERROR_MEMORY;
    }

    // Record launch in context
    app_context_record_launch(ctx);
    app_context_set_state(ctx, APP_STATE_RUNNING);

    // Store context handle in app descriptor
    app->context_handle = ctx->context_id;

    // Update memory pressure
    update_memory_pressure();

    // Launch via appLoader
    uflake_result_t result = app_loader_launch(app_id);
    if (result != UFLAKE_OK)
    {
        app_context_release(ctx);
        UFLAKE_LOGE(TAG, "Failed to launch app %u: %d", app_id, result);
        return result;
    }

    UFLAKE_LOGI(TAG, "App %u launched successfully (context %u)", app_id, ctx->context_id);
    return UFLAKE_OK;
}

uflake_result_t mem_launcher_resume_app(uint32_t app_id)
{
    app_context_t *ctx = app_context_find_by_id(app_id);
    if (ctx == NULL)
    {
        UFLAKE_LOGW(TAG, "No context for app %u, launching instead", app_id);
        return mem_launcher_launch_app(app_id, false);
    }

    // Resume via appLoader
    uflake_result_t result = app_loader_resume(app_id);
    if (result != UFLAKE_OK)
        return result;

    // Update context
    app_context_set_state(ctx, APP_STATE_RUNNING);
    app_context_record_launch(ctx);

    update_memory_pressure();

    UFLAKE_LOGI(TAG, "App %u resumed", app_id);
    return UFLAKE_OK;
}

uflake_result_t mem_launcher_pause_app(uint32_t app_id)
{
    app_context_t *ctx = app_context_find_by_id(app_id);

    // Pause via appLoader
    uflake_result_t result = app_loader_pause(app_id);
    if (result != UFLAKE_OK)
        return result;

    // Update context
    if (ctx != NULL)
    {
        app_context_set_state(ctx, APP_STATE_PAUSED);
    }

    update_memory_pressure();

    UFLAKE_LOGI(TAG, "App %u paused", app_id);
    return UFLAKE_OK;
}

uflake_result_t mem_launcher_terminate_app(uint32_t app_id)
{
    app_context_t *ctx = app_context_find_by_id(app_id);

    // Terminate via appLoader
    uflake_result_t result = app_loader_terminate(app_id);

    // Release context
    if (ctx != NULL)
    {
        app_context_release(ctx);
    }

    update_memory_pressure();

    UFLAKE_LOGI(TAG, "App %u terminated", app_id);
    return result;
}

// ============================================================================
// MEMORY RECLAMATION
// ============================================================================

uint32_t mem_launcher_free_ram(uint32_t target_free_bytes)
{
    uint32_t freed = 0;
    uint32_t initial_free = mem_launcher_get_free_ram();

    // Get pauseable apps (skip launcher, already running, etc.)
    uint32_t *pauseable_ids = NULL;
    uint32_t count = 0;

    if (mem_launcher_get_pauseable_apps(&pauseable_ids, &count) != UFLAKE_OK)
    {
        UFLAKE_LOGW(TAG, "Could not get pauseable apps list");
        return 0;
    }

    // Pause apps until we have enough free RAM
    for (uint32_t i = 0; i < count && freed < target_free_bytes; i++)
    {
        uint32_t app_id = pauseable_ids[i];
        UFLAKE_LOGI(TAG, "Pausing app %u to free RAM", app_id);

        if (mem_launcher_pause_app(app_id) == UFLAKE_OK)
        {
            uint32_t current_free = mem_launcher_get_free_ram();
            if (current_free > initial_free)
            {
                freed = current_free - initial_free;
                UFLAKE_LOGI(TAG, "Freed %u bytes so far", freed);
            }
        }
    }

    if (pauseable_ids != NULL)
    {
        free(pauseable_ids);
    }

    return freed;
}

uflake_result_t mem_launcher_get_pauseable_apps(uint32_t **app_ids, uint32_t *count)
{
    // TODO: Implement - get list of running apps that can be paused
    // (exclude launcher, critical services, etc.)

    if (app_ids == NULL || count == NULL)
        return UFLAKE_ERROR_INVALID_PARAM;

    *app_ids = NULL;
    *count = 0;

    // Stub: return empty list
    UFLAKE_LOGW(TAG, "mem_launcher_get_pauseable_apps not implemented yet");
    return UFLAKE_OK;
}

// ============================================================================
// MEMORY STATISTICS
// ============================================================================

uint32_t mem_launcher_get_free_ram(void)
{
    // Use memory manager to get free RAM
    // This is a stub - actual implementation would call memory_manager functions
    // For now, estimate based on available heap
    return esp_get_free_heap_size();
}

uint32_t mem_launcher_get_used_ram(void)
{
    return app_context_get_total_memory();
}

uint32_t mem_launcher_get_app_ram(uint32_t app_id)
{
    app_context_t *ctx = app_context_find_by_id(app_id);
    if (ctx == NULL)
        return 0;

    return app_context_get_memory_used(ctx);
}

void mem_launcher_register_callback(mem_launcher_cb_t callback)
{
    if (launcher_state.mutex != NULL)
    {
        uflake_mutex_lock(launcher_state.mutex, 50);
        launcher_state.pressure_callback = callback;
        uflake_mutex_unlock(launcher_state.mutex);
    }
}

// ============================================================================
// ELF APP SUPPORT (Stubs)
// ============================================================================

void *mem_launcher_load_elf(const char *fap_path, uint32_t app_id)
{
    // TODO: Implement ELF loading from SD card
    UFLAKE_LOGW(TAG, "ELF loading not implemented yet: %s", fap_path);
    (void)app_id;
    return NULL;
}

uflake_result_t mem_launcher_unload_elf(void *elf_handle)
{
    // TODO: Implement ELF unloading
    (void)elf_handle;
    UFLAKE_LOGW(TAG, "ELF unloading not implemented yet");
    return UFLAKE_OK;
}

app_entry_fn mem_launcher_get_elf_entry(void *elf_handle)
{
    // TODO: Implement ELF entry point lookup
    (void)elf_handle;
    return NULL;
}
