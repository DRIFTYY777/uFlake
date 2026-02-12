#ifndef APP_LIFECYCLE_H
#define APP_LIFECYCLE_H

#include "kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declarations - full types defined in appLoader.h
    typedef struct app_descriptor_t app_descriptor_t;

    // ============================================================================
    // APP LIFECYCLE MANAGEMENT
    // ============================================================================

    /**
     * @brief Initialize the app lifecycle manager
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_lifecycle_init(void);

    /**
     * @brief Launch an app (creates task and calls app_main)
     * @param app Pointer to app descriptor
     * @param launcher_id ID of launcher app (for pause/resume)
     * @param current_app_id Pointer to store current app ID
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_lifecycle_launch(app_descriptor_t *app,
                                         uint32_t launcher_id,
                                         uint32_t *current_app_id);

    /**
     * @brief Terminate a running app
     * @param app Pointer to app descriptor
     * @param launcher_id ID of launcher app (for resume)
     * @param current_app_id Pointer to current app ID
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_lifecycle_terminate(app_descriptor_t *app,
                                            uint32_t launcher_id,
                                            uint32_t *current_app_id);

    /**
     * @brief Pause an app (suspend task, keeps state)
     * @param app Pointer to app descriptor
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_lifecycle_pause(app_descriptor_t *app);

    /**
     * @brief Resume a paused app
     * @param app Pointer to app descriptor
     * @param current_app_id Pointer to store current app ID
     * @return UFLAKE_OK on success
     */
    uflake_result_t app_lifecycle_resume(app_descriptor_t *app,
                                         uint32_t *current_app_id);

    /**
     * @brief Get app task wrapper function (used internally)
     * @param arg App ID as void pointer
     */
    void app_task_wrapper(void *arg);

#ifdef __cplusplus
}
#endif

#endif // APP_LIFECYCLE_H
