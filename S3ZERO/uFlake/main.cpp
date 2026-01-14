#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "kernel.h"
#include "uI2c.h"
#include "uSPI.h"
#include "unvs.h"
#include "uUART.h"
#include "appLoader.h"

#include "pca9555.h"
#include "sdCard.h"
#include "nrf24.h"

static const char *TAG = "MAIN";

// Process ID for uFlake kernel process management demonstration
static uint32_t input_process_pid = 0;

void input_read_task(void *arg)
{
    ESP_LOGI(TAG, "[INPUT_TASK] Starting input read task");

    // initialize PCA9555 as input
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for PCA9555 to stabilize

    ESP_LOGI(TAG, "[INPUT_TASK] Initialization complete, entering main loop");

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

void init_nrf24()
{
    NRF24_t nrf24_dev = {};
    nrf24_dev.cePin = GPIO_NUM_48;
    nrf24_dev.csnPin = GPIO_NUM_45;
    nrf24_dev.channel = 76;
    nrf24_dev.payload = 16;
    nrf24_dev.spiHost = USPI_HOST_SPI3;
    nrf24_dev.frequency = USPI_FREQ_20MHZ;
    nrf24_dev.status = 0; // initialize status to a known value

    if (!Nrf24_init(&nrf24_dev))
    {
        ESP_LOGE(TAG, "Failed to initialize NRF24L01+");
        return;
    }

    ESP_LOGI(TAG, "NRF24L01+ initialized successfully");

    // check connection
    if (Nrf24_isConnected(&nrf24_dev))
    {
        ESP_LOGI(TAG, "NRF24L01+ is connected");
    }
    else
    {
        ESP_LOGE(TAG, "NRF24L01+ is NOT connected");
    }
}

extern const app_bundle_t counter_app; // From Apps/counter_app/app_main.c

extern "C"
{
    void app_main(void)
    {

        printf("Internal heap: %zu bytes\n", (size_t)esp_get_free_heap_size());
        printf("PSRAM heap: %zu bytes\n",
               (size_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

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

        // INITIALIZE the FIRST SPI BUS  - before adding any devices
        if (uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 4096) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SPI bus");
            return;
        }

        // INITIALIZE the Second SPI BUS FIRST - before adding any devices
        if (uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SPI bus");
            return;
        }

        uflake_result_t result = uflake_process_create(
            "InputReader",           // Process name
            input_read_task,         // Entry point
            NULL,                    // Arguments
            4096,                    // Stack size
            PROCESS_PRIORITY_NORMAL, // Kernel priority enum
            &input_process_pid       // Returns PID
        );
        if (result != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to create Input Reader process");
            return;
        }

        // init_nrf24();

        // initialize SD card
        SD_CardConfig sd_config = {};
        sd_config.csPin = GPIO_NUM_39;
        sd_config.clockSpeedHz = USPI_FREQ_1MHZ;
        sd_config.host = USPI_HOST_SPI2;

        if (!sdCard_init(&sd_config))
        {
            ESP_LOGE(TAG, "Failed to initialize SD card");
            return;
        }

        app_loader_init();
        app_loader_launch(app_loader_register(&counter_app)); // Launch counter app

        // Create a process using uflake kernel to handle inputs
        // uflake_process_create("InputsProcess", input_read_task, NULL, 2048, PROCESS_PRIORITY_NORMAL, NULL);
    }
}