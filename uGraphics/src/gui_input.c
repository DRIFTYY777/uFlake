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

lv_indev_t *keypad_get_indev(void)
{
    return keypad_indev;
}

void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev; // Unused parameter
    
    InputKey key;
    InputType input_type = input_get_key_event(&key);

    // Check if we have a valid input event
    if (input_type == InputTypePress || input_type == InputTypeShort)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        
        // Map InputKey to LVGL key codes
        switch (key)
        {
            case InputKeyUp:
                data->key = LV_KEY_UP;
                UFLAKE_LOGI(TAG, "Keypad event: UP");
                break;
            case InputKeyDown:
                data->key = LV_KEY_DOWN;
                UFLAKE_LOGI(TAG, "Keypad event: DOWN");
                break;
            case InputKeyRight:
                data->key = LV_KEY_RIGHT;
                UFLAKE_LOGI(TAG, "Keypad event: RIGHT");
                break;
            case InputKeyLeft:
                data->key = LV_KEY_LEFT;
                UFLAKE_LOGI(TAG, "Keypad event: LEFT");
                break;
            case InputKeyOk:
                data->key = LV_KEY_ENTER;
                UFLAKE_LOGI(TAG, "Keypad event: OK");
                break;
            case InputKeyBack:
                data->key = LV_KEY_ESC;
                UFLAKE_LOGI(TAG, "Keypad event: BACK");
                break;
            default:
                data->state = LV_INDEV_STATE_RELEASED;
                data->key = 0;
                break;
        }
    }
    else if (input_type == InputTypeRelease)
    {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = 0;
        UFLAKE_LOGI(TAG, "Keypad event: RELEASE");
    }
    else
    {
        // No input event - keep previous state as released
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = 0;
    }
}