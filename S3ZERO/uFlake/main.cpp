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

static const char *TAG = "MAIN";

#define PCA9555_ADDRESS 0x20 // Default I2C address for PCA9555

// PCA9555 Register Addresses
#define PCA9555_INPUT_PORT_0 0x00
#define PCA9555_INPUT_PORT_1 0x01
#define PCA9555_OUTPUT_PORT_0 0x02
#define PCA9555_OUTPUT_PORT_1 0x03
#define PCA9555_POLARITY_INV_0 0x04
#define PCA9555_POLARITY_INV_1 0x05
#define PCA9555_CONFIG_0 0x06
#define PCA9555_CONFIG_1 0x07

#define PCA9555_NORMAL_MODE 0x00
#define PCA9555_INVERTED_MODE 0xFF

#define PCA9555_DiRECTION_INPUT 0x00
#define PCA9555_DiRECTION_OUTPUT 0xFF

// initialize PCA9555 as buttons input
void init_pca9555_as_input(i2c_port_t port, uint8_t device_address)
{
    // add device to I2C bus
    i2c_bus_manager_add_device(port, device_address);
    // Set all pins as input
    i2c_manager_write_reg_bytes(port, device_address, PCA9555_CONFIG_0, (uint8_t[]){PCA9555_DiRECTION_INPUT, PCA9555_DiRECTION_INPUT}, 2);
    // Set polarity inversion if needed
    i2c_manager_write_reg_bytes(port, device_address, PCA9555_POLARITY_INV_0, (uint8_t[]){PCA9555_NORMAL_MODE, PCA9555_NORMAL_MODE}, 2);
}

uint16_t read_pca9555_inputs(i2c_port_t port, uint8_t device_address)
{
    uint8_t input_data[2] = {0};
    i2c_manager_read_reg_bytes(port, device_address, PCA9555_INPUT_PORT_0, input_data, 2);
    return (input_data[1] << 8) | input_data[0];
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

        // initialize I2C
        if (i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2C bus");
            return;
        }

        // initialize PCA9555 as input
        init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);
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
}