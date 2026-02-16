#include "input.h"
#include "uHal.h"
#include "pca9555.h"
#include <sys/time.h>
#include "appService.h"

static const char *TAG = "INPUT";
static InputService g_input = {0};

// Register as a service (no task assigned)
// Non-static so it can be externally registered (like apps)

service_manifest_t input_manifest = {
    .name = "input_service",
    .version = "1.0",
    .type = SERVICE_TYPE_INPUT,
    .stack_size = 0, // No task
    .priority = 0,   // No task
    .auto_start = false,
    .critical = false,
    .dependencies = {0}};

const service_bundle_t input_bundle = {
    .manifest = &input_manifest,
    .init = input_init,
    .start = NULL, // No task
    .stop = NULL,  // No task
    .deinit = input_deinit,
    .context = NULL};

// Simple function to get current time in milliseconds
static uint32_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// Convert hardware reading to key press states
static bool is_key_pressed_hw(InputKey key, uint16_t hw_reading)
{
    switch (key)
    {
    case InputKeyUp:
        return (!((hw_reading >> 0) & 0x01));
    case InputKeyDown:
        return (!((hw_reading >> 1) & 0x01));
    case InputKeyRight:
        return (!((hw_reading >> 2) & 0x01));
    case InputKeyLeft:
        return (!((hw_reading >> 3) & 0x01));
    case InputKeyOk:
        return (!((hw_reading >> 7) & 0x01));
    case InputKeyBack:
        return (!((hw_reading >> 6) & 0x01));
    default:
        return false;
    }
}

uflake_result_t input_init(void)
{
    UFLAKE_LOGI(TAG, "Initializing simple input service");

    if (g_input.initialized)
    {
        UFLAKE_LOGW(TAG, "Input already initialized");
        return UFLAKE_OK;
    }

    // Initialize PCA9555
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    // Initialize input state
    memset(&g_input, 0, sizeof(InputService));
    g_input.initialized = true;

    UFLAKE_LOGI(TAG, "Input service initialized");
    return UFLAKE_OK;
}

uflake_result_t input_deinit(void)
{
    if (!g_input.initialized)
    {
        return UFLAKE_OK;
    }

    g_input.initialized = false;
    UFLAKE_LOGI(TAG, "Input service deinitialized");
    return UFLAKE_OK;
}

InputType input_get_key_event(InputKey *key)
{
    if (!g_input.initialized || key == NULL)
    {
        *key = InputKeyNone;
        return InputTypeNone;
    }

    uint16_t hw_reading = read_pca9555_inputs(UI2C_PORT_0, PCA9555_ADDRESS);
    uint32_t current_time = get_time_ms();

    // Check each key
    for (int i = 0; i < 6; i++)
    {
        InputKey current_key = (InputKey)i;
        InputKeyState *key_state = &g_input.keys[i];
        bool currently_pressed = is_key_pressed_hw(current_key, hw_reading);

        // Debouncing
        if (currently_pressed != key_state->last_state)
        {
            key_state->last_debounce_time = current_time;
        }
        key_state->last_state = currently_pressed;

        // Only process if debounce time has passed
        if (current_time - key_state->last_debounce_time > INPUT_DEBOUNCE_MS)
        {
            // Key press detected
            if (currently_pressed && !key_state->is_pressed)
            {
                key_state->is_pressed = true;
                key_state->press_start_time = current_time;
                key_state->long_press_sent = false;
                *key = current_key;
                return InputTypePress;
            }
            // Key release detected
            else if (!currently_pressed && key_state->is_pressed)
            {
                key_state->is_pressed = false;
                uint32_t press_duration = current_time - key_state->press_start_time;
                *key = current_key;

                if (key_state->long_press_sent)
                {
                    return InputTypeRelease;
                }
                else if (press_duration >= INPUT_LONG_PRESS_MS)
                {
                    return InputTypeRelease; // Already sent long press
                }
                else
                {
                    return InputTypeShort;
                }
            }
            // Check for long press while key is held
            else if (currently_pressed && key_state->is_pressed && !key_state->long_press_sent)
            {
                uint32_t press_duration = current_time - key_state->press_start_time;
                if (press_duration >= INPUT_LONG_PRESS_MS)
                {
                    key_state->long_press_sent = true;
                    *key = current_key;
                    return InputTypeLong;
                }
            }
        }
    }

    *key = InputKeyNone;
    return InputTypeNone;
}

bool input_is_key_pressed(InputKey key)
{
    if (!g_input.initialized || key >= 6)
    {
        return false;
    }
    return g_input.keys[key].is_pressed;
}

uint32_t input_get_press_duration(InputKey key)
{
    if (!g_input.initialized || key >= 6 || !g_input.keys[key].is_pressed)
    {
        return 0;
    }
    return get_time_ms() - g_input.keys[key].press_start_time;
}
