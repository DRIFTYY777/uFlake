/**
 * @file app_context.h
 * @brief App Context Manager - Per-app state and memory tracking
 *
 * Manages the lifecycle and memory tracking for each running app.
 * Provides a pool of reusable contexts for fast app switching without
 * the overhead of alloc/free on every launch/terminate cycle.
 */

#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include "kernel.h"
#include "appLoader.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Forward declarations
typedef struct app_context_t app_context_t;
typedef struct app_context_pool_t app_context_pool_t;

// ============================================================================
// APP CONTEXT - Per-app runtime state and memory tracking
// ============================================================================

typedef struct app_context_t
{
    // Basic info
    uint32_t app_id;            // Associated app ID
    uint32_t context_id;        // Unique context ID (handle)
    app_state_t state;          // Current app state

    // Memory tracking
    uint32_t memory_allocated;  // Total memory allocated for this app
    uint32_t peak_memory;       // Peak memory usage recorded
    uint32_t stack_base;        // Task stack base address
    uint32_t stack_size;        // Task stack size in bytes

    // Task management
    TaskHandle_t task_handle;   // FreeRTOS task handle
    bool is_active;             // Whether context is currently in use

    // Timing
    uint64_t launch_time_ms;    // Timestamp when app was launched
    uint32_t run_duration_ms;   // Total runtime so far
    uint32_t run_count;         // Number of times resumed

    // GUI state (if applicable)
    bool has_gui;               // Whether app uses GUI
    void *gui_window;           // Opaque handle to GUI window (uGui)

    // Cleanup hooks
    void (*on_pause)(uint32_t app_id);   // Called when app paused
    void (*on_resume)(uint32_t app_id);  // Called when app resumed
    void (*on_exit)(uint32_t app_id);    // Called when app exits

    // User data
    void *userdata;             // Opaque user data pointer
} app_context_t;

// ============================================================================
// CONTEXT MANAGER - Global context pool management
// ============================================================================

/**
 * @brief Initialize app context manager
 * @param max_contexts Maximum number of contexts to allocate
 * @return UFLAKE_OK on success
 */
uflake_result_t app_context_manager_init(uint32_t max_contexts);

/**
 * @brief Allocate a new app context
 * @param app_id App ID this context is for
 * @return Pointer to allocated context, NULL on failure
 */
app_context_t *app_context_allocate(uint32_t app_id);

/**
 * @brief Release app context back to pool for reuse
 * @param ctx Context to release
 * @return UFLAKE_OK on success
 */
uflake_result_t app_context_release(app_context_t *ctx);

/**
 * @brief Find context by app ID
 * @param app_id App ID to search for
 * @return Pointer to context, NULL if not found
 */
app_context_t *app_context_find_by_id(uint32_t app_id);

/**
 * @brief Find context by context ID/handle
 * @param context_id Context handle/ID
 * @return Pointer to context, NULL if not found
 */
app_context_t *app_context_find_by_handle(uint32_t context_id);

// ============================================================================
// CONTEXT STATE MANAGEMENT
// ============================================================================

/**
 * @brief Mark context as active/in use
 * @param ctx Context to activate
 */
void app_context_activate(app_context_t *ctx);

/**
 * @brief Mark context as inactive (paused/not running)
 * @param ctx Context to deactivate
 */
void app_context_deactivate(app_context_t *ctx);

/**
 * @brief Update context state
 * @param ctx Context
 * @param state New state
 */
void app_context_set_state(app_context_t *ctx, app_state_t state);

/**
 * @brief Get context state
 * @param ctx Context
 * @return Current state
 */
app_state_t app_context_get_state(const app_context_t *ctx);

// ============================================================================
// MEMORY TRACKING
// ============================================================================

/**
 * @brief Record memory allocation for app
 * @param ctx Context
 * @param bytes Number of bytes allocated
 */
void app_context_record_allocation(app_context_t *ctx, uint32_t bytes);

/**
 * @brief Record memory deallocation for app
 * @param ctx Context
 * @param bytes Number of bytes freed
 */
void app_context_record_deallocation(app_context_t *ctx, uint32_t bytes);

/**
 * @brief Get total memory used by app
 * @param ctx Context
 * @return Memory in bytes
 */
uint32_t app_context_get_memory_used(const app_context_t *ctx);

/**
 * @brief Get peak memory recorded for app
 * @param ctx Context
 * @return Memory in bytes
 */
uint32_t app_context_get_peak_memory(const app_context_t *ctx);

// ============================================================================
// TIMING AND STATISTICS
// ============================================================================

/**
 * @brief Record app launch
 * @param ctx Context
 */
void app_context_record_launch(app_context_t *ctx);

/**
 * @brief Get elapsed runtime for app
 * @param ctx Context
 * @return Runtime in milliseconds
 */
uint32_t app_context_get_runtime_ms(const app_context_t *ctx);

/**
 * @brief Get total times app was resumed
 * @param ctx Context
 * @return Resume count
 */
uint32_t app_context_get_run_count(const app_context_t *ctx);

// ============================================================================
// GUI MANAGEMENT
// ============================================================================

/**
 * @brief Set GUI window handle for app
 * @param ctx Context
 * @param window_handle Opaque GUI window handle from uGui
 */
void app_context_set_gui_window(app_context_t *ctx, void *window_handle);

/**
 * @brief Get GUI window handle for app
 * @param ctx Context
 * @return GUI window handle, or NULL if no GUI
 */
void *app_context_get_gui_window(const app_context_t *ctx);

/**
 * @brief Check if app has GUI
 * @param ctx Context
 * @return true if app uses GUI
 */
bool app_context_has_gui(const app_context_t *ctx);

// ============================================================================
// CLEANUP AND STATISTICS
// ============================================================================

/**
 * @brief Get total number of active contexts
 * @return Count of active contexts
 */
uint32_t app_context_get_active_count(void);

/**
 * @brief Get total number of allocated contexts
 * @return Count of allocated contexts
 */
uint32_t app_context_get_allocated_count(void);

/**
 * @brief Get total memory used by all contexts
 * @return Memory in bytes
 */
uint32_t app_context_get_total_memory(void);

/**
 * @brief Clear all contexts (cleanup - typically on shutdown)
 * @return UFLAKE_OK on success
 */
uflake_result_t app_context_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif // APP_CONTEXT_H
