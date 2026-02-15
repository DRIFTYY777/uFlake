#if !defined(U_ADC_H)
#define U_ADC_H

#include "hal/adc_types.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include <stddef.h>

#include "kernel.h"

// ADC Configuration
#define ADC_ATTEN ADC_ATTEN_DB_12    // 0-3.3V range (0-2500mV)
#define ADC_BITWIDTH ADC_BITWIDTH_12 // Full 12-bit resolution (0-4095)

// Continuous mode settings
#define ADC_NUM_CHANNELS 10          // Number of ADC channels
#define ADC_READ_LEN 300             // Buffer size for reading
#define ADC_RING_BUFFER_SAMPLES 1024 // Per-channel ring buffer size

#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2

#define ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
#define ADC_GET_DATA(p_data) ((p_data)->type2.data)

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        FREQ_1KHz = 1000,
        FREQ_5KHz = 5000,
        FREQ_10KHz = 10000,
        FREQ_20KHz = 20000
    } adc_frequency_t;

    typedef enum
    {
        UNIT_1 = ADC_UNIT_1, // ADC1: GPIO1-GPIO10 (ESP32-S3)
        UNIT_2 = ADC_UNIT_2  // ADC2: GPIO11-GPIO20 (ESP32-S3) - Cannot use in continuous mode
    } uadc_unit_t;

    /*
     ESP32-S3 ADC Channel to GPIO Mapping:

     ADC1 (GPIO1-GPIO10):
     ADC1_CHANNEL_0 = GPIO1
     ADC1_CHANNEL_1 = GPIO2
     ADC1_CHANNEL_2 = GPIO3
     ADC1_CHANNEL_3 = GPIO4  <-- Your GPIO4
     ADC1_CHANNEL_4 = GPIO5
     ADC1_CHANNEL_5 = GPIO6
     ADC1_CHANNEL_6 = GPIO7
     ADC1_CHANNEL_7 = GPIO8
     ADC1_CHANNEL_8 = GPIO9
     ADC1_CHANNEL_9 = GPIO10

     ADC2 (GPIO11-GPIO20):
     ADC2_CHANNEL_0 = GPIO11
     ADC2_CHANNEL_1 = GPIO12
     ADC2_CHANNEL_2 = GPIO13
     ADC2_CHANNEL_3 = GPIO14
     ADC2_CHANNEL_4 = GPIO15
     ADC2_CHANNEL_5 = GPIO16
     ADC2_CHANNEL_6 = GPIO17
     ADC2_CHANNEL_7 = GPIO18
     ADC2_CHANNEL_8 = GPIO19
     ADC2_CHANNEL_9 = GPIO20

     Note: For continuous/DMA mode, only ADC1 is supported.
           ADC2 can only be used in single-shot (oneshot) mode.
    */
    typedef enum
    {
        CHANNEL_0 = ADC_CHANNEL_0,
        CHANNEL_1 = ADC_CHANNEL_1,
        CHANNEL_2 = ADC_CHANNEL_2,
        CHANNEL_3 = ADC_CHANNEL_3,
        CHANNEL_4 = ADC_CHANNEL_4,
        CHANNEL_5 = ADC_CHANNEL_5,
        CHANNEL_6 = ADC_CHANNEL_6,
        CHANNEL_7 = ADC_CHANNEL_7,
        CHANNEL_8 = ADC_CHANNEL_8,
        CHANNEL_9 = ADC_CHANNEL_9
    } uadc_channel_t;

    // Callback type for continuous mode interrupt
    typedef void (*adc_conv_done_callback_t)(uint8_t *data, uint32_t length);

    // ============================================================================
    // Continuous Mode Functions (ADC1 only)
    // ============================================================================

    /**
     * @brief Initialize ADC in continuous mode with optional callback
     * @param _unit ADC unit (must be UNIT_1 for ESP32-S3)
     * @param channel Pointer to channel to read
     * @param frequency Sampling frequency
     * @param callback Optional callback function called when conversion is done (can be NULL)
     * @return ESP_OK on success
     */
    esp_err_t uADC_init_continuous(uadc_unit_t _unit, uadc_channel_t *channel,
                                   adc_frequency_t frequency, adc_conv_done_callback_t callback);

    /**
     * @brief Start continuous ADC conversion
     * @return ESP_OK on success
     */
    esp_err_t uADC_start_continuous(void);

    /**
     * @brief Stop continuous ADC conversion
     * @return ESP_OK on success
     */
    esp_err_t uADC_stop_continuous(void);

    /**
     * @brief Get channel value from continuous mode
     * @param channel Pointer to channel to read
     * @return ADC value (0-4095) or 0 on error
     */
    uint16_t uADC_get_continuous_value(uadc_channel_t *channel);

    // ============================================================================
    // Single-Shot Mode Functions (ADC1 and ADC2)
    // ============================================================================

    /**
     * @brief Initialize ADC in single-shot (oneshot) mode
     * @param _unit ADC unit (UNIT_1 or UNIT_2)
     * @return ESP_OK on success
     */
    esp_err_t uADC_init_oneshot(uadc_unit_t _unit);

    /**
     * @brief Read single ADC value in oneshot mode
     * @param _unit ADC unit
     * @param channel Channel to read
     * @param raw_value Pointer to store raw ADC value
     * @return ESP_OK on success
     */
    esp_err_t uADC_read_oneshot(uadc_unit_t _unit, uadc_channel_t channel, int *raw_value);

    /**
     * @brief Deinitialize oneshot ADC
     * @param _unit ADC unit
     * @return ESP_OK on success
     */
    esp_err_t uADC_deinit_oneshot(uadc_unit_t _unit);

#ifdef __cplusplus
}
#endif

#endif // U_ADC_H
