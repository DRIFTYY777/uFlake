#include "uBootScreen.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <math.h>
#include <string.h>
#include "kernel.h"
#include "ST7789.h"

static const char *TAG = "uBootScreen";

// Sin table for fast trigonometric calculations (0-1023 = 0-360 degrees)
static const uint8_t sin_table[1024] = {
    128, 129, 130, 130, 131, 132, 133, 133, 134, 135, 136, 137, 137, 138, 139, 140,
    140, 141, 142, 143, 144, 144, 145, 146, 147, 147, 148, 149, 150, 151, 151, 152,
    153, 154, 154, 155, 156, 157, 157, 158, 159, 160, 160, 161, 162, 163, 164, 164,
    165, 166, 167, 167, 168, 169, 169, 170, 171, 172, 172, 173, 174, 175, 175, 176,
    177, 178, 178, 179, 180, 180, 181, 182, 183, 183, 184, 185, 185, 186, 187, 187,
    188, 189, 189, 190, 191, 192, 192, 193, 194, 194, 195, 196, 196, 197, 198, 198,
    199, 199, 200, 201, 201, 202, 203, 203, 204, 205, 205, 206, 206, 207, 208, 208,
    209, 209, 210, 211, 211, 212, 212, 213, 214, 214, 215, 215, 216, 216, 217, 218,
    218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226,
    227, 227, 228, 228, 229, 229, 229, 230, 230, 231, 231, 232, 232, 233, 233, 234,
    234, 234, 235, 235, 236, 236, 237, 237, 237, 238, 238, 239, 239, 239, 240, 240,
    240, 241, 241, 242, 242, 242, 243, 243, 243, 244, 244, 244, 245, 245, 245, 245,
    246, 246, 246, 247, 247, 247, 248, 248, 248, 248, 249, 249, 249, 249, 250, 250,
    250, 250, 250, 251, 251, 251, 251, 251, 252, 252, 252, 252, 252, 253, 253, 253,
    253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 254, 254, 254, 254, 254, 254, 254, 254, 253, 253, 253,
    253, 253, 253, 253, 252, 252, 252, 252, 252, 251, 251, 251, 251, 251, 250, 250,
    250, 250, 250, 249, 249, 249, 249, 248, 248, 248, 248, 247, 247, 247, 246, 246,
    246, 245, 245, 245, 245, 244, 244, 244, 243, 243, 243, 242, 242, 242, 241, 241,
    240, 240, 240, 239, 239, 239, 238, 238, 237, 237, 237, 236, 236, 235, 235, 234,
    234, 234, 233, 233, 232, 232, 231, 231, 230, 230, 229, 229, 229, 228, 228, 227,
    227, 226, 226, 225, 225, 224, 224, 223, 222, 222, 221, 221, 220, 220, 219, 219,
    218, 218, 217, 216, 216, 215, 215, 214, 214, 213, 212, 212, 211, 211, 210, 209,
    209, 208, 208, 207, 206, 206, 205, 205, 204, 203, 203, 202, 201, 201, 200, 199,
    199, 198, 198, 197, 196, 196, 195, 194, 194, 193, 192, 192, 191, 190, 189, 189,
    188, 187, 187, 186, 185, 185, 184, 183, 183, 182, 181, 180, 180, 179, 178, 178,
    177, 176, 175, 175, 174, 173, 172, 172, 171, 170, 169, 169, 168, 167, 167, 166,
    165, 164, 164, 163, 162, 161, 160, 160, 159, 158, 157, 157, 156, 155, 154, 154,
    153, 152, 151, 151, 150, 149, 148, 147, 147, 146, 145, 144, 144, 143, 142, 141,
    140, 140, 139, 138, 137, 137, 136, 135, 134, 133, 133, 132, 131, 130, 130, 129,
    128, 127, 126, 126, 125, 124, 123, 123, 122, 121, 120, 119, 119, 118, 117, 116,
    116, 115, 114, 113, 112, 112, 111, 110, 109, 109, 108, 107, 106, 105, 105, 104,
    103, 102, 102, 101, 100, 99, 99, 98, 97, 96, 96, 95, 94, 93, 92, 92,
    91, 90, 89, 89, 88, 87, 87, 86, 85, 84, 84, 83, 82, 81, 81, 80,
    79, 78, 78, 77, 76, 76, 75, 74, 73, 73, 72, 71, 71, 70, 69, 69,
    68, 67, 67, 66, 65, 64, 64, 63, 62, 62, 61, 60, 60, 59, 58, 58,
    57, 57, 56, 55, 55, 54, 53, 53, 52, 51, 51, 50, 50, 49, 48, 48,
    47, 47, 46, 45, 45, 44, 44, 43, 42, 42, 41, 41, 40, 40, 39, 38,
    38, 37, 37, 36, 36, 35, 35, 34, 34, 33, 32, 32, 31, 31, 30, 30,
    29, 29, 28, 28, 27, 27, 27, 26, 26, 25, 25, 24, 24, 23, 23, 22,
    22, 22, 21, 21, 20, 20, 19, 19, 19, 18, 18, 17, 17, 17, 16, 16,
    16, 15, 15, 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 11, 11,
    10, 10, 10, 9, 9, 9, 8, 8, 8, 8, 7, 7, 7, 7, 6, 6,
    6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 3, 3, 3,
    3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
    3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6,
    6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10,
    10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15,
    16, 16, 16, 17, 17, 17, 18, 18, 19, 19, 19, 20, 20, 21, 21, 22,
    22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 27, 28, 28, 29,
    29, 30, 30, 31, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37,
    38, 38, 39, 40, 40, 41, 41, 42, 42, 43, 44, 44, 45, 45, 46, 47,
    47, 48, 48, 49, 50, 50, 51, 51, 52, 53, 53, 54, 55, 55, 56, 57,
    57, 58, 58, 59, 60, 60, 61, 62, 62, 63, 64, 64, 65, 66, 67, 67,
    68, 69, 69, 70, 71, 71, 72, 73, 73, 74, 75, 76, 76, 77, 78, 78,
    79, 80, 81, 81, 82, 83, 84, 84, 85, 86, 87, 87, 88, 89, 89, 90,
    91, 92, 92, 93, 94, 95, 96, 96, 97, 98, 99, 99, 100, 101, 102, 102,
    103, 104, 105, 105, 106, 107, 108, 109, 109, 110, 111, 112, 112, 113, 114, 115,
    116, 116, 117, 118, 119, 119, 120, 121, 122, 123, 123, 124, 125, 126, 126, 127};

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

    BaseType_t ret = xTaskCreatePinnedToCore(
        boot_screen_task,
        "boot_screen",
        4096,
        driver,
        BOOT_SCREEN_TASK_PRIORITY,
        &boot_state.task_handle,
        BOOT_SCREEN_TASK_CORE);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create boot screen task");
        boot_state.running = false;
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
