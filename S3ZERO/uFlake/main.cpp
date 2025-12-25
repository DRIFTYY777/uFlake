#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "sdkconfig.h"
#include "kernel.h"
#include "uI2c.h"
#include "uSPI.h"
#include "unvs.h"

#include "ST7789_driver.h"
#include "pca9555.h"

static const char *TAG = "MAIN";

void input_read_task(void *arg)
{
    // initialize PCA9555 as input
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for PCA9555 to stabilize

    while (1)
    {
        uint16_t inputs_value = read_pca9555_inputs(UI2C_PORT_0, PCA9555_ADDRESS);
        if (!((inputs_value >> 0) & 0x01))
        {
            ESP_LOGI(TAG, "_Up pressed");
        }

        if (!((inputs_value >> 1) & 0x01))
        {
            ESP_LOGI(TAG, "_Down pressed");
        }

        if (!((inputs_value >> 2) & 0x01))
        {
            ESP_LOGI(TAG, "_Right pressed");
        }

        if (!((inputs_value >> 3) & 0x01))
        {
            ESP_LOGI(TAG, "_Left pressed");
        }

        if (!((inputs_value >> 4) & 0x01))
        {
            // Serial.println("isnt used"); // isnt used
        }

        if (!((inputs_value >> 5) & 0x01))
        {
            ESP_LOGI(TAG, "_Menu pressed");
        }

        if (!((inputs_value >> 6) & 0x01))
        {
            ESP_LOGI(TAG, "_Back pressed");
        }

        if (!((inputs_value >> 7) & 0x01))
        {
            ESP_LOGI(TAG, "_OK pressed");
        }

        if (!((inputs_value >> 8) & 0x01))
        {
            ESP_LOGI(TAG, "_Home pressed");
        }

        if (!((inputs_value >> 9) & 0x01))
        {
            ESP_LOGI(TAG, "_A pressed");
        }
        if (!((inputs_value >> 10) & 0x01))
        {
            ESP_LOGI(TAG, "_B pressed");
        }

        if (!((inputs_value >> 11) & 0x01))
        {
            ESP_LOGI(TAG, "_Y pressed");
        }
        if (!((inputs_value >> 12) & 0x01))
        {
            ESP_LOGI(TAG, "_X pressed");
        }

        if (!((inputs_value >> 13) & 0x01))
        {
            ESP_LOGI(TAG, "_L1 pressed");
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100 ms
    }
}

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

        // initialize nvs subsystem
        if (unvs_init() != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize NVS subsystem");
            return;
        }

        // initialize I2C
        if (i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2C bus");
            return;
        }

        // read and write test
        const char *test_namespace = "test_ns";
        const char *test_key = "test_key";
        const char *test_value = "Hello, uFlake!";
        if (unvs_write_string(test_namespace, test_key, test_value) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write string to NVS");
            return;
        }

        char read_value[50];
        size_t len = sizeof(read_value);
        if (unvs_read_string(test_namespace, test_key, read_value, &len) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read string from NVS");
            return;
        }

        ESP_LOGI(TAG, "uFlake OS started successfully");

        // INITIALIZE SPI BUS FIRST - before adding any devices
        if (uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 4096) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SPI bus");
            return;
        }

        // Initialize display
        init_display();

        // Create a process using uflake kernel to handle inputs
        uflake_process_create("InputsProcess", input_read_task, NULL, 2048, PROCESS_PRIORITY_NORMAL, NULL);
    }
}