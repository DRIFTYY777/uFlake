#include "gui_input.h"
#include "logger.h"
#include "input.h"

static lv_indev_t *keypad_indev;

static const char *TAG = "uGUI-Input";

void keypad_init(void)
{
    // Create LVGL input device for keypad
    keypad_indev = lv_indev_create();
    if (keypad_indev)
    {
        lv_indev_set_type(keypad_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(keypad_indev, keypad_read_cb);
        UFLAKE_LOGI(TAG, "Keypad input device created");
    }
    else
    {
        UFLAKE_LOGE(TAG, "Failed to create keypad input device");
    }
}

void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    InputKey key;
    input_get_key_event(&key);

    if (key == InputKeyUp)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyUp;
        UFLAKE_LOGI(TAG, "Keypad event: UP");
    }
    else if (key == InputKeyDown)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyDown;
        UFLAKE_LOGI(TAG, "Keypad event: DOWN");
    }
    else if (key == InputKeyRight)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyRight;
        UFLAKE_LOGI(TAG, "Keypad event: RIGHT");
    }
    else if (key == InputKeyLeft)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyLeft;
        UFLAKE_LOGI(TAG, "Keypad event: LEFT");
    }
    else if (key == InputKeyOk)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyOk;
        UFLAKE_LOGI(TAG, "Keypad event: OK");
    }
    else if (key == InputKeyBack)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = InputKeyBack;
        UFLAKE_LOGI(TAG, "Keypad event: BACK");
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = InputKeyNone;
    }
}