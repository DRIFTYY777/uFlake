#include <stdio.h>
#include "appLoader.h"
#include "logger.h"
#include "uHal.h"

static const char *TAG = "ADC_APP";

// Choose which mode to use (comment/uncomment as needed)
#define USE_CONTINUOUS_MODE 1    // Set to 1 for continuous mode, 0 for single-shot mode
#define USE_INTERRUPT_CALLBACK 0 // Set to 1 to use interrupt callback in continuous mode

// ============================================================================
// APP MANIFEST - Define metadata for this app
// ============================================================================
static const app_manifest_t counter_manifest = {
    .name = "ADC Reader",
    .version = "2.0.0",
    .author = "DRIFTYY",
    .description = "Reads ADC values from GPIO4",
    .icon = "adc_reader.png",
    .type = APP_TYPE_INTERNAL,
    .stack_size = 4096,
    .priority = 5,
    .requires_gui = true,
    .requires_sdcard = false,
    .requires_network = false};

// Forward declare entry point
void adc_reader_app_main(void);

// Export app bundle for registration (single line registration in main.cpp)
const app_bundle_t adc_reader_app = {
    .manifest = &counter_manifest,
    .entry_point = adc_reader_app_main,
    .is_launcher = false};

#if USE_INTERRUPT_CALLBACK && USE_CONTINUOUS_MODE
// Callback function for continuous mode (called from ISR context)
static void adc_callback(uint8_t *data, uint32_t length)
{
    // Note: This runs in ISR context, keep it minimal
    // For real processing, trigger a task or set a flag
    static uint32_t callback_count = 0;
    callback_count++;
}
#endif

void adc_reader_app_main(void)
{
    // GPIO4 is ADC1_CHANNEL_3 on ESP32-S3
    uadc_channel_t channel = CHANNEL_3; // GPIO4

    UFLAKE_LOGI(TAG, "Starting ADC Reader App - Reading from GPIO4 (ADC1_CHANNEL_3)");

#if USE_CONTINUOUS_MODE
    // ========================================================================
    // Continuous Mode Example (High-speed DMA-based sampling)
    // ========================================================================
    UFLAKE_LOGI(TAG, "Initializing ADC in CONTINUOUS mode");

#if USE_INTERRUPT_CALLBACK
    // Initialize with interrupt callback
    esp_err_t ret = uADC_init_continuous(UNIT_1, &channel, FREQ_10KHz, adc_callback);
#else
    // Initialize without callback (polling mode)
    esp_err_t ret = uADC_init_continuous(UNIT_1, &channel, FREQ_10KHz, NULL);
#endif

    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize continuous mode: %d", ret);
        return;
    }

    ret = uADC_start_continuous();
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to start continuous mode: %d", ret);
        return;
    }

    UFLAKE_LOGI(TAG, "Continuous mode started. Reading ADC values...");

    while (true)
    {
        uint16_t adc_value = uADC_get_continuous_value(&channel);

        // Convert to voltage (assuming 12-bit ADC with 3.3V reference and 11dB attenuation)
        // With 11dB attenuation: 0-2500mV range (approximately)
        float voltage = (adc_value / 4095.0f) * 3.3f;

        UFLAKE_LOGI(TAG, "GPIO4 - Raw: %4d, Voltage: %.3f V", adc_value, voltage);
        vTaskDelay(pdMS_TO_TICKS(500)); // Read every 500ms
    }

#else
    // ========================================================================
    // Single-Shot Mode Example (On-demand sampling, lower power)
    // ========================================================================
    UFLAKE_LOGI(TAG, "Initializing ADC in SINGLE-SHOT mode");

    esp_err_t ret = uADC_init_oneshot(UNIT_1);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize oneshot mode: %d", ret);
        return;
    }

    UFLAKE_LOGI(TAG, "Single-shot mode initialized. Reading ADC values...");

    while (true)
    {
        int adc_raw = 0;
        ret = uADC_read_oneshot(UNIT_1, channel, &adc_raw);

        if (ret == ESP_OK)
        {
            // Convert to voltage
            float voltage = (adc_raw / 4095.0f) * 2.5f;
            UFLAKE_LOGI(TAG, "GPIO4 - Raw: %4d, Voltage: %.3f V", adc_raw, voltage);
        }
        else
        {
            UFLAKE_LOGE(TAG, "Failed to read ADC: %d", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Read every 1 second
    }
#endif
}
