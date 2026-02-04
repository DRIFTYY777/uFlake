#ifndef UBOOT_SCREEN_LVGL_H
#define UBOOT_SCREEN_LVGL_H

#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize LVGL-based boot screen
     *
     * Creates a canvas and allocates buffers for plasma animation
     *
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t uboot_screen_lvgl_init(void);

    /**
     * @brief Render one frame of the boot screen animation
     *
     * @param frame Frame number (0-based)
     */
    void uboot_screen_lvgl_render_frame(int frame);

    /**
     * @brief Cleanup boot screen resources
     */
    void uboot_screen_lvgl_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // UBOOT_SCREEN_LVGL_H
