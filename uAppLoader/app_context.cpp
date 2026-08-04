/**
 * @file app_context.cpp
 * @brief App Context Manager - Implementation
 *
 * Context pool management with safe allocation/deallocation and memory tracking.
 */

#include "app_context.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "APP_CONTEXT";

// ============================================================================
// GLOBAL CONTEXT POOL
// ============================================================================

#define DEFAULT_MAX_CONTEXTS 16

typedef struct
{
    app_context_t *contexts;      // Array of contexts
    uint32_t max_contexts;        // Maximum contexts
    uint32_t allocated_count;     // Currently allocated
    uint32_t active_count;        // Currently active
    uflake_mutex_t *pool_mutex;   // Mutex for thread-safe access
    uint32_t next_context_id;     // For generating unique context IDs
} app_context_pool_t;

static app_context_pool_t context_pool = {
    .contexts = NULL,
    .max_contexts = 0,
    .allocated_count = 0,
    .active_count = 0,
    .pool_mutex = NULL,
    .next_context_id = 1
};

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t app_context_manager_init(uint32_t max_contexts)
{
    if (context_pool.contexts != NULL)
    {
        UFLAKE_LOGW(TAG, "Context manager already initialized");
        return UFLAKE_OK;
    }

    if (max_contexts == 0)
        max_contexts = DEFAULT_MAX_CONTEXTS;

    // Allocate context array
    context_pool.contexts = (app_context_t *)malloc(max_contexts * sizeof(app_context_t));
    if (context_pool.contexts == NULL)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate context pool");
        return UFLAKE_ERROR_MEMORY;
    }

    // Initialize all contexts as unused
    memset(context_pool.contexts, 0, max_contexts * sizeof(app_context_t));

    // Create mutex
    if (uflake_mutex_create(&context_pool.pool_mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create pool mutex");
        free(context_pool.contexts);
        context_pool.contexts = NULL;
        return UFLAKE_ERROR_MEMORY;
    }

    context_pool.max_contexts = max_contexts;
    context_pool.allocated_count = 0;
    context_pool.active_count = 0;
    context_pool.next_context_id = 1;

    UFLAKE_LOGI(TAG, "Context manager initialized with %u contexts", max_contexts);
    return UFLAKE_OK;
}

// ============================================================================
// CONTEXT ALLOCATION/DEALLOCATION
// ============================================================================

app_context_t *app_context_allocate(uint32_t app_id)
{
    if (context_pool.contexts == NULL)
    {
        UFLAKE_LOGE(TAG, "Context manager not initialized");
        return NULL;
    }

    if (uflake_mutex_lock(context_pool.pool_mutex, 100) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to acquire pool mutex");
        return NULL;
    }

    app_context_t *ctx = NULL;

    // Search for free context (is_active == false)
    for (uint32_t i = 0; i < context_pool.max_contexts; i++)
    {
        if (!context_pool.contexts[i].is_active)
        {
            ctx = &context_pool.contexts[i];
            break;
        }
    }

    if (ctx == NULL)
    {
        UFLAKE_LOGE(TAG, "No free context available (max: %u)", context_pool.max_contexts);
        uflake_mutex_unlock(context_pool.pool_mutex);
        return NULL;
    }

    // Initialize context
    memset(ctx, 0, sizeof(app_context_t));
    ctx->app_id = app_id;
    ctx->context_id = context_pool.next_context_id++;
    ctx->is_active = true;
    ctx->state = APP_STATE_STOPPED;
    ctx->launch_time_ms = esp_timer_get_time() / 1000; // Convert to ms

    context_pool.allocated_count++;
    context_pool.active_count++;

    uflake_mutex_unlock(context_pool.pool_mutex);

    UFLAKE_LOGI(TAG, "Allocated context ID %u for app %u", ctx->context_id, app_id);
    return ctx;
}

uflake_result_t app_context_release(app_context_t *ctx)
{
    if (ctx == NULL)
        return UFLAKE_ERROR_INVALID_PARAM;

    if (uflake_mutex_lock(context_pool.pool_mutex, 100) != UFLAKE_OK)
        return UFLAKE_ERROR_TIMEOUT;

    if (!ctx->is_active)
    {
        uflake_mutex_unlock(context_pool.pool_mutex);
        UFLAKE_LOGW(TAG, "Context already inactive");
        return UFLAKE_OK;
    }

    // Reset context
    uint32_t context_id = ctx->context_id;
    uint32_t app_id = ctx->app_id;

    memset(ctx, 0, sizeof(app_context_t));

    context_pool.allocated_count--;
    context_pool.active_count--;

    uflake_mutex_unlock(context_pool.pool_mutex);

    UFLAKE_LOGI(TAG, "Released context ID %u (app %u)", context_id, app_id);
    return UFLAKE_OK;
}

// ============================================================================
// CONTEXT LOOKUP
// ============================================================================

app_context_t *app_context_find_by_id(uint32_t app_id)
{
    if (context_pool.contexts == NULL)
        return NULL;

    for (uint32_t i = 0; i < context_pool.max_contexts; i++)
    {
        if (context_pool.contexts[i].is_active &&
            context_pool.contexts[i].app_id == app_id)
        {
            return &context_pool.contexts[i];
        }
    }
    return NULL;
}

app_context_t *app_context_find_by_handle(uint32_t context_id)
{
    if (context_pool.contexts == NULL)
        return NULL;

    for (uint32_t i = 0; i < context_pool.max_contexts; i++)
    {
        if (context_pool.contexts[i].is_active &&
            context_pool.contexts[i].context_id == context_id)
        {
            return &context_pool.contexts[i];
        }
    }
    return NULL;
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void app_context_activate(app_context_t *ctx)
{
    if (ctx == NULL)
        return;

    if (uflake_mutex_lock(context_pool.pool_mutex, 100) == UFLAKE_OK)
    {
        if (!ctx->is_active)
        {
            ctx->is_active = true;
            context_pool.active_count++;
        }
        uflake_mutex_unlock(context_pool.pool_mutex);
    }
}

void app_context_deactivate(app_context_t *ctx)
{
    if (ctx == NULL)
        return;

    if (uflake_mutex_lock(context_pool.pool_mutex, 100) == UFLAKE_OK)
    {
        if (ctx->is_active)
        {
            ctx->is_active = false;
            context_pool.active_count--;
        }
        uflake_mutex_unlock(context_pool.pool_mutex);
    }
}

void app_context_set_state(app_context_t *ctx, app_state_t state)
{
    if (ctx == NULL)
        return;
    ctx->state = state;
}

app_state_t app_context_get_state(const app_context_t *ctx)
{
    if (ctx == NULL)
        return APP_STATE_STOPPED;
    return ctx->state;
}

// ============================================================================
// MEMORY TRACKING
// ============================================================================

void app_context_record_allocation(app_context_t *ctx, uint32_t bytes)
{
    if (ctx == NULL)
        return;

    ctx->memory_allocated += bytes;
    if (ctx->memory_allocated > ctx->peak_memory)
    {
        ctx->peak_memory = ctx->memory_allocated;
    }
}

void app_context_record_deallocation(app_context_t *ctx, uint32_t bytes)
{
    if (ctx == NULL)
        return;

    if (ctx->memory_allocated >= bytes)
    {
        ctx->memory_allocated -= bytes;
    }
    else
    {
        UFLAKE_LOGW(TAG, "Memory deallocation exceeds allocated amount");
        ctx->memory_allocated = 0;
    }
}

uint32_t app_context_get_memory_used(const app_context_t *ctx)
{
    if (ctx == NULL)
        return 0;
    return ctx->memory_allocated;
}

uint32_t app_context_get_peak_memory(const app_context_t *ctx)
{
    if (ctx == NULL)
        return 0;
    return ctx->peak_memory;
}

// ============================================================================
// TIMING AND STATISTICS
// ============================================================================

void app_context_record_launch(app_context_t *ctx)
{
    if (ctx == NULL)
        return;

    ctx->launch_time_ms = esp_timer_get_time() / 1000;
    ctx->run_count++;
}

uint32_t app_context_get_runtime_ms(const app_context_t *ctx)
{
    if (ctx == NULL)
        return 0;

    uint32_t current_time_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (current_time_ms >= ctx->launch_time_ms)
    {
        return current_time_ms - ctx->launch_time_ms + ctx->run_duration_ms;
    }
    return ctx->run_duration_ms;
}

uint32_t app_context_get_run_count(const app_context_t *ctx)
{
    if (ctx == NULL)
        return 0;
    return ctx->run_count;
}

// ============================================================================
// GUI MANAGEMENT
// ============================================================================

void app_context_set_gui_window(app_context_t *ctx, void *window_handle)
{
    if (ctx == NULL)
        return;

    ctx->gui_window = window_handle;
    ctx->has_gui = (window_handle != NULL);
}

void *app_context_get_gui_window(const app_context_t *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->gui_window;
}

bool app_context_has_gui(const app_context_t *ctx)
{
    if (ctx == NULL)
        return false;
    return ctx->has_gui;
}

// ============================================================================
// STATISTICS AND CLEANUP
// ============================================================================

uint32_t app_context_get_active_count(void)
{
    return context_pool.active_count;
}

uint32_t app_context_get_allocated_count(void)
{
    return context_pool.allocated_count;
}

uint32_t app_context_get_total_memory(void)
{
    uint32_t total = 0;
    if (context_pool.contexts == NULL)
        return 0;

    for (uint32_t i = 0; i < context_pool.max_contexts; i++)
    {
        if (context_pool.contexts[i].is_active)
        {
            total += context_pool.contexts[i].memory_allocated;
        }
    }
    return total;
}

uflake_result_t app_context_clear_all(void)
{
    if (context_pool.contexts == NULL)
        return UFLAKE_OK;

    if (uflake_mutex_lock(context_pool.pool_mutex, 100) != UFLAKE_OK)
        return UFLAKE_ERROR_TIMEOUT;

    memset(context_pool.contexts, 0, context_pool.max_contexts * sizeof(app_context_t));
    context_pool.allocated_count = 0;
    context_pool.active_count = 0;

    uflake_mutex_unlock(context_pool.pool_mutex);

    UFLAKE_LOGI(TAG, "Cleared all contexts");
    return UFLAKE_OK;
}
