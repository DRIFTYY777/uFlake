#if !defined(UUART_H_)
#define UUART_H_

#include "driver/uart.h"
#include "driver/gpio.h"
#include "kernel.h"

// UART Configuration defaults
#define UART_0 UART_NUM_0
#define UART_1 UART_NUM_1
#define UART_2 UART_NUM_2

#define UART_TX_PIN_DEFAULT (GPIO_NUM_43)
#define UART_RX_PIN_DEFAULT (GPIO_NUM_44)
#define UART_RTS_PIN_DEFAULT (UART_PIN_NO_CHANGE)
#define UART_CTS_PIN_DEFAULT (UART_PIN_NO_CHANGE)

#define UART_BAUD_RATE_DEFAULT (115200)
#define UART_RX_BUF_SIZE (2048)
#define UART_TX_BUF_SIZE (1024)
#define UART_QUEUE_SIZE (20)
#define UART_EVENT_TASK_STACK_SIZE (4096)
#define UART_EVENT_TASK_PRIORITY (10)

#ifdef __cplusplus
extern "C"
{
#endif

    // UART Configuration structure
    typedef struct
    {
        uart_port_t port;
        int tx_pin;
        int rx_pin;
        int rts_pin;
        int cts_pin;
        int baud_rate;
        uart_word_length_t data_bits;
        uart_parity_t parity;
        uart_stop_bits_t stop_bits;
        uart_hw_flowcontrol_t flow_ctrl;
        int rx_flow_ctrl_thresh;
        int rx_buffer_size;
        int tx_buffer_size;
        int event_queue_size;
        int rx_threshold;           // RX FIFO threshold for interrupt trigger
        bool use_isr_in_iram;       // Place ISR in IRAM for faster execution
        bool enable_pattern_detect; // Enable pattern detection interrupt
        uint8_t pattern_char;       // Pattern character (e.g., '\n' for line-based)
        uint8_t pattern_count;      // Number of pattern chars to trigger (1-127)
    } uflake_uart_config_t;

    // UART Handle structure
    typedef struct
    {
        uart_port_t port;
        QueueHandle_t event_queue;
        TaskHandle_t event_task_handle;
        bool is_initialized;
        void (*rx_callback)(uint8_t *data, size_t len);       // Called from task context
        void (*error_callback)(uart_event_type_t error_type); // Called from task context
        void (*isr_rx_callback)(uint8_t *data, size_t len);   // Called from ISR context (must be IRAM_ATTR)
        void (*pattern_callback)(int pos);                    // Called when pattern detected
        uint8_t *pattern_buffer;                              // Buffer for pattern-detected data
        size_t pattern_buffer_size;
    } uflake_uart_handle_t;

    // Initialization and deinitialization
    uflake_result_t uflake_uart_init(uflake_uart_handle_t *handle, const uflake_uart_config_t *config);
    uflake_result_t uflake_uart_deinit(uflake_uart_handle_t *handle);

    // Data transmission
    uflake_result_t uflake_uart_write(uflake_uart_handle_t *handle, const uint8_t *data, size_t len);
    uflake_result_t uflake_uart_write_string(uflake_uart_handle_t *handle, const char *str);
    uflake_result_t uflake_uart_write_bytes(uflake_uart_handle_t *handle, const uint8_t *data, size_t len, uint32_t timeout_ms);

    // Data reception
    uflake_result_t uflake_uart_read(uflake_uart_handle_t *handle, uint8_t *buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms);
    uflake_result_t uflake_uart_read_blocking(uflake_uart_handle_t *handle, uint8_t *buffer, size_t len, size_t *bytes_read);
    int uflake_uart_available(uflake_uart_handle_t *handle);

    // Configuration functions
    uflake_result_t uflake_uart_set_baud_rate(uflake_uart_handle_t *handle, int baud_rate);
    uflake_result_t uflake_uart_set_pins(uflake_uart_handle_t *handle, int tx, int rx, int rts, int cts);
    uflake_result_t uflake_uart_set_mode(uflake_uart_handle_t *handle, uart_mode_t mode);

    // Callback registration
    uflake_result_t uflake_uart_register_rx_callback(uflake_uart_handle_t *handle, void (*callback)(uint8_t *data, size_t len));
    uflake_result_t uflake_uart_register_error_callback(uflake_uart_handle_t *handle, void (*callback)(uart_event_type_t error_type));
    uflake_result_t uflake_uart_register_isr_callback(uflake_uart_handle_t *handle, void (*callback)(uint8_t *data, size_t len)); // ISR context callback
    uflake_result_t uflake_uart_register_pattern_callback(uflake_uart_handle_t *handle, void (*callback)(int pos));

    // Interrupt configuration
    uflake_result_t uflake_uart_set_rx_threshold(uflake_uart_handle_t *handle, int threshold);
    uflake_result_t uflake_uart_enable_pattern_detect(uflake_uart_handle_t *handle, char pattern_char, uint8_t chr_num, int post_idle, int pre_idle);
    uflake_result_t uflake_uart_disable_pattern_detect(uflake_uart_handle_t *handle);

    // Buffer control
    uflake_result_t uflake_uart_flush(uflake_uart_handle_t *handle);
    uflake_result_t uflake_uart_flush_input(uflake_uart_handle_t *handle);
    uflake_result_t uflake_uart_get_pattern_data(uflake_uart_handle_t *handle, uint8_t *buffer, size_t max_len, size_t *data_len);

    // Helper function to get default configuration
    void uflake_uart_get_default_config(uflake_uart_config_t *config, uart_port_t port);

#ifdef __cplusplus
}
#endif

#endif // UUART_H_