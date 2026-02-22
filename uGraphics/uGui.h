#ifndef UGUI_H
#define UGUI_H

#include "lvgl.h"
#include "kernel.h"

#define LV_TICK_PERIOD_MS 10
#define DISP_BUF_SIZE (240 * 30) // width * 2

class uGUI
{
private:
    // Forward declarations
    static void lv_tick_timer_cb(void *arg);
    static void gui_task(void *arg);

public:
    void initialize(uint16_t display_width, uint16_t display_height);
};

#endif // UGUI_H