#if !defined(INPUT_H)
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define INPUT_DEBOUNCE_MS 50     // 50ms debounce
#define INPUT_LONG_PRESS_MS 1000 // 1 second for long press

    typedef enum
    {
        InputKeyUp = 0,
        InputKeyDown,
        InputKeyRight,
        InputKeyLeft,
        InputKeyOk,
        InputKeyBack,
        InputKeyNone = 0xFF
    } InputKey;

    typedef enum
    {
        InputTypePress,
        InputTypeRelease,
        InputTypeShort,
        InputTypeLong,
        InputTypeNone
    } InputType;

    typedef struct
    {
        bool is_pressed;
        uint32_t press_start_time;
        bool long_press_sent;
        uint32_t last_debounce_time;
        bool last_state;
    } InputKeyState;

    typedef struct
    {
        InputKeyState keys[6]; // 6 keys: Up, Down, Right, Left, Ok, Back
        bool initialized;
    } InputService;

    // Simple API functions
    uflake_result_t input_init(void);
    uflake_result_t input_deinit(void);

    // Poll for input events (call this regularly)
    InputType input_get_key_event(InputKey *key);

    // Check current key states
    bool input_is_key_pressed(InputKey key);
    uint32_t input_get_press_duration(InputKey key);

#ifdef __cplusplus
}
#endif

#endif // INPUT_H
