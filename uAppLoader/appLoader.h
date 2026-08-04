/**
 * @file appLoader.h
 * @brief uFlake Application Loader - Main API
 *
 * The App Loader provides a modular system for managing apps and services:
 *
 * - appLoader: Core app registration and management
 * - appService: Background services (input, power, battery, etc.)
 * - appLifecycle: App launch, pause, resume, terminate
 * - appManifest: Manifest parsing and validation
 *
 * Apps are GUI applications that users can launch and interact with.
 * Services are background tasks (no GUI) that provide system functionality.
 */

#ifndef APP_LOADER_H
#define APP_LOADER_H

#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define APP_NAME_MAX_LEN 32
#define APP_VERSION_MAX_LEN 16
#define APP_AUTHOR_MAX_LEN 64
#define APP_DESC_MAX_LEN 128
#define APP_ICON_MAX_LEN 32
#define APP_PATH_MAX_LEN 128
#define MAX_APPS 32

#define INTERNAL_APPS_FOLDER "/internal_apps"
#define EXTERNAL_APPS_FOLDER "/sdcard/apps"
#define APP_MANIFEST_FILENAME "manifest.txt"

    // #define FORCE_EXIT_HOLD_TIME_MS 2000 // Hold Right+Back for 2 seconds to force exit

    // App types
    typedef enum
    {
        APP_TYPE_INTERNAL = 0, // Built-in apps in flash
        APP_TYPE_EXTERNAL,     // Apps loaded from SD card (.fap files)
        APP_TYPE_LAUNCHER,     // Launcher/Home screen (special app)
        APP_TYPE_SERVICE       // Background service (no UI, auto-start)
    } app_type_t;

    // App subtypes - distinguishes GUI vs non-GUI vs ELF apps
    typedef enum
    {
        APP_SUBTYPE_NON_GUI = 0,  // Background service, no display
        APP_SUBTYPE_GUI_NATIVE,   // GUI app using LVGL directly (internal/native)
        APP_SUBTYPE_GUI_ELF       // External ELF GUI app loaded from SD card
    } app_subtype_t;

    // Memory pressure levels
    typedef enum
    {
        MEM_PRESSURE_NORMAL = 0,
        MEM_PRESSURE_LOW,
        MEM_PRESSURE_CRITICAL
    } mem_pressure_level_t;

    // App location (where app code lives)
    typedef enum
    {
        APP_LOCATION_INTERNAL = 0, // Linked into firmware
        APP_LOCATION_EXTERNAL      // Loaded from storage
    } app_location_t;

    // App state
    typedef enum
    {
        APP_STATE_STOPPED = 0,
        APP_STATE_RUNNING,
        APP_STATE_PAUSED
    } app_state_t;

    // App manifest - describes the app (like Flipper's FAP manifest)
    // Read from manifest.txt file in app folder
    typedef struct app_manifest_t
    {
        char name[APP_NAME_MAX_LEN];        // Display name
        char version[APP_VERSION_MAX_LEN];  // Version string
        char author[APP_AUTHOR_MAX_LEN];    // Author name
        char description[APP_DESC_MAX_LEN]; // Short description
        char icon[APP_ICON_MAX_LEN];        // Icon filename
        app_type_t type;                    // App type
        uint32_t stack_size;                // Task stack size (0 = default)
        uint32_t priority;                  // Task priority (0 = default)
        bool requires_gui;                  // True if needs display
        bool requires_sdcard;               // True if needs SD card
        bool requires_network;              // True if needs WiFi/BT
        // NEW FIELDS (at end for backward compatibility)
        app_subtype_t subtype;              // App subtype (GUI vs non-GUI) - default: APP_SUBTYPE_GUI_NATIVE
        uint32_t min_ram_bytes;             // Minimum RAM needed - default: 0 (no minimum)
    } app_manifest_t;

    // Helper: Initialize manifest with required fields, new fields default
    // Use this for backward compatibility: APP_MANIFEST_INIT("MyApp", "1.0", ...)
    #define APP_MANIFEST_INIT(name_str, version_str, author_str, type_val, has_gui, has_sdcard, has_net) \
        { \
            .name = name_str, \
            .version = version_str, \
            .author = author_str, \
            .description = "", \
            .icon = "", \
            .type = type_val, \
            .stack_size = 4096, \
            .priority = 5, \
            .requires_gui = has_gui, \
            .requires_sdcard = has_sdcard, \
            .requires_network = has_net, \
            .subtype = APP_SUBTYPE_GUI_NATIVE, \
            .min_ram_bytes = 0 \
        }

    // App descriptor - runtime info about app
    typedef struct app_descriptor_t
    {
        uint32_t app_id;             // Unique ID
        app_manifest_t manifest;     // App metadata
        app_location_t location;     // Where code lives
        char path[APP_PATH_MAX_LEN]; // Folder path
        void *entry_point;           // app_main function pointer
        void *elf_handle;            // Handle for external apps
        app_state_t state;           // Current state
        TaskHandle_t task_handle;    // FreeRTOS task handle
        bool is_launcher;            // True if this is launcher
        uint32_t launch_count;       // Times launched
        uint32_t last_run_time;      // Last launch timestamp
        // Memory tracking (NEW)
        uint32_t memory_used;        // Current memory used by app
        uint32_t peak_memory;        // Peak memory usage
        uint32_t context_handle;     // Handle to app context (for quick lookup)
    } app_descriptor_t;

    // App entry point signature - simple function that apps must implement
    typedef void (*app_entry_fn)(void);

    // App bundle - combines manifest and entry point in one struct
    typedef struct
    {
        const app_manifest_t *manifest;
        app_entry_fn entry_point;
        bool is_launcher;
    } app_bundle_t;

    // ============================================================================
    // APP LOADER INITIALIZATION
    // ============================================================================

    /**
     * @brief Initialize the app loader system
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_loader_init(void);

    /**
     * @brief Discover all external apps (on SD card)
     * Scans EXTERNAL_APPS_FOLDER for .fap files and registers apps
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_loader_scan_external_apps(void);

    // ============================================================================
    // APP REGISTRATION (Called by app loader during discovery)
    // ============================================================================

    /**
     * @brief Register an internal app (simplified - one line registration)
     * @param app_bundle Pointer to app's bundle structure (contains manifest + entry point)
     * @return App ID on success, 0 on failure
     *
     * Example usage:
     *   app_loader_register(&my_app_bundle);
     */
    uint32_t app_loader_register(const app_bundle_t *app_bundle);

    /**
     * @brief Register an internal app (full control version)
     * @param manifest App manifest structure
     * @param entry_point Pointer to app_main function
     * @param is_launcher True if this is the launcher app
     * @return App ID on success, 0 on failure
     */
    uint32_t app_loader_register_internal(const app_manifest_t *manifest,
                                          app_entry_fn entry_point,
                                          bool is_launcher);

    /**
     * @brief Register an external app (.fap file)
     * @param fap_path Path to .fap file
     * @return App ID on success, 0 on failure
     */
    uint32_t app_loader_register_external(const char *fap_path);

    // ============================================================================
    // APP LIFECYCLE (Called by launcher to control apps)
    // ============================================================================

    /**
     * @brief Launch an app (creates task and calls app_main)
     * @param app_id App ID to launch
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_loader_launch(uint32_t app_id);

    /**
     * @brief Launch an app with memory checks (NEW - memory-aware)
     * @param app_id App ID to launch
     * @param min_free_ram Minimum free RAM required (0 = use manifest default)
     * @return UFLAKE_OK on success, UFLAKE_ERROR_MEMORY if insufficient RAM
     */
    uflake_result_t app_loader_launch_with_memory_check(uint32_t app_id, uint32_t min_free_ram);

    /**
     * @brief Terminate a running app
     * @param app_id App ID to terminate
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_loader_terminate(uint32_t app_id);

    /**
     * @brief Pause an app (suspend task, keeps state) - fast app switching
     * @param app_id App ID to pause
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_loader_pause(uint32_t app_id);

    /**
     * @brief Resume a paused app - faster than relaunch
     * @param app_id App ID to resume
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_loader_resume(uint32_t app_id);

    // ============================================================================
    // APP QUERY (Used by launcher to get list of apps)
    // ============================================================================

    /**
     * @brief Get list of all registered apps
     * @param apps Pointer to receive array of app descriptors
     * @param count Pointer to receive number of apps
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_loader_get_apps(app_descriptor_t **apps, uint32_t *count);

    /**
     * @brief Get app descriptor by ID
     * @param app_id App ID
     * @return Pointer to app descriptor, NULL if not found
     */
    app_descriptor_t *app_loader_get_app(uint32_t app_id);

    /**
     * @brief Find app by name
     * @param name App name
     * @return App ID, or 0 if not found
     */
    uint32_t app_loader_find_by_name(const char *name);

    /**
     * @brief Get currently running app
     * @return App ID of foreground app, or 0 if none
     */
    uint32_t app_loader_get_current(void);

    /**
     * @brief Get launcher app ID
     * @return App ID of launcher, or 0 if not registered
     */
    uint32_t app_loader_get_launcher(void);

    // ============================================================================
    // MEMORY MANAGEMENT (NEW)
    // ============================================================================

    /**
     * @brief Get current free RAM available
     * @return Free RAM in bytes
     */
    uint32_t app_loader_get_free_ram(void);

    /**
     * @brief Get current memory pressure level
     * @return mem_pressure_level_t indicating memory pressure
     */
    mem_pressure_level_t app_loader_get_memory_pressure(void);

    /**
     * @brief Register callback for memory pressure events
     * @param callback Function to call on memory pressure changes
     */
    typedef void (*app_loader_mem_pressure_cb_t)(mem_pressure_level_t level);
    void app_loader_register_memory_callback(app_loader_mem_pressure_cb_t callback);

    /**
     * @brief Get app context handle (for advanced use)
     * @param app_id App ID
     * @return Context handle, or 0 if not found
     */
    uint32_t app_loader_get_context_handle(uint32_t app_id);

    // ============================================================================
    // FORCE EXIT (Button Combo Detection)
    // ============================================================================

    /**
     * @brief Check for force exit button combo (Right + Back held 2s)
     * Call this from button ISR or main loop
     * @param right_pressed True if Right button pressed
     * @param back_pressed True if Back button pressed
     */
    void app_loader_check_force_exit(bool right_pressed, bool back_pressed);

    // ============================================================================
    // MANIFEST PARSING
    // ============================================================================

    /**
     * @brief Parse manifest file
     * @param path Path to manifest.txt file
     * @param manifest Pointer to manifest structure to fill
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_manifest_parse(const char *path, app_manifest_t *manifest);

#ifdef __cplusplus
}
#endif

#endif // APP_LOADER_H
