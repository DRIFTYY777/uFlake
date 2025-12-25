#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "sdkconfig.h"

#include "uSPI.h"
#include "ST7789_driver.h"

static const char *TAG = "MAIN";

st7789_t display = {
    .pin_reset = GPIO_NUM_46,
    .spi = nullptr,
    .width = 240,
    .height = 320,
};

void init_display()
{
    ESP_LOGI(TAG, "Initializing display...");

    // Initialize the backlight pin
    gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_3, 1); // Turn on backlight

    // Add display device to SPI bus
    uspi_device_config_t dev_cfg = {
        .cs_pin = GPIO_NUM_10,
        .clock_speed_hz = USPI_FREQ_10MHZ, // Start with lower speed for stability
        .mode = USPI_MODE_0,
        .queue_size = 1,
        .cs_ena_pretrans = true,
        .cs_ena_posttrans = true,
        .address_bits = 0,
        .command_bits = 0,
        .dummy_bits = 0,
        .device_type = USPI_DEVICE_DISPLAY,
        .device_name = "ST7789"};

    spi_device_handle_t spi_handle;
    esp_err_t ret = uspi_device_add(USPI_HOST_SPI3, &dev_cfg, &spi_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return;
    }

    // Prepare display struct
    st7789_t display = {
        .pin_reset = GPIO_NUM_46,
        .pin_dc = GPIO_NUM_14,
        .spi = spi_handle,
        .width = 240,
        .height = 320};

    // Initialize display
    if (st7789_init(&display))
    {
        ESP_LOGI(TAG, "Display initialized successfully");

        // Test the display
        st7789_fill_area(&display, 0x001F, 0, 0, display.width, display.height); // Blue
        vTaskDelay(pdMS_TO_TICKS(1000));

        st7789_fill_area(&display, 0xF800, 0, 0, display.width / 2, display.height / 2);                                  // Red rectangle
        st7789_fill_area(&display, 0x07E0, display.width / 2, display.height / 2, display.width / 2, display.height / 2); // Green rectangle
    }
    else
    {
        ESP_LOGE(TAG, "Display initialization failed");
    }
}

extern "C"
{
    void app_main(void)
    {
        // Initialize the kernel
        if (uflake_kernel_init() != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize uFlake Kernel");
            return;
        }

        // Start the kernel
        if (uflake_kernel_start() != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to start uFlake Kernel");
            return;
        }

        // INITIALIZE SPI BUS FIRST - before adding any devices
        if (uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 4096) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SPI bus");
            return;
        }

        // Initialize display
        init_display();
    }
}