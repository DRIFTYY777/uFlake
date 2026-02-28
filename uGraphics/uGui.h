#ifndef UGUI_H
#define UGUI_H

#include "lvgl.h"
#include "kernel.h"
#include "gui_types.h"

#include "ST7789.h"

#define LV_TICK_PERIOD_MS 10
#define DISP_BUF_SIZE (240 * 30) // width * 2

// Main initialization
void uGui_init(st7789_driver_t *drv);

// Accessors for global input device and group (for multi-window management)
lv_indev_t *uGui_get_input_device(void);
lv_group_t *uGui_get_group(void);
uflake_mutex_t *uGui_get_mutex(void);

// Get the content container for app UI (properly positioned below notification bar)
lv_obj_t *uGui_get_content_container(void);

// Group management for app windows
void uGui_clear_group(void); // Clear all objects from group
void uGui_reset_group(void); // Reset group for new window

#endif // UGUI_H