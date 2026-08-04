/**
 * @file memory_aware_launcher.h
 * @brief Memory-Aware App Launcher - RAM checks before launch
 *
 * Provides helper functions for launching apps with memory constraints.
 * Integrates with app_context manager and memory manager for fast,
 * RAM-efficient app launching.
 */

#ifndef MEMORY_AWARE_LAUNCHER_H
#define MEMORY_AWARE_LAUNCHER_H

#include "kernel.h"
#include "appLoader.h"
#include "app_context.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// MEMORY PRESSURE MONITORING
// ============================================================================

/**
 * @brief Memory pressure thresholds (in bytes)
 *
 * Configure based on your system:
 * - Critical: < 64KB free
 * - Low: < 256KB free
 * - Normal: >= 256KB free
 */
typedef struct
{
    uint32_t critical_threshold;  // Bytes below = CRITICAL
    uint32_t low_threshold;       // Bytes below = LOW
} mem_pressure_config_t;

/**
 * @brief Initialize memory-aware launcher
 * @param config Memory pressure thresholds
 * @return UFLAKE_OK on success
 */
uflake_result_t mem_launcher_init(const mem_pressure_config_t *config);

/**
 * @brief Get current memory pressure level
 * @return mem_pressure_level_t indicating system memory status
 */
mem_pressure_level_t mem_launcher_get_pressure(void);

/**
 * @brief Check if sufficient RAM available for app
 * @param app Pointer to app descriptor
 * @param extra_headroom Extra bytes to reserve beyond app requirement (0 = none)
 * @return true if sufficient RAM, false otherwise
 */
bool mem_launcher_has_sufficient_ram(const app_descriptor_t *app, uint32_t extra_headroom);

// ============================================================================
// FAST APP LAUNCH/RESUME
// ============================================================================

/**
 * @brief Launch app with memory awareness
 * - Checks free RAM before launch
 * - Allocates context from pool
 * - Pauses less-important apps if needed
 * - Non-blocking execution
 *
 * @param app_id App ID to launch
 * @param allow_pause_others If true, can pause other apps to free RAM
 * @return UFLAKE_OK on success
 *         UFLAKE_ERROR_MEMORY if insufficient RAM and can't free more
 */
uflake_result_t mem_launcher_launch_app(uint32_t app_id, bool allow_pause_others);

/**
 * @brief Resume a paused app (faster than relaunch)
 * @param app_id App ID to resume
 * @return UFLAKE_OK on success
 */
uflake_result_t mem_launcher_resume_app(uint32_t app_id);

/**
 * @brief Pause running app to free memory
 * @param app_id App ID to pause
 * @return UFLAKE_OK on success
 */
uflake_result_t mem_launcher_pause_app(uint32_t app_id);

/**
 * @brief Terminate app and free all resources
 * @param app_id App ID to terminate
 * @return UFLAKE_OK on success
 */
uflake_result_t mem_launcher_terminate_app(uint32_t app_id);

// ============================================================================
// MEMORY RECLAMATION
// ============================================================================

/**
 * @brief Intelligently free RAM by pausing apps
 * - Pauses least-recently-used apps first
 * - Avoids pausing launcher or critical services
 *
 * @param target_free_bytes Target amount to free
 * @return Actual bytes freed
 */
uint32_t mem_launcher_free_ram(uint32_t target_free_bytes);

/**
 * @brief Get list of pauseable apps (candidates for RAM reclamation)
 * @param app_ids Pointer to receive array of app IDs
 * @param count Pointer to receive count
 * @return UFLAKE_OK on success
 */
uflake_result_t mem_launcher_get_pauseable_apps(uint32_t **app_ids, uint32_t *count);

/**
 * @brief Estimate RAM needed for app
 * @param app Pointer to app descriptor
 * @return Estimated RAM in bytes (includes overhead)
 */
uint32_t mem_launcher_estimate_ram_needed(const app_descriptor_t *app);

// ============================================================================
// STATISTICS AND MONITORING
// ============================================================================

/**
 * @brief Get free RAM available
 * @return Free RAM in bytes
 */
uint32_t mem_launcher_get_free_ram(void);

/**
 * @brief Get total used RAM by all apps
 * @return Used RAM in bytes
 */
uint32_t mem_launcher_get_used_ram(void);

/**
 * @brief Get memory usage for specific app
 * @param app_id App ID
 * @return Memory used in bytes, 0 if not found
 */
uint32_t mem_launcher_get_app_ram(uint32_t app_id);

/**
 * @brief Register callback for memory pressure changes
 * @param callback Function to call when pressure level changes
 */
typedef void (*mem_launcher_cb_t)(mem_pressure_level_t level);
void mem_launcher_register_callback(mem_launcher_cb_t callback);

// ============================================================================
// ELF APP SUPPORT (Stubs for future implementation)
// ============================================================================

/**
 * @brief Load ELF app dynamically from SD card
 * @param fap_path Path to .fap file
 * @param app_id App ID (for context tracking)
 * @return Handle to loaded ELF, NULL on failure
 */
void *mem_launcher_load_elf(const char *fap_path, uint32_t app_id);

/**
 * @brief Unload ELF app and free memory
 * @param elf_handle Handle returned by mem_launcher_load_elf()
 * @return UFLAKE_OK on success
 */
uflake_result_t mem_launcher_unload_elf(void *elf_handle);

/**
 * @brief Get entry point function from loaded ELF
 * @param elf_handle Handle returned by mem_launcher_load_elf()
 * @return Function pointer, NULL if not found
 */
app_entry_fn mem_launcher_get_elf_entry(void *elf_handle);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_AWARE_LAUNCHER_H
