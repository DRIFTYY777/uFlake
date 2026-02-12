#include "input.h"
#include "uI2c.h"
#include "pca9555.h"
#include <sys/time.h>
#include "appService.h"

static const char *TAG = "INPUT";
static InputService g_input = {0};

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
        return (hw_reading == 0x0001);
    case InputKeyDown:
        return (hw_reading == 0xFDFF);
    case InputKeyRight:
        return (hw_reading == 0xFBFF);
    case InputKeyLeft:
        return (hw_reading == 0xF7FF);
    case InputKeyOk:
        return (hw_reading == 0xEFFF);
    case InputKeyBack:
        return (hw_reading == 0xDFFF);
    default:
        return false;
    }
}

uflake_result_t input_init(void)
{
    ESP_LOGI(TAG, "Initializing simple input service");

    if (g_input.initialized)
    {
        ESP_LOGW(TAG, "Input already initialized");
        return UFLAKE_OK;
    }

    // Register as a service (no task assigned)
    static service_manifest_t input_manifest = {
        .name = "input_service",
        .version = "1.0",
        .type = SERVICE_TYPE_INPUT,
        .stack_size = 0, // No task
        .priority = 0,   // No task
        .auto_start = false,
        .critical = false,
        .dependencies = {0}};

    static service_bundle_t input_bundle = {
        .manifest = &input_manifest,
        .init = NULL,   // Already handled by input_init
        .start = NULL,  // No task
        .stop = NULL,   // No task
        .deinit = NULL, // Optional: could point to input_deinit
        .context = NULL};
    service_register(&input_bundle);

    // Initialize PCA9555
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    // Initialize input state
    memset(&g_input, 0, sizeof(InputService));
    g_input.initialized = true;

    ESP_LOGI(TAG, "Input service initialized");
    return UFLAKE_OK;
}

uflake_result_t input_deinit(void)
{
    if (!g_input.initialized)
    {
        return UFLAKE_OK;
    }

    g_input.initialized = false;
    ESP_LOGI(TAG, "Input service deinitialized");
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
