#ifndef UBOOT_SCREEN_H
#define UBOOT_SCREEN_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "kernel.h"

#include "ST7789.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Boot screen configuration

#define BOOT_SCREEN_STRIP_HEIGHT 20 // Render in strips for memory efficiency
#define BOOT_SCREEN_FPS 30
#define BOOT_SCREEN_DURATION_FRAMES 120 // Show for 2 seconds at 60 FPS
#define BOOT_SCREEN_TASK_PRIORITY 5
#define BOOT_SCREEN_TASK_CORE 1 // Run on core 1

    // Boot screen state structure
    typedef struct
    {
        st7789_driver_t *driver;
        TaskHandle_t task_handle;
        volatile bool running;
        volatile bool completed;
        volatile int frame;
    } boot_screen_state_t;

    /**
     * @brief Initialize and start the boot screen animation
     *
     * @param driver Pointer to initialized ST7789 display driver
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t uboot_screen_start(st7789_driver_t *driver);

    /**
     * @brief Stop the boot screen animation
     */
    void uboot_screen_stop(void);

    /**
     * @brief Check if boot screen is currently running
     *
     * @return true if running, false otherwise
     */
    bool uboot_screen_is_running(void);

    /**
     * @brief Check if boot screen has completed
     *
     * @return true if completed, false otherwise
     */
    bool uboot_screen_is_completed(void);

    /**
     * @brief Get current frame number
     *
     * @return Current frame number
     */
    int uboot_screen_get_frame(void);

    /**
     * @brief Set boot screen brightness manually
     *
     * @param brightness Brightness level (0.0 - 100.0%)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t uboot_screen_set_brightness(float brightness);

#ifdef __cplusplus
}
#endif

#endif // UBOOT_SCREEN_H
