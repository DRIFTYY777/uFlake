/**
 * @file app_force_exit.cpp
 * @brief Force Exit Monitor Implementation
 *
 * Monitors raw button input via PCA9555 to detect back button hold.
 * When held for 3 seconds, force-terminates the current app.
 */

#include "app_force_exit.h"
#include "appLoader.h"
#include "uGui.h"
#include "pca9555.h"
#include "uI2c.h"
#include "timer_manager.h"
#include <sys/time.h>

static const char *TAG = "ForceExit";

// State for back button hold detection
static uint32_t back_press_start_time = 0;
static bool back_was_pressed = false;
static bool force_exit_triggered = false;
static uint32_t force_exit_timer_id = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static uint32_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static bool is_back_button_pressed(void)
{
    // Read PCA9555 directly (bit 6 is back button)
    const uint16_t inputs_value = read_pca9555_inputs(UI2C_PORT_0, PCA9555_ADDRESS);
    // Button is active low
    return !((inputs_value >> 6) & 0x01);
}

// ============================================================================
// FORCE EXIT CHECK
// ============================================================================

void app_force_exit_check(void)
{
    bool back_pressed = is_back_button_pressed();
    uint32_t current_time = get_time_ms();

    if (back_pressed)
    {
        if (!back_was_pressed)
        {
            // Button just pressed - start timing
            back_press_start_time = current_time;
            back_was_pressed = true;
            force_exit_triggered = false;
        }
        else if (!force_exit_triggered)
        {
            // Button held - check duration
            uint32_t hold_time = current_time - back_press_start_time;

            if (hold_time >= FORCE_EXIT_HOLD_TIME_MS)
            {
                // Held long enough - trigger force exit
                force_exit_triggered = true;
                UFLAKE_LOGW(TAG, "Force exit triggered (held %lu ms)", hold_time);
                app_force_exit_trigger();
            }
        }
    }
    else
    {
        // Button released
        back_was_pressed = false;
        force_exit_triggered = false;
    }
}

// ============================================================================
// FORCE EXIT TRIGGER
// ============================================================================

uflake_result_t app_force_exit_trigger(void)
{
    // Check if we're in an app (not on home screen)
    if (uGui_is_home_screen())
    {
        UFLAKE_LOGI(TAG, "On home screen - nothing to force exit");
        return UFLAKE_OK;
    }

    uint32_t current_app_id = uGui_get_current_app_id();
    if (current_app_id == 0)
    {
        UFLAKE_LOGW(TAG, "No current app to force exit");
        return UFLAKE_ERROR_NOT_FOUND;
    }

    UFLAKE_LOGW(TAG, "Force terminating app ID: %lu", current_app_id);

    // Terminate via app loader
    uflake_result_t result = app_loader_terminate(current_app_id);

    if (result == UFLAKE_OK)
    {
        // Also tell uGui to exit (clears UI, returns to launcher)
        uGui_exit_current_app();
    }

    return result;
}

// ============================================================================
// TIMER CALLBACK
// ============================================================================

static void force_exit_timer_cb(void *arg)
{
    (void)arg;
    app_force_exit_check();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

uflake_result_t app_force_exit_init(void)
{
    // Create periodic timer to check button state (50ms interval)
    uflake_result_t result = uflake_timer_create(
        &force_exit_timer_id,
        50,   // 50ms period
        force_exit_timer_cb,
        NULL,
        true  // Periodic
    );

    if (result != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create force exit timer");
        return result;
    }

    result = uflake_timer_start(force_exit_timer_id);
    if (result != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to start force exit timer");
        return result;
    }

    UFLAKE_LOGI(TAG, "Force exit monitor initialized (hold Back %dms)",
                FORCE_EXIT_HOLD_TIME_MS);
    return UFLAKE_OK;
}
