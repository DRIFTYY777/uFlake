/**
 * @file appLoader_simple.h
 * @brief Simplified Application Loader API
 *
 * Simple app management system with:
 * - Direct app registration and launch
 * - Memory-aware launching
 * - Support for internal and external apps
 * - Minimal complexity
 */

#ifndef APP_LOADER_SIMPLE_H
#define APP_LOADER_SIMPLE_H

#include "app_types.h"
#include "kernel.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize app loader system
 * @return UFLAKE_OK on success
 */
uflake_result_t app_loader_init(void);

// ============================================================================
// APP REGISTRATION
// ============================================================================

/**
 * @brief Register an internal app
 * 
 * Call this during boot to register all internal (built-in) apps.
 * Apps are identified by unique ID assigned by loader.
 *
 * @param bundle App bundle (manifest + entry point)
 * @param is_launcher true if this is the launcher app (only one allowed)
 * @return App ID (>0) on success, 0 on error
 */
uint32_t app_loader_register_internal_app(const app_bundle_t *bundle, bool is_launcher);

/**
 * @brief Register external app from SD card
 * 
 * Scans SD card for .elf files and registers them.
 * Reads manifest.txt from app folder.
 *
 * @param path Path to app folder on SD card
 * @return App ID on success, 0 on error
 */
uint32_t app_loader_register_external_app(const char *path);

// ============================================================================
// APP QUERIES
// ============================================================================

/**
 * @brief Get total number of registered apps
 * @return App count
 */
uint32_t app_loader_get_app_count(void);

/**
 * @brief Get app by ID
 * @param app_id App ID
 * @return App descriptor or NULL if not found
 */
app_descriptor_t* app_loader_get_app(uint32_t app_id);

/**
 * @brief Get app by name
 * @param name App name
 * @return App ID or 0 if not found
 */
uint32_t app_loader_get_app_id_by_name(const char *name);

/**
 * @brief Get list of all apps
 * @param count Pointer to receive app count
 * @return Array of app descriptors (do not free)
 */
app_descriptor_t* app_loader_get_app_list(uint32_t *count);

/**
 * @brief Find launcher app
 * @return App ID of launcher, or 0 if not found
 */
uint32_t app_loader_get_launcher_id(void);

// ============================================================================
// APP LIFECYCLE
// ============================================================================

/**
 * @brief Launch an app
 * 
 * Checks memory, calls entry function, updates app state.
 * For GUI apps, calls app_main() in GUI context.
 * For non-GUI apps, creates background task.
 *
 * @param app_id App ID to launch
 * @return UFLAKE_OK on success
 *         UFLAKE_ERROR_MEMORY if insufficient RAM
 *         UFLAKE_ERROR_NOT_FOUND if app not found
 *         UFLAKE_ERROR if app launch failed
 */
uflake_result_t app_loader_launch_app(uint32_t app_id);

/**
 * @brief Launch app by name
 * @param name App name
 * @return UFLAKE_OK on success
 */
uflake_result_t app_loader_launch_app_by_name(const char *name);

/**
 * @brief Check if app can be launched (enough memory)
 * @param app_id App ID
 * @return true if app can be launched
 */
bool app_loader_can_launch(uint32_t app_id);

/**
 * @brief Terminate running app
 * @param app_id App ID
 * @return UFLAKE_OK on success
 */
uflake_result_t app_loader_terminate_app(uint32_t app_id);

/**
 * @brief Terminate all running apps
 */
void app_loader_terminate_all_apps(void);

// ============================================================================
// STATUS & DEBUGGING
// ============================================================================

/**
 * @brief Print info about all registered apps
 */
void app_loader_print_app_list(void);

/**
 * @brief Print info about specific app
 * @param app_id App ID
 */
void app_loader_print_app_info(uint32_t app_id);

/**
 * @brief Print memory usage of all apps
 */
void app_loader_print_memory_usage(void);

#ifdef __cplusplus
}
#endif

#endif // APP_LOADER_SIMPLE_H
