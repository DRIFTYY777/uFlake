#include <stdio.h>
#include "appLoader_simple.h"
#include "esp_log.h"
#include "uADC.h"

static const char *TAG = "ADC_APP";

// ============================================================================
// APP MANIFEST - Define metadata for this app
// ============================================================================
static const app_manifest_t adc_manifest = {
    .name = "ADC Reader",
    .version = "2.0.0",
    .author = "DRIFTYY",
    .description = "Reads ADC values from GPIO4",
    .icon = "adc_reader.png",
    .category = APP_CAT_UTILITY,
    .subtype = APP_SUBTYPE_NON_GUI,
    .stack_size = 4096,
    .priority = PROCESS_PRIORITY_NORMAL,
    .min_ram_bytes = 4096,
    .requires_sdcard = false,
    .requires_network = false
};

// Forward declare entry point
void adc_reader_app_main(void);

// Export app bundle for registration
const app_bundle_t adc_reader_app = {
    .manifest = adc_manifest,
    .entry_point = adc_reader_app_main,
    .is_launcher = false
};

void adc_reader_app_main(void)
{
    ESP_LOGI(TAG, "Starting ADC Reader App");

    // GPIO4 is ADC1 channel 3 on ESP32-S3
    esp_err_t init_ret = uADC_init_oneshot(UNIT_1);
    if (init_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC oneshot: %s", esp_err_to_name(init_ret));
        return;
    }

    ESP_LOGI(TAG, "ADC initialized - Reading GPIO4 (ADC1_CHANNEL_3)");

    for (int i = 0; i < 50; i++)
    {
        int adc_value = 0;
        esp_err_t read_ret = uADC_read_oneshot(UNIT_1, CHANNEL_3, &adc_value);
        if (read_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(read_ret));
            break;
        }
        float voltage = (adc_value / 4095.0f) * 3.3f;

        ESP_LOGI(TAG, "GPIO4 - Raw: %4d, Voltage: %.3f V", adc_value, voltage);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGI(TAG, "ADC Reader App completed");
}
