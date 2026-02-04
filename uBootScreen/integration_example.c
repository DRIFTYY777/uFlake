/**
 * @file uFlakeCore_with_bootscreen.c
 * @brief Example integration of boot screen into uFlakeCore initialization
 *
 * Copy this code into your uFlakeCore.c to add the boot screen animation
 */

// Add this include at the top of uFlakeCore.c
#include "uBootScreen.h"

// Replace your existing uflake_core_init() function with this enhanced version:

void uflake_core_init(void)
{
    // Initialize the kernel
    uflake_kernel_init();

    // Start the kernel
    uflake_kernel_start();

    // initialize nvs subsystem
    unvs_init();

    ESP_LOGI(TAG, "Initializing I2C and SPI busses...");

    // initialize I2C
    i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ);

    // INITIALIZE the FIRST SPI BUS - before adding any devices
    uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 32768);
    // INITIALIZE the Second SPI BUS FIRST - before adding any devices
    uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096);

    ESP_LOGI(TAG, "Initializing display...");
    config_and_init_display();

    // ⭐ START BOOT SCREEN ANIMATION ⭐
    ESP_LOGI(TAG, "Starting boot screen animation");
    esp_err_t ret = uboot_screen_start(&display);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Boot screen failed to start, continuing without animation");
    }

    // Continue initialization while animation plays in background
    ESP_LOGI(TAG, "Initializing peripherals...");
    config_and_init_nrf24();

    ESP_LOGI(TAG, "Initializing SD card...");
    config_and_init_sd_card();

    ESP_LOGI(TAG, "Initializing LVGL GUI...");
    uGui_init(&display);

    // Wait for boot screen animation to complete (or timeout after 4 seconds)
    int wait_count = 0;
    while (uboot_screen_is_running() && wait_count < 40)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }

    // Stop boot screen if still running
    if (uboot_screen_is_running())
    {
        ESP_LOGI(TAG, "Stopping boot screen");
        uboot_screen_stop();
        vTaskDelay(pdMS_TO_TICKS(100)); // Give it time to clean up
    }

    ESP_LOGI(TAG, "Loading applications...");
    register_builtin_apps();

    ESP_LOGI(TAG, "uFlake Core initialized successfully with boot screen!");
}

/**
 * ALTERNATIVE: Simpler integration without waiting
 * The boot screen will auto-complete after 3 seconds
 */
void uflake_core_init_simple(void)
{
    uflake_kernel_init();
    uflake_kernel_start();
    unvs_init();
    i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ);
    uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 32768);
    uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096);

    config_and_init_display();

    // Start boot screen - it will run and auto-stop after 3 seconds
    uboot_screen_start(&display);

    // Do rest of init (boot screen continues in background)
    config_and_init_nrf24();
    config_and_init_sd_card();
    uGui_init(&display);
    register_builtin_apps();

    ESP_LOGI(TAG, "uFlake Core initialized successfully");
}

/**
 * ALTERNATIVE: Early boot splash (before LVGL)
 * Show animation immediately, then transition to LVGL
 */
void uflake_core_init_early_splash(void)
{
    uflake_kernel_init();
    uflake_kernel_start();
    unvs_init();
    i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ);
    uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 32768);
    uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096);

    config_and_init_display();

    // Show splash immediately
    uboot_screen_start(&display);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Show for 2 seconds
    uboot_screen_stop();

    // Now initialize LVGL and continue
    config_and_init_nrf24();
    config_and_init_sd_card();
    uGui_init(&display);
    register_builtin_apps();

    ESP_LOGI(TAG, "uFlake Core initialized successfully");
}
