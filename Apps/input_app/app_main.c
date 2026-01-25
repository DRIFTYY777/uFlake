#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "../../uAppLoader/appLoader.h"
#include "uI2c.h"

#include "pca9555.h"

static const char *TAG = "InputApp";
void input_app_main(void);

static const app_manifest_t input_app_manifest = {
    .name = "InputApp",
    .version = "1.0.0",
    .author = "uFlake Team",
    .description = "Input Handling Test App",
    .icon = "input.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 4096,
    .priority = 5,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

const app_bundle_t input_app = {
    .manifest = &input_app_manifest,
    .entry_point = input_app_main,
    .is_launcher = false};

void input_app_main(void)
{
    ESP_LOGI(TAG, "Starting input read task");

    // initialize PCA9555 as input
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for PCA9555 to stabilize

    while (1)
    {
        // Watchdog is automatically fed by the kernel
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
            // Not used
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
        vTaskDelay(pdMS_TO_TICKS(50)); // Polling delay
    }
}
