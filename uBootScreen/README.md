/**
 * @file boot_screen_example.c
 * @brief Example usage of uFlake boot screen with plasma animation
 * 
 * This file shows how to integrate the boot screen into your uFlake system
 */

#include "uBootScreen.h"
#include "uBootScreenLVGL.h"
#include "ST7789.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "BootExample";

/**
 * Example 1: Direct rendering to display (no LVGL)
 * This method renders directly to the ST7789 display using DMA
 * Best for minimal overhead and fast rendering
 */
void boot_screen_example_direct(st7789_driver_t *display_driver)
{
    ESP_LOGI(TAG, "Starting direct boot screen");
    
    // Start the boot screen animation
    esp_err_t ret = uboot_screen_start(display_driver);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start boot screen");
        return;
    }

    // Wait for boot screen to complete (or you can do other init tasks)
    while (uboot_screen_is_running())
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Boot screen completed");
}

/**
 * Example 2: LVGL-based rendering
 * This method uses LVGL canvas for rendering
 * Best for integration with LVGL GUI system
 */
void boot_screen_example_lvgl(void)
{
    ESP_LOGI(TAG, "Starting LVGL boot screen");

    // Initialize LVGL boot screen
    esp_err_t ret = uboot_screen_lvgl_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LVGL boot screen");
        return;
    }

    // Render animation frames
    const int total_frames = 120; // 3 seconds at 40 FPS
    const int frame_delay_ms = 25; // 40 FPS

    for (int frame = 0; frame < total_frames; frame++)
    {
        uboot_screen_lvgl_render_frame(frame);
        
        // Let LVGL process the frame
        lv_task_handler();
        
        vTaskDelay(pdMS_TO_TICKS(frame_delay_ms));
    }

    // Cleanup
    uboot_screen_lvgl_deinit();

    ESP_LOGI(TAG, "Boot screen completed");
}

/**
 * Example 3: Boot screen with custom callback
 * Run initialization tasks while boot screen is playing
 */
void boot_screen_example_with_init(st7789_driver_t *display_driver)
{
    ESP_LOGI(TAG, "Starting boot screen with parallel initialization");

    // Start boot screen
    uboot_screen_start(display_driver);

    // Perform system initialization while animation plays
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Loading WiFi drivers...");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Initializing file system...");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Loading applications...");

    // Wait for boot screen to finish
    while (uboot_screen_is_running())
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "System ready!");
}

/**
 * Example 4: Early boot screen (before LVGL init)
 * Useful for showing animation during system initialization
 */
void boot_screen_example_early_boot(st7789_driver_t *display_driver)
{
    ESP_LOGI(TAG, "Early boot screen - before LVGL");

    // This can run before LVGL is initialized
    uboot_screen_start(display_driver);

    // Do your initialization
    ESP_LOGI(TAG, "Initializing kernel...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Initializing peripherals...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Stop boot screen when ready to show LVGL UI
    uboot_screen_stop();
    
    ESP_LOGI(TAG, "Transitioning to main UI");
}

/**
 * Integration example for uFlakeCore
 * Add this to your uFlakeCore.c initialization sequence
 */
void uflake_core_init_with_boot_screen(void)
{
    // ... existing uFlake init code ...
    // uflake_kernel_init();
    // uflake_kernel_start();
    // initialize I2C, SPI, etc.
    
    // Initialize display
    // config_and_init_display();
    
    // Show boot screen while initializing rest of system
    // uboot_screen_start(&display);
    
    // Continue with initialization
    // config_and_init_nrf24();
    // config_and_init_sd_card();
    
    // Initialize LVGL (boot screen will continue in background)
    // uGui_init(&display);
    
    // Wait for boot screen to finish
    // while (uboot_screen_is_running()) {
    //     vTaskDelay(pdMS_TO_TICKS(50));
    // }
    
    // Now transition to main UI
    // register_builtin_apps();
}
