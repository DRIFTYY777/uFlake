#ifndef INPUTS_H
#define INPUTS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Initialize the keypad input device for LVGL
     * This function creates an LVGL input device of type keypad and sets the read callback to keypad_read_cb.
     */
    void keypad_init(void);

    /**
     * @brief LVGL input device read callback for the keypad
     * This function is called by LVGL to read the current state of the keypad.
     */
    static void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif

#endif // INPUTS_H