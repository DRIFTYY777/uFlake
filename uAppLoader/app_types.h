/**
 * @file app_types.h
 * @brief Application type definitions
 *
 * Defines app descriptors, manifests, and type information for the simple app loader.
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H

#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// CONSTANTS
// ============================================================================

#define APP_NAME_MAX 32
#define APP_VERSION_MAX 16
#define APP_AUTHOR_MAX 64
#define APP_DESC_MAX 128
#define APP_ICON_MAX 32
#define APP_PATH_MAX 256
#define MAX_APPS 32

// ============================================================================
// APP TYPES
// ============================================================================

/** App category */
typedef enum
{
    APP_CAT_SYSTEM = 0,    // System apps (launcher, settings)
    APP_CAT_UTILITY,       // Utilities
    APP_CAT_GAME,          // Games
    APP_CAT_DEMO,          // Demos
    APP_CAT_SERVICE        // Background services
} app_category_t;

/** App subtype (GUI vs non-GUI) */
typedef enum
{
    APP_SUBTYPE_NON_GUI = 0,   // No display/input
    APP_SUBTYPE_GUI            // Has display/input
} app_subtype_t;

/** Where app is stored */
typedef enum
{
    APP_LOCATION_INTERNAL = 0,  // Linked into firmware
    APP_LOCATION_EXTERNAL       // Loaded from SD card
} app_location_t;

/** App state */
typedef enum
{
    APP_STATE_STOPPED = 0,
    APP_STATE_RUNNING,
    APP_STATE_EXITED
} app_state_t;

// ============================================================================
// APP ENTRY POINTS
// ============================================================================

/**
 * @brief App entry function (main function)
 */
typedef void (*app_entry_fn)(void);

/**
 * @brief App result callback
 * 
 * Called when app exits. Return value from app_main().
 */
typedef void (*app_result_callback_t)(uint32_t result, void *userdata);

// ============================================================================
// APP MANIFEST
// ============================================================================

/**
 * @brief App manifest - metadata about the app
 * 
 * For internal apps: embedded in app code
 * For external apps: loaded from manifest.txt file
 */
typedef struct
{
    char name[APP_NAME_MAX];           // Display name
    char version[APP_VERSION_MAX];     // Version string
    char author[APP_AUTHOR_MAX];       // Author name
    char description[APP_DESC_MAX];    // Short description
    char icon[APP_ICON_MAX];           // Icon filename
    app_category_t category;           // App category
    app_subtype_t subtype;             // GUI or non-GUI
    uint32_t stack_size;               // Stack size (0 = default 8KB)
    uint32_t priority;                 // Task priority (0 = default)
    uint32_t min_ram_bytes;            // Minimum RAM required
    bool requires_sdcard;              // Needs SD card access
    bool requires_network;             // Needs WiFi/BT
} app_manifest_t;

// ============================================================================
// APP DESCRIPTOR
// ============================================================================

/**
 * @brief Complete app descriptor with runtime info
 */
typedef struct
{
    uint32_t app_id;                   // Unique app ID
    app_manifest_t manifest;           // App metadata
    app_location_t location;           // Internal or external
    char path[APP_PATH_MAX];           // Path to app (external) or name (internal)
    app_entry_fn entry_point;          // Entry function (NULL for external ELF)
    app_state_t state;                 // Current state
    uint32_t task_id;                  // Task ID when running (0 if not running)
    uint32_t ram_used;                 // RAM used by app instance
    bool is_launcher;                  // Special launcher app
} app_descriptor_t;

// ============================================================================
// APP BUNDLE (for internal apps)
// ============================================================================

/**
 * @brief Bundle of app metadata and entry point
 * 
 * Used to register internal apps in code.
 * Example:
 * 
 *     const app_bundle_t my_app = {
 *         .manifest = { .name = "My App", ... },
 *         .entry_point = my_app_main,
 *         .is_launcher = false
 *     };
 */
typedef struct
{
    app_manifest_t manifest;
    app_entry_fn entry_point;
    bool is_launcher;
} app_bundle_t;

#ifdef __cplusplus
}
#endif

#endif // APP_TYPES_H
