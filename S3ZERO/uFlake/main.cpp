#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "kernel.h"
#include "uI2c.h"

static const char *TAG = "MAIN";

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

        // Initialize uI2C HAL
        if (i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_18, GPIO_NUM_19, UI2C_DEFAULT_FREQ_HZ) != true)
        {
            ESP_LOGE(TAG, "Failed to initialize uI2C HAL");
            return;
        }

        // sacn I2C bus for devices
        uint8_t found_devices[10] = {0};
        size_t found_count = 0;
        if (i2c_bus_manager_scan(UI2C_PORT_0, found_devices,
                                 sizeof(found_devices), &found_count) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found %d I2C devices:", (int)found_count);
            for (size_t i = 0; i < found_count; i++)
            {
                ESP_LOGI(TAG, " - Device at address 0x%02X", found_devices[i]);
            }
        }
        else
        {
            ESP_LOGE(TAG, "I2C bus scan failed");
        }
    }
}