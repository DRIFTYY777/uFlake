#ifndef APP_MANIFEST_H
#define APP_MANIFEST_H

#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declaration - full type defined in appLoader.h
    typedef struct app_manifest_t app_manifest_t;

    // ============================================================================
    // MANIFEST PARSING
    // ============================================================================

    /**
     * @brief Parse manifest file from storage (for external apps)
     * @param path Path to manifest.txt file
     * @param manifest Pointer to manifest structure to fill
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_manifest_parse(const char *path, app_manifest_t *manifest);

    /**
     * @brief Validate manifest structure
     * @param manifest Pointer to manifest to validate
     * @return UFLAKE_OK if valid
     */
    uflake_result_t app_manifest_validate(const app_manifest_t *manifest);

    /**
     * @brief Create default manifest structure
     * @param manifest Pointer to manifest to initialize
     * @param name App name
     * @param version App version
     */
    void app_manifest_create_default(app_manifest_t *manifest,
                                     const char *name,
                                     const char *version);

    /**
     * @brief Print manifest info to log
     * @param manifest Pointer to manifest to print
     */
    void app_manifest_print(const app_manifest_t *manifest);

#ifdef __cplusplus
}
#endif

#endif // APP_MANIFEST_H
