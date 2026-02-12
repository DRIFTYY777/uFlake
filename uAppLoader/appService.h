#ifndef APP_SERVICE_H
#define APP_SERVICE_H

#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_SERVICES 16
#define SERVICE_NAME_MAX_LEN 32
#define SERVICE_VERSION_MAX_LEN 16

    // Service types - different categories of background services
    typedef enum
    {
        SERVICE_TYPE_SYSTEM = 0, // Core system services (power, battery)
        SERVICE_TYPE_INPUT,      // Input handling services (buttons, touch)
        SERVICE_TYPE_DRIVER,     // Hardware driver services
        SERVICE_TYPE_NETWORK,    // Network services (WiFi, BT)
        SERVICE_TYPE_STORAGE,    // Storage services (SD card, file system)
        SERVICE_TYPE_CUSTOM      // User-defined services
    } service_type_t;

    // Service state
    typedef enum
    {
        SERVICE_STATE_STOPPED = 0,
        SERVICE_STATE_STARTING,
        SERVICE_STATE_RUNNING,
        SERVICE_STATE_STOPPING,
        SERVICE_STATE_ERROR
    } service_state_t;

    // Service lifecycle callbacks
    typedef uflake_result_t (*service_init_fn)(void);
    typedef uflake_result_t (*service_start_fn)(void);
    typedef uflake_result_t (*service_stop_fn)(void);
    typedef uflake_result_t (*service_deinit_fn)(void);

    // Service manifest - describes the service
    typedef struct
    {
        char name[SERVICE_NAME_MAX_LEN];       // Service name
        char version[SERVICE_VERSION_MAX_LEN]; // Version string
        service_type_t type;                   // Service category
        uint32_t stack_size;                   // Task stack size (0 = default)
        uint32_t priority;                     // Task priority (0 = default)
        bool auto_start;                       // Auto-start on boot
        bool critical;                         // System-critical service
        uint32_t dependencies[MAX_SERVICES];   // Array of service IDs this depends on (0-terminated)
    } service_manifest_t;

    // Service descriptor - runtime info
    typedef struct
    {
        uint32_t service_id;         // Unique service ID
        service_manifest_t manifest; // Service metadata
        service_state_t state;       // Current state
        TaskHandle_t task_handle;    // FreeRTOS task handle
        void *context;               // Service-specific context data

        // Lifecycle callbacks
        service_init_fn init;
        service_start_fn start;
        service_stop_fn stop;
        service_deinit_fn deinit;

        // Stats
        uint32_t start_count;     // Times started
        uint32_t crash_count;     // Times crashed
        uint32_t last_start_time; // Last start timestamp
    } service_descriptor_t;

    // Service registration bundle
    typedef struct
    {
        const service_manifest_t *manifest;
        service_init_fn init;
        service_start_fn start;
        service_stop_fn stop;
        service_deinit_fn deinit;
        void *context;
    } service_bundle_t;

    // ============================================================================
    // SERVICE MANAGER INITIALIZATION
    // ============================================================================

    /**
     * @brief Initialize the service manager
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_manager_init(void);

    /**
     * @brief Start all auto-start services in dependency order
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_manager_start_all(void);

    /**
     * @brief Stop all running services
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_manager_stop_all(void);

    // ============================================================================
    // SERVICE REGISTRATION
    // ============================================================================

    /**
     * @brief Register a service (simplified - one line registration)
     * @param service_bundle Pointer to service bundle structure
     * @return Service ID on success, 0 on failure
     */
    uint32_t service_register(const service_bundle_t *service_bundle);

    /**
     * @brief Unregister a service
     * @param service_id Service ID to unregister
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_unregister(uint32_t service_id);

    // ============================================================================
    // SERVICE LIFECYCLE
    // ============================================================================

    /**
     * @brief Start a service (calls init then start callbacks)
     * @param service_id Service ID to start
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_start(uint32_t service_id);

    /**
     * @brief Stop a running service (calls stop then deinit callbacks)
     * @param service_id Service ID to stop
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_stop(uint32_t service_id);

    /**
     * @brief Restart a service (stop then start)
     * @param service_id Service ID to restart
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_restart(uint32_t service_id);

    // ============================================================================
    // SERVICE QUERY
    // ============================================================================

    /**
     * @brief Get list of all registered services
     * @param services Pointer to receive array of service descriptors
     * @param count Pointer to receive number of services
     * @return UFLAKE_OK on success
     */
    uflake_result_t service_get_all(service_descriptor_t **services, uint32_t *count);

    /**
     * @brief Get service descriptor by ID
     * @param service_id Service ID
     * @return Pointer to service descriptor, NULL if not found
     */
    service_descriptor_t *service_get(uint32_t service_id);

    /**
     * @brief Find service by name
     * @param name Service name
     * @return Service ID, or 0 if not found
     */
    uint32_t service_find_by_name(const char *name);

    /**
     * @brief Check if service is running
     * @param service_id Service ID
     * @return true if running, false otherwise
     */
    bool service_is_running(uint32_t service_id);

    /**
     * @brief Get service context data
     * @param service_id Service ID
     * @return Pointer to context data, NULL if not found
     */
    void *service_get_context(uint32_t service_id);

#ifdef __cplusplus
}
#endif

#endif // APP_SERVICE_H
