#include "uBootScreen.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

#include "sin_table.h"
#include "uGPIO.h"
#include <math.h>
#include <string.h>
#include "ST7789.h"

#define BACKLIGHT_PIN GPIO_NUM_3
#define BRIGHTNESS_FADE_IN_FRAMES 30 // 0.5 seconds
#define BRIGHTNESS_FADE_OUT_START 90 // Start fade out at frame 90
#define BRIGHTNESS_MAX 100.0f        // Maximum brightness in %

static const char *TAG = "uBootScreen";

// Dither table for smooth color gradients
static uint8_t dither_table[256];

// Boot screen state
static boot_screen_state_t boot_state = {0};

// Fast sine lookup
static inline uint8_t fast_sin(int value)
{
    return sin_table[value & 0x3ff];
}

// RGB to RGB565 color conversion
static inline uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((((uint16_t)(r) >> 3) << 11) | (((uint16_t)(g) >> 2) << 5) | ((uint16_t)(b) >> 3));
}

// RGB565 to RGB conversion
static inline void rgb565_to_rgb(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *b = (color << 3);
    color >>= 5;
    color <<= 2;
    *g = color;
    color >>= 8;
    *r = color << 3;
}

// RGB to RGB565 with dithering for smooth gradients
static inline uint16_t rgb_to_rgb565_dither(uint8_t r, uint8_t g, uint8_t b, uint16_t x, uint16_t y)
{
    const uint8_t pos = ((y << 4) + x) & 0xff;
    uint8_t rand_val = dither_table[pos];
    const uint8_t rand_r = rand_val & 0x07;
    rand_val >>= 3;
    const uint8_t rand_g = rand_val & 0x03;
    rand_val >>= 2;
    const uint8_t rand_b = rand_val;

    if (r < 249)
        r = r + rand_r;
    if (g < 253)
        g = g + rand_g;
    if (b < 249)
        b = b + rand_b;

    return rgb_to_rgb565(r, g, b);
}

// Randomize dither table for each frame
static void randomize_dither_table(void)
{
    for (size_t i = 0; i < 256; ++i)
    {
        dither_table[i] = esp_random() & 0xff;
    }
}

// Calculate brightness based on current frame with smooth fade in/out
static float calculate_brightness(int frame)
{
    float brightness = 0.0f;

    if (frame < BRIGHTNESS_FADE_IN_FRAMES)
    {
        // Smooth fade in using ease-out cubic curve
        float t = (float)frame / BRIGHTNESS_FADE_IN_FRAMES;
        t = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        brightness = BRIGHTNESS_MAX * t;
    }
    else if (frame < BRIGHTNESS_FADE_OUT_START)
    {
        // Full brightness
        brightness = BRIGHTNESS_MAX;
    }
    else if (frame < BOOT_SCREEN_DURATION_FRAMES)
    {
        // Smooth fade out using ease-in cubic curve
        float t = (float)(frame - BRIGHTNESS_FADE_OUT_START) /
                  (BOOT_SCREEN_DURATION_FRAMES - BRIGHTNESS_FADE_OUT_START);
        t = t * t * t;
        brightness = BRIGHTNESS_MAX * (1.0f - t);
    }

    return brightness;
}

// Update backlight brightness
static void update_brightness(int frame)
{
    float brightness = calculate_brightness(frame);
    ugpio_pwm_set_duty(BACKLIGHT_PIN, brightness);
}

// Render plasma effect to buffer
static void render_plasma_line(uint16_t *buffer, int y, int frame, int width)
{
    const int plasma_shift = frame < 256 ? 1 : 2;
    const int frame_1 = frame << 1;
    const int frame_2 = frame << 2;
    const int frame_7 = frame * 7;

    for (int x = 0; x < width; x++)
    {
        const int x_1 = x << 1;
        const int x_2 = x << 2;
        const int y_1 = y << 1;
        const int y_2 = y << 2;

        // Calculate plasma color using sine waves
        uint16_t plasma_r = fast_sin(x_2 + y_1 + frame_2);
        plasma_r += fast_sin(fast_sin(((y_1 + frame) << 1) + x) + frame_7);
        plasma_r >>= plasma_shift;

        uint16_t plasma_b = fast_sin(x + y_2 + frame_1);
        plasma_b += fast_sin(fast_sin(((x_1 + frame) << 1) + y) + frame_1);
        plasma_b >>= plasma_shift;

        // Fade in/out effects
        if (frame < 256)
        {
            if (frame < 64)
            {
                plasma_r = (plasma_r * frame) >> 6;
                plasma_b = (plasma_b * frame) >> 6;
            }
            if (frame > 192)
            {
                int fade = (32 + ((256 - frame) >> 2));
                plasma_r = (plasma_r * fade) >> 6;
                plasma_b = (plasma_b * fade) >> 6;
            }
        }

        uint8_t color_r = plasma_r;
        uint8_t color_g = (plasma_r >> 1) + (plasma_b >> 1);
        uint8_t color_b = plasma_b;

        buffer[x] = rgb_to_rgb565_dither(color_r, color_g, color_b, x, y);
    }
}

// Draw text using LVGL canvas
static void render_text_overlay(uint16_t *buffer, int buffer_y, int frame,
                                const char *text, int text_x, int text_y,
                                int width, int height)
{
    // Simple character rendering - you can enhance this with LVGL fonts
    // For now, this is a placeholder that you can replace with LVGL text rendering
    (void)buffer;
    (void)buffer_y;
    (void)frame;
    (void)text;
    (void)text_x;
    (void)text_y;
    (void)width;
    (void)height;
}

// Render one strip of the boot screen
static void render_boot_screen_strip(st7789_driver_t *driver, int strip_y, int strip_height)
{
    if (!boot_state.running)
        return;

    uint16_t *buffer = driver->current_buffer;
    int frame = boot_state.frame;
    const int width = driver->display_width;

    // Render plasma for this strip
    for (int y = 0; y < strip_height; y++)
    {
        render_plasma_line(&buffer[y * width], strip_y + y, frame, width);
    }

    // Add text overlay if in visible range
    if (frame < 100 && strip_y < 140 && (strip_y + strip_height) > 80)
    {
        render_text_overlay(buffer, strip_y, frame, "uFlake",
                            width / 2 - 80, 100,
                            width, strip_height);
    }
}

// Boot screen task
static void boot_screen_task(void *arg)
{
    st7789_driver_t *driver = (st7789_driver_t *)arg;

    ESP_LOGI(TAG, "Boot screen started");

    const int display_height = driver->display_height;
    const int display_width = driver->display_width;
    const int strips_per_frame = display_height / BOOT_SCREEN_STRIP_HEIGHT;
    const int frame_delay_ms = 1000 / BOOT_SCREEN_FPS;

    while (boot_state.running && boot_state.frame < BOOT_SCREEN_DURATION_FRAMES)
    {
        uint64_t frame_start = esp_timer_get_time();

        // Update brightness based on current frame
        update_brightness(boot_state.frame);

        // Randomize dither for smooth gradients
        randomize_dither_table();

        // Render frame strip by strip
        for (int strip = 0; strip < strips_per_frame; strip++)
        {
            if (!boot_state.running)
                break;

            int strip_y = strip * BOOT_SCREEN_STRIP_HEIGHT;

            // Set window for this strip
            ST7789_set_window(driver, 0, strip_y,
                              display_width - 1,
                              strip_y + BOOT_SCREEN_STRIP_HEIGHT - 1);

            // Render to buffer
            render_boot_screen_strip(driver, strip_y, BOOT_SCREEN_STRIP_HEIGHT);

            // Send to display via DMA
            ST7789_write_pixels(driver, driver->current_buffer,
                                display_width * BOOT_SCREEN_STRIP_HEIGHT);

            // Wait for DMA transfer
            ST7789_queue_empty(driver);

            // Swap buffers for double buffering
            driver->current_buffer = (driver->current_buffer == driver->buffer_primary)
                                         ? driver->buffer_secondary
                                         : driver->buffer_primary;
        }

        boot_state.frame++;

        // Frame rate limiting
        uint64_t frame_time = (esp_timer_get_time() - frame_start) / 1000;
        if (frame_time < frame_delay_ms)
        {
            vTaskDelay(pdMS_TO_TICKS(frame_delay_ms - frame_time));
        }
    }

    boot_state.running = false;
    boot_state.completed = true;

    // Ensure backlight is at full brightness after animation
    ugpio_pwm_set_duty(BACKLIGHT_PIN, 100.0f);

    ESP_LOGI(TAG, "Boot screen completed");

    vTaskDelete(NULL);
}

// Initialize and start boot screen
esp_err_t uboot_screen_start(st7789_driver_t *driver)
{
    if (!driver)
    {
        ESP_LOGE(TAG, "Invalid driver");
        return ESP_ERR_INVALID_ARG;
    }

    boot_state.driver = driver;
    boot_state.running = true;
    boot_state.completed = false;
    boot_state.frame = 0;

    // Initialize dither table
    randomize_dither_table();

    ESP_LOGI(TAG, "Creating boot screen task");

    uint32_t gui_pid;
    if (uflake_process_create("Boot_Screen_Task",
                              boot_screen_task,
                              driver,
                              4096,
                              BOOT_SCREEN_TASK_PRIORITY,
                              &gui_pid) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create GUI process");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Stop boot screen
void uboot_screen_stop(void)
{
    if (boot_state.running)
    {
        ESP_LOGI(TAG, "Stopping boot screen");
        boot_state.running = false;

        // Wait for task to finish
        int timeout = 100;
        while (!boot_state.completed && timeout-- > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// Check if boot screen is running
bool uboot_screen_is_running(void)
{
    return boot_state.running;
}

// Check if boot screen completed
bool uboot_screen_is_completed(void)
{
    return boot_state.completed;
}

// Get current frame number
int uboot_screen_get_frame(void)
{
    return boot_state.frame;
}

// Set brightness manually (useful for debugging or custom animations)
esp_err_t uboot_screen_set_brightness(float brightness)
{
    if (brightness < 0.0f)
        brightness = 0.0f;
    if (brightness > 100.0f)
        brightness = 100.0f;

    return ugpio_pwm_set_duty(BACKLIGHT_PIN, brightness);
}
