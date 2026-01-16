#include "uUART.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UART_HAL";

// Event task function prototype
static void uart_event_task(void *pvParameters);

// NOTE: This HAL driver uses FreeRTOS directly for hardware-level operations because:
// 1. ESP-IDF's uart_driver_install() returns a raw FreeRTOS QueueHandle_t
// 2. xQueueReceive/xQueueReset are needed to interact with ESP-IDF's UART event queue
// 3. HAL is the low-level hardware interface - kernel abstraction is for app-level tasks
// 4. Adding wrappers here would create unnecessary overhead for real-time operations

// Get default configuration
void uflake_uart_get_default_config(uflake_uart_config_t *config, uart_port_t port)
{
    if (config == NULL)
    {
        return;
    }

    config->port = port;
    config->tx_pin = UART_TX_PIN_DEFAULT;
    config->rx_pin = UART_RX_PIN_DEFAULT;
    config->rts_pin = UART_RTS_PIN_DEFAULT;
    config->cts_pin = UART_CTS_PIN_DEFAULT;
    config->baud_rate = UART_BAUD_RATE_DEFAULT;
    config->data_bits = UART_DATA_8_BITS;
    config->parity = UART_PARITY_DISABLE;
    config->stop_bits = UART_STOP_BITS_1;
    config->flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config->rx_flow_ctrl_thresh = 122;
    config->rx_buffer_size = UART_RX_BUF_SIZE;
    config->tx_buffer_size = UART_TX_BUF_SIZE;
    config->event_queue_size = UART_QUEUE_SIZE;
    config->rx_threshold = 1;       // Trigger interrupt after 1 bytes
    config->use_isr_in_iram = true; // Place ISR in IRAM for speed
    config->enable_pattern_detect = false;
    config->pattern_char = '\n';
    config->pattern_count = 1;
}

// Initialize UART with event-driven approach
uflake_result_t uflake_uart_init(uflake_uart_handle_t *handle, const uflake_uart_config_t *config)
{
    if (handle == NULL || config == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (handle->is_initialized)
    {
        ESP_LOGW(TAG, "UART port %d already initialized", config->port);
        return UFLAKE_OK;
    }

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = config->data_bits,
        .parity = config->parity,
        .stop_bits = config->stop_bits,
        .flow_ctrl = config->flow_ctrl,
        .rx_flow_ctrl_thresh = config->rx_flow_ctrl_thresh,
        .source_clk = UART_SCLK_DEFAULT,
    };

    int intr_alloc_flags = 0;
    if (config->use_isr_in_iram)
    {
        intr_alloc_flags = ESP_INTR_FLAG_IRAM;
    }

    // Install UART driver with event queue
    esp_err_t err = uart_driver_install(config->port,
                                        config->rx_buffer_size,
                                        config->tx_buffer_size,
                                        config->event_queue_size,
                                        &handle->event_queue,
                                        intr_alloc_flags);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    // Configure UART parameters
    err = uart_param_config(config->port, &uart_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(err));
        uart_driver_delete(config->port);
        return UFLAKE_ERROR;
    }

    // Set UART pins
    err = uart_set_pin(config->port, config->tx_pin, config->rx_pin, config->rts_pin, config->cts_pin);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        uart_driver_delete(config->port);
        return UFLAKE_ERROR;
    }

    // Initialize handle
    handle->port = config->port;
    handle->is_initialized = true;
    handle->rx_callback = NULL;
    handle->error_callback = NULL;
    handle->isr_rx_callback = NULL;
    handle->pattern_callback = NULL;
    handle->pattern_buffer = NULL;
    handle->pattern_buffer_size = 0;

    // Set RX FIFO threshold for interrupt
    err = uart_set_rx_full_threshold(config->port, config->rx_threshold);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to set RX threshold: %s", esp_err_to_name(err));
    }

    // Enable pattern detection if requested
    if (config->enable_pattern_detect)
    {
        err = uart_enable_pattern_det_baud_intr(config->port, config->pattern_char, config->pattern_count, 9, 0, 0);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to enable pattern detection: %s", esp_err_to_name(err));
        }
        else
        {
            // Allocate pattern buffer
            handle->pattern_buffer = (uint8_t *)uflake_malloc(config->rx_buffer_size, UFLAKE_MEM_INTERNAL);
            if (handle->pattern_buffer)
            {
                handle->pattern_buffer_size = config->rx_buffer_size;
            }
        }
    }

    // Create event task using uFlake kernel
    uint32_t pid;
    uflake_result_t result = uflake_process_create(
        "uart_event_task",
        uart_event_task,
        handle,
        UART_EVENT_TASK_STACK_SIZE,
        PROCESS_PRIORITY_NORMAL,
        &pid);

    if (result != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create UART event task");
        uart_driver_delete(config->port);
        handle->is_initialized = false;
        return UFLAKE_ERROR_MEMORY;
    }

    // Get the task handle from FreeRTOS for later deletion
    handle->event_task_handle = xTaskGetHandle("uart_event_task");

    ESP_LOGI(TAG, "UART%d initialized (TX=%d, RX=%d, Baud=%d)",
             config->port, config->tx_pin, config->rx_pin, config->baud_rate);

    return UFLAKE_OK;
}

// Deinitialize UART
uflake_result_t uflake_uart_deinit(uflake_uart_handle_t *handle)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    // Delete event task
    if (handle->event_task_handle != NULL)
    {
        vTaskDelete(handle->event_task_handle);
        handle->event_task_handle = NULL;
    }

    // Free pattern buffer if allocated
    if (handle->pattern_buffer != NULL)
    {
        uflake_free(handle->pattern_buffer);
        handle->pattern_buffer = NULL;
    }

    // Uninstall UART driver
    esp_err_t err = uart_driver_delete(handle->port);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete UART driver: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    handle->is_initialized = false;
    handle->event_queue = NULL;
    handle->rx_callback = NULL;
    handle->error_callback = NULL;
    handle->isr_rx_callback = NULL;
    handle->pattern_callback = NULL;

    ESP_LOGI(TAG, "UART%d deinitialized", handle->port);
    return UFLAKE_OK;
}

// Write data to UART
uflake_result_t uflake_uart_write(uflake_uart_handle_t *handle, const uint8_t *data, size_t len)
{
    if (handle == NULL || !handle->is_initialized || data == NULL)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    int bytes_written = uart_write_bytes(handle->port, (const char *)data, len);
    if (bytes_written < 0)
    {
        ESP_LOGE(TAG, "Failed to write to UART");
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

// Write string to UART
uflake_result_t uflake_uart_write_string(uflake_uart_handle_t *handle, const char *str)
{
    if (str == NULL)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }
    return uflake_uart_write(handle, (const uint8_t *)str, strlen(str));
}

// Write bytes with timeout
uflake_result_t uflake_uart_write_bytes(uflake_uart_handle_t *handle, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (handle == NULL || !handle->is_initialized || data == NULL)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    int bytes_written = uart_write_bytes(handle->port, (const char *)data, len);

    if (bytes_written < 0)
    {
        return UFLAKE_ERROR;
    }

    // Wait for transmission to complete
    esp_err_t err = uart_wait_tx_done(handle->port, ticks);
    if (err != ESP_OK)
    {
        return UFLAKE_ERROR_TIMEOUT;
    }

    return UFLAKE_OK;
}

// Read data from UART with timeout
uflake_result_t uflake_uart_read(uflake_uart_handle_t *handle, uint8_t *buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    if (handle == NULL || !handle->is_initialized || buffer == NULL)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    int read = uart_read_bytes(handle->port, buffer, len, ticks);

    if (read < 0)
    {
        if (bytes_read != NULL)
        {
            *bytes_read = 0;
        }
        return UFLAKE_ERROR;
    }

    if (bytes_read != NULL)
    {
        *bytes_read = read;
    }

    return (read > 0) ? UFLAKE_OK : UFLAKE_ERROR_TIMEOUT;
}

// Read data blocking until buffer is full
uflake_result_t uflake_uart_read_blocking(uflake_uart_handle_t *handle, uint8_t *buffer, size_t len, size_t *bytes_read)
{
    return uflake_uart_read(handle, buffer, len, bytes_read, portMAX_DELAY);
}

// Get available bytes in RX buffer
int uflake_uart_available(uflake_uart_handle_t *handle)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return 0;
    }

    size_t available = 0;
    esp_err_t err = uart_get_buffered_data_len(handle->port, &available);
    if (err != ESP_OK)
    {
        return 0;
    }

    return (int)available;
}

// Set baud rate
uflake_result_t uflake_uart_set_baud_rate(uflake_uart_handle_t *handle, int baud_rate)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    esp_err_t err = uart_set_baudrate(handle->port, baud_rate);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set baud rate: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "UART%d baud rate set to %d", handle->port, baud_rate);
    return UFLAKE_OK;
}

// Set pins
uflake_result_t uflake_uart_set_pins(uflake_uart_handle_t *handle, int tx, int rx, int rts, int cts)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    esp_err_t err = uart_set_pin(handle->port, tx, rx, rts, cts);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set pins: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

// Set UART mode
uflake_result_t uflake_uart_set_mode(uflake_uart_handle_t *handle, uart_mode_t mode)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    esp_err_t err = uart_set_mode(handle->port, mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set mode: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

// Register RX callback
uflake_result_t uflake_uart_register_rx_callback(uflake_uart_handle_t *handle, void (*callback)(uint8_t *data, size_t len))
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    handle->rx_callback = callback;
    return UFLAKE_OK;
}

// Register ISR RX callback (called from ISR context - must be IRAM_ATTR)
uflake_result_t uflake_uart_register_isr_callback(uflake_uart_handle_t *handle, void (*callback)(uint8_t *data, size_t len))
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    handle->isr_rx_callback = callback;
    ESP_LOGI(TAG, "ISR callback registered for UART%d (will be called from ISR context)", handle->port);
    return UFLAKE_OK;
}

// Register pattern detection callback
uflake_result_t uflake_uart_register_pattern_callback(uflake_uart_handle_t *handle, void (*callback)(int pos))
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    handle->pattern_callback = callback;
    return UFLAKE_OK;
}

// Register error callback
uflake_result_t uflake_uart_register_error_callback(uflake_uart_handle_t *handle, void (*callback)(uart_event_type_t error_type))
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    handle->error_callback = callback;
    return UFLAKE_OK;
}

// Flush all buffers
uflake_result_t uflake_uart_flush(uflake_uart_handle_t *handle)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    esp_err_t err = uart_flush(handle->port);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to flush: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

// Flush input buffer
uflake_result_t uflake_uart_flush_input(uflake_uart_handle_t *handle)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    esp_err_t err = uart_flush_input(handle->port);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to flush input: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    return UFLAKE_OK;
}

// Set RX threshold for interrupt trigger
uflake_result_t uflake_uart_set_rx_threshold(uflake_uart_handle_t *handle, int threshold)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (threshold < 1 || threshold > 127)
    {
        ESP_LOGE(TAG, "Invalid RX threshold: %d (must be 1-127)", threshold);
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    esp_err_t err = uart_set_rx_full_threshold(handle->port, threshold);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set RX threshold: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "UART%d RX threshold set to %d bytes", handle->port, threshold);
    return UFLAKE_OK;
}

// Enable pattern detection interrupt
uflake_result_t uflake_uart_enable_pattern_detect(uflake_uart_handle_t *handle, char pattern_char, uint8_t chr_num, int post_idle, int pre_idle)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (chr_num < 1 || chr_num > 127)
    {
        ESP_LOGE(TAG, "Invalid pattern count: %d (must be 1-127)", chr_num);
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    // Allocate pattern buffer if not already done
    if (handle->pattern_buffer == NULL)
    {
        handle->pattern_buffer = (uint8_t *)uflake_malloc(UART_RX_BUF_SIZE, UFLAKE_MEM_INTERNAL);
        if (handle->pattern_buffer == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate pattern buffer");
            return UFLAKE_ERROR_MEMORY;
        }
        handle->pattern_buffer_size = UART_RX_BUF_SIZE;
    }

    esp_err_t err = uart_enable_pattern_det_baud_intr(handle->port, pattern_char, chr_num, 9, post_idle, pre_idle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable pattern detection: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "UART%d pattern detection enabled (char=0x%02X, count=%d)", handle->port, pattern_char, chr_num);
    return UFLAKE_OK;
}

// Disable pattern detection
uflake_result_t uflake_uart_disable_pattern_detect(uflake_uart_handle_t *handle)
{
    if (handle == NULL || !handle->is_initialized)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    esp_err_t err = uart_disable_pattern_det_intr(handle->port);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disable pattern detection: %s", esp_err_to_name(err));
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "UART%d pattern detection disabled", handle->port);
    return UFLAKE_OK;
}

// Get pattern-detected data
uflake_result_t uflake_uart_get_pattern_data(uflake_uart_handle_t *handle, uint8_t *buffer, size_t max_len, size_t *data_len)
{
    if (handle == NULL || !handle->is_initialized || buffer == NULL)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    int pos = uart_pattern_pop_pos(handle->port);
    if (pos < 0)
    {
        if (data_len != NULL)
        {
            *data_len = 0;
        }
        return UFLAKE_ERROR_NOT_FOUND;
    }

    size_t read_len = (pos < (int)max_len) ? pos : max_len;
    int len = uart_read_bytes(handle->port, buffer, read_len, pdMS_TO_TICKS(100));

    if (len < 0)
    {
        if (data_len != NULL)
        {
            *data_len = 0;
        }
        return UFLAKE_ERROR;
    }

    if (data_len != NULL)
    {
        *data_len = len;
    }

    return UFLAKE_OK;
}

// UART Event Task - handles all UART events
static void uart_event_task(void *pvParameters)
{
    uflake_uart_handle_t *handle = (uflake_uart_handle_t *)pvParameters;
    uart_event_t event;
    uint8_t *rx_buffer = (uint8_t *)uflake_malloc(UART_RX_BUF_SIZE, UFLAKE_MEM_INTERNAL);

    if (rx_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate RX buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UART event task started for UART%d", handle->port);

    while (1)
    {
        // Wait for UART event
        if (xQueueReceive(handle->event_queue, &event, portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                // Data received
                ESP_LOGD(TAG, "UART_DATA: %d bytes", event.size);

                // Call ISR callback first if registered (lower latency)
                if (handle->isr_rx_callback != NULL)
                {
                    size_t bytes_to_read = event.size;
                    if (bytes_to_read > UART_RX_BUF_SIZE)
                    {
                        bytes_to_read = UART_RX_BUF_SIZE;
                    }
                    int len = uart_read_bytes(handle->port, rx_buffer, bytes_to_read, pdMS_TO_TICKS(100));
                    if (len > 0)
                    {
                        handle->isr_rx_callback(rx_buffer, len);
                    }
                }
                // Then call regular callback if registered
                else if (handle->rx_callback != NULL)
                {
                    size_t bytes_to_read = event.size;
                    if (bytes_to_read > UART_RX_BUF_SIZE)
                    {
                        bytes_to_read = UART_RX_BUF_SIZE;
                    }
                    int len = uart_read_bytes(handle->port, rx_buffer, bytes_to_read, pdMS_TO_TICKS(100));
                    if (len > 0)
                    {
                        handle->rx_callback(rx_buffer, len);
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART FIFO overflow");
                uart_flush_input(handle->port);
                xQueueReset(handle->event_queue);
                if (handle->error_callback != NULL)
                {
                    handle->error_callback(UART_FIFO_OVF);
                }
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART ring buffer full");
                uart_flush_input(handle->port);
                xQueueReset(handle->event_queue);
                if (handle->error_callback != NULL)
                {
                    handle->error_callback(UART_BUFFER_FULL);
                }
                break;

            case UART_BREAK:
                ESP_LOGW(TAG, "UART break detected");
                if (handle->error_callback != NULL)
                {
                    handle->error_callback(UART_BREAK);
                }
                break;

            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "UART parity error");
                if (handle->error_callback != NULL)
                {
                    handle->error_callback(UART_PARITY_ERR);
                }
                break;

            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "UART frame error");
                if (handle->error_callback != NULL)
                {
                    handle->error_callback(UART_FRAME_ERR);
                }
                break;

            case UART_PATTERN_DET:
                ESP_LOGD(TAG, "UART pattern detected");
                if (handle->pattern_callback != NULL)
                {
                    int pos = uart_pattern_pop_pos(handle->port);
                    if (pos >= 0)
                    {
                        // Read data up to pattern
                        if (handle->pattern_buffer != NULL && pos <= (int)handle->pattern_buffer_size)
                        {
                            int len = uart_read_bytes(handle->port, handle->pattern_buffer, pos, pdMS_TO_TICKS(100));
                            if (len > 0)
                            {
                                // Null-terminate if it's likely a string
                                if (len < (int)handle->pattern_buffer_size)
                                {
                                    handle->pattern_buffer[len] = '\0';
                                }
                                handle->pattern_callback(len);
                            }
                        }
                        else
                        {
                            handle->pattern_callback(pos);
                        }
                    }
                }
                else
                {
                    // Clear pattern queue if no callback
                    uart_pattern_pop_pos(handle->port);
                }
                break;

            default:
                ESP_LOGW(TAG, "Unknown UART event type: %d", event.type);
                break;
            }
        }
    }

    ESP_LOGW(TAG, "UART event task exiting for UART%d", handle->port);
    uflake_free(rx_buffer);
    vTaskDelete(NULL);
}