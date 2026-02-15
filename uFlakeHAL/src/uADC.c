#include "uADC.h"
#include "esp_err.h"
#include <stdbool.h>
#include <string.h>

static const char *TAG = "uADC";

// ============================================================================
// Continuous Mode Variables
// ============================================================================
static adc_continuous_handle_t continuous_handle = NULL;
static bool continuous_initialized = false;
static adc_conv_done_callback_t user_callback = NULL;

// ============================================================================
// Oneshot Mode Variables
// ============================================================================
static adc_oneshot_unit_handle_t oneshot_handle_unit1 = NULL;
static adc_oneshot_unit_handle_t oneshot_handle_unit2 = NULL;
static bool oneshot_unit1_initialized = false;
static bool oneshot_unit2_initialized = false;

// ============================================================================
// Continuous Mode Implementation
// ============================================================================

static bool IRAM_ATTR adc_conv_done_isr_callback(adc_continuous_handle_t handle,
                                                 const adc_continuous_evt_data_t *edata,
                                                 void *user_data)
{
    if (user_callback != NULL && edata->conv_frame_buffer != NULL)
    {
        user_callback(edata->conv_frame_buffer, edata->size);
    }
    return false; // Don't yield
}

esp_err_t uADC_init_continuous(uadc_unit_t _unit, uadc_channel_t *channel,
                               adc_frequency_t frequency, adc_conv_done_callback_t callback)
{
    // Validate that only ADC1 is used (ESP32-S3 only supports ADC1 for continuous mode)
    if (_unit != UNIT_1)
    {
        UFLAKE_LOGE(TAG, "Continuous mode only supports ADC1 on ESP32-S3");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (continuous_initialized)
    {
        UFLAKE_LOGW(TAG, "Continuous mode already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Configure ADC continuous handle
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = ADC_READ_LEN,
    };

    esp_err_t ret = adc_continuous_new_handle(&adc_config, &continuous_handle);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create continuous handle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure single unit and single channel
    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN,
        .channel = *channel,
        .unit = ADC_UNIT_1, // Must be ADC1 for continuous mode
        .bit_width = ADC_BITWIDTH};

    // Configure ADC digital controller
    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1, // Only one channel pattern
        .adc_pattern = &adc_pattern,
        .sample_freq_hz = frequency,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_OUTPUT_TYPE,
    };

    ret = adc_continuous_config(continuous_handle, &dig_cfg);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to configure continuous mode: %s", esp_err_to_name(ret));
        adc_continuous_deinit(continuous_handle);
        continuous_handle = NULL;
        return ret;
    }

    // Register callback if provided
    if (callback != NULL)
    {
        user_callback = callback;
        adc_continuous_evt_cbs_t cbs = {
            .on_conv_done = adc_conv_done_isr_callback,
        };
        ret = adc_continuous_register_event_callbacks(continuous_handle, &cbs, NULL);
        if (ret != ESP_OK)
        {
            UFLAKE_LOGW(TAG, "Failed to register callback: %s", esp_err_to_name(ret));
        }
    }

    continuous_initialized = true;
    UFLAKE_LOGI(TAG, "Continuous mode initialized on ADC1 Channel %d at %d Hz", *channel, frequency);
    return ESP_OK;
}

esp_err_t uADC_start_continuous(void)
{
    if (!continuous_initialized || continuous_handle == NULL)
    {
        UFLAKE_LOGE(TAG, "Continuous mode not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = adc_continuous_start(continuous_handle);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to start continuous mode: %s", esp_err_to_name(ret));
        return ret;
    }

    UFLAKE_LOGI(TAG, "Continuous mode started");
    return ESP_OK;
}

esp_err_t uADC_stop_continuous(void)
{
    if (!continuous_initialized || continuous_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = adc_continuous_stop(continuous_handle);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to stop continuous mode: %s", esp_err_to_name(ret));
        return ret;
    }

    UFLAKE_LOGI(TAG, "Continuous mode stopped");
    return ESP_OK;
}

uint16_t uADC_get_continuous_value(uadc_channel_t *channel)
{
    if (!continuous_initialized || continuous_handle == NULL)
    {
        UFLAKE_LOGE(TAG, "Continuous mode not initialized");
        return 0;
    }

    uint8_t data[ADC_READ_LEN];
    uint32_t bytes_read = 0;

    // Read ADC data (non-blocking with 0 timeout)
    esp_err_t err = adc_continuous_read(continuous_handle, data, sizeof(data), &bytes_read, 0);
    if (err != ESP_OK)
    {
        if (err != ESP_ERR_TIMEOUT)
        {
            UFLAKE_LOGW(TAG, "Read error: %s", esp_err_to_name(err));
        }
        return 0;
    }

    // Process the read data to extract the value for the specified channel
    for (size_t i = 0; i < bytes_read; i += sizeof(adc_digi_output_data_t))
    {
        adc_digi_output_data_t *p_data = (adc_digi_output_data_t *)(data + i);
        if (ADC_GET_CHANNEL(p_data) == *channel)
        {
            return ADC_GET_DATA(p_data);
        }
    }

    return 0; // Channel not found in the read data
}

// ============================================================================
// Oneshot Mode Implementation
// ============================================================================

esp_err_t uADC_init_oneshot(uadc_unit_t _unit)
{
    adc_oneshot_unit_handle_t *handle_ptr = NULL;
    bool *init_flag = NULL;

    // Select the appropriate handle based on unit
    if (_unit == UNIT_1)
    {
        handle_ptr = &oneshot_handle_unit1;
        init_flag = &oneshot_unit1_initialized;
    }
    else if (_unit == UNIT_2)
    {
        handle_ptr = &oneshot_handle_unit2;
        init_flag = &oneshot_unit2_initialized;
    }
    else
    {
        UFLAKE_LOGE(TAG, "Invalid ADC unit");
        return ESP_ERR_INVALID_ARG;
    }

    if (*init_flag)
    {
        UFLAKE_LOGW(TAG, "ADC%d oneshot already initialized", _unit == UNIT_1 ? 1 : 2);
        return ESP_OK;
    }

    // Initialize ADC oneshot unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = _unit,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, handle_ptr);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to initialize ADC%d oneshot: %s",
                    _unit == UNIT_1 ? 1 : 2, esp_err_to_name(ret));
        return ret;
    }

    *init_flag = true;
    UFLAKE_LOGI(TAG, "ADC%d oneshot initialized", _unit == UNIT_1 ? 1 : 2);
    return ESP_OK;
}

esp_err_t uADC_read_oneshot(uadc_unit_t _unit, uadc_channel_t channel, int *raw_value)
{
    adc_oneshot_unit_handle_t handle = NULL;
    bool initialized = false;

    // Select the appropriate handle based on unit
    if (_unit == UNIT_1)
    {
        handle = oneshot_handle_unit1;
        initialized = oneshot_unit1_initialized;
    }
    else if (_unit == UNIT_2)
    {
        handle = oneshot_handle_unit2;
        initialized = oneshot_unit2_initialized;
    }
    else
    {
        UFLAKE_LOGE(TAG, "Invalid ADC unit");
        return ESP_ERR_INVALID_ARG;
    }

    if (!initialized || handle == NULL)
    {
        UFLAKE_LOGE(TAG, "ADC%d oneshot not initialized", _unit == UNIT_1 ? 1 : 2);
        return ESP_ERR_INVALID_STATE;
    }

    // Configure channel (attenuation)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };

    esp_err_t ret = adc_oneshot_config_channel(handle, channel, &config);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to configure channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read ADC value
    ret = adc_oneshot_read(handle, channel, raw_value);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t uADC_deinit_oneshot(uadc_unit_t _unit)
{
    adc_oneshot_unit_handle_t *handle_ptr = NULL;
    bool *init_flag = NULL;

    // Select the appropriate handle based on unit
    if (_unit == UNIT_1)
    {
        handle_ptr = &oneshot_handle_unit1;
        init_flag = &oneshot_unit1_initialized;
    }
    else if (_unit == UNIT_2)
    {
        handle_ptr = &oneshot_handle_unit2;
        init_flag = &oneshot_unit2_initialized;
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!*init_flag || *handle_ptr == NULL)
    {
        return ESP_OK;
    }

    esp_err_t ret = adc_oneshot_del_unit(*handle_ptr);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to deinitialize ADC%d: %s",
                    _unit == UNIT_1 ? 1 : 2, esp_err_to_name(ret));
        return ret;
    }

    *handle_ptr = NULL;
    *init_flag = false;
    UFLAKE_LOGI(TAG, "ADC%d oneshot deinitialized", _unit == UNIT_1 ? 1 : 2);
    return ESP_OK;
}
