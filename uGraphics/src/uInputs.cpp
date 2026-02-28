#include "uInputs.h"
#include "pca9555.h"
#include "uI2c.h"
#include "uGui.h"

#include "kernel.h"
#include <sys/time.h>

static const char *TAG = "uInputs";

#define DEBOUNCE_DELAY 0 // milliseconds

// Debounce state for each button
static unsigned long lastPressTime[8] = {0};
static bool buttonPressed[8] = {false};

static uint32_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void uInput(lv_indev_t *indev, lv_indev_data_t *data)
{
    unsigned long currentTime = get_time_ms();

    // Read PCA9555 directly - same as Arduino approach
    const uint16_t inputs_value = read_pca9555_inputs(UI2C_PORT_0, PCA9555_ADDRESS);

    // Get the group for focus control
    lv_group_t *group = uGui_get_group();

    data->state = LV_INDEV_STATE_RELEASED; // Default state: Released

    // Up button (bit 0)
    if (!((inputs_value >> 0) & 0x01))
    {
        if (!buttonPressed[0] && (currentTime - lastPressTime[0] > DEBOUNCE_DELAY))
        {
            UFLAKE_LOGI(TAG, "Up Pressed");
            data->key = LV_KEY_UP;
            data->state = LV_INDEV_STATE_PRESSED;
            lastPressTime[0] = currentTime;
            buttonPressed[0] = true;
            if (group)
                lv_group_focus_prev(group);
        }
    }
    else
    {
        buttonPressed[0] = false;
    }

    // Down button (bit 1)
    if (!((inputs_value >> 1) & 0x01))
    {
        if (!buttonPressed[1] && (currentTime - lastPressTime[1] > DEBOUNCE_DELAY))
        {
            UFLAKE_LOGI(TAG, "Down Pressed");
            data->key = LV_KEY_DOWN;
            data->state = LV_INDEV_STATE_PRESSED;
            lastPressTime[1] = currentTime;
            buttonPressed[1] = true;
            if (group)
                lv_group_focus_next(group);
        }
    }
    else
    {
        buttonPressed[1] = false;
    }

    // Right button (bit 2)
    if (!((inputs_value >> 2) & 0x01))
    {
        if (!buttonPressed[2] && (currentTime - lastPressTime[2] > DEBOUNCE_DELAY))
        {
            UFLAKE_LOGI(TAG, "Right Pressed");
            data->key = LV_KEY_RIGHT;
            data->state = LV_INDEV_STATE_PRESSED;
            lastPressTime[2] = currentTime;
            buttonPressed[2] = true;
        }
    }
    else
    {
        buttonPressed[2] = false;
    }

    // Left button (bit 3)
    if (!((inputs_value >> 3) & 0x01))
    {
        if (!buttonPressed[3] && (currentTime - lastPressTime[3] > DEBOUNCE_DELAY))
        {
            UFLAKE_LOGI(TAG, "Left Pressed");
            data->key = LV_KEY_LEFT;
            data->state = LV_INDEV_STATE_PRESSED;
            lastPressTime[3] = currentTime;
            buttonPressed[3] = true;
        }
    }
    else
    {
        buttonPressed[3] = false;
    }

    // Back button (bit 6)
    if (!((inputs_value >> 6) & 0x01))
    {
        if (!buttonPressed[4] && (currentTime - lastPressTime[4] > DEBOUNCE_DELAY))
        {
            UFLAKE_LOGI(TAG, "Back Pressed");
            data->key = LV_KEY_ESC;
            data->state = LV_INDEV_STATE_PRESSED;
            lastPressTime[4] = currentTime;
            buttonPressed[4] = true;
        }
    }
    else
    {
        buttonPressed[4] = false;
    }

    // OK/Enter button (bit 7)
    if (!((inputs_value >> 7) & 0x01))
    {
        if (!buttonPressed[5] && (currentTime - lastPressTime[5] > DEBOUNCE_DELAY))
        {
            UFLAKE_LOGI(TAG, "Enter Pressed");
            data->key = LV_KEY_ENTER;
            data->state = LV_INDEV_STATE_PRESSED;
            lastPressTime[5] = currentTime;
            buttonPressed[5] = true;
        }
    }
    else
    {
        buttonPressed[5] = false;
    }
}
