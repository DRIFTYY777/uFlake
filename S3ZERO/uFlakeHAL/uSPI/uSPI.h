#ifndef USPI_H_
#define USPI_H_

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

// SPI frequency presets
#define USPI_FREQ_1MHZ 1000000
#define USPI_FREQ_5MHZ 5000000
#define USPI_FREQ_10MHZ 10000000
#define USPI_FREQ_20MHZ 20000000
#define USPI_FREQ_40MHZ 40000000
#define USPI_FREQ_80MHZ 80000000
#define USPI_DEFAULT_FREQ USPI_FREQ_10MHZ

// SPI bus definitions
#define USPI_HOST_SPI2 SPI2_HOST
#define USPI_HOST_SPI3 SPI3_HOST

// Max devices per bus
#define USPI_MAX_DEVICES_PER_BUS 6
#define USPI_MAX_TRANSFER_SIZE 8192

    // SPI device types
    typedef enum
    {
        USPI_DEVICE_GENERIC = 0,
        USPI_DEVICE_DISPLAY,
        USPI_DEVICE_FLASH,
        USPI_DEVICE_SD_CARD,
        USPI_DEVICE_SENSOR,
        USPI_DEVICE_CUSTOM
    } uspi_device_type_t;

    // SPI mode definitions
    typedef enum
    {
        USPI_MODE_0 = 0, // CPOL=0, CPHA=0
        USPI_MODE_1 = 1, // CPOL=0, CPHA=1
        USPI_MODE_2 = 2, // CPOL=1, CPHA=0
        USPI_MODE_3 = 3  // CPOL=1, CPHA=1
    } uspi_mode_t;

    // SPI device configuration
    typedef struct
    {
        uint8_t cs_pin;
        uint32_t clock_speed_hz;
        uspi_mode_t mode;
        uint8_t queue_size;
        bool cs_ena_pretrans;
        bool cs_ena_posttrans;
        uint8_t address_bits;
        uint8_t command_bits;
        uint8_t dummy_bits;
        uspi_device_type_t device_type;
        const char *device_name;
    } uspi_device_config_t;

    // Bus initialization
    uflake_result_t uspi_bus_init(spi_host_device_t host, gpio_num_t mosi, gpio_num_t miso,
                                  gpio_num_t sclk, int max_transfer_sz);
    esp_err_t uspi_bus_deinit(spi_host_device_t host);

    // Device management
    esp_err_t uspi_device_add(spi_host_device_t host, const uspi_device_config_t *dev_config,
                              spi_device_handle_t *out_handle);
    esp_err_t uspi_device_remove(spi_device_handle_t handle);
    esp_err_t uspi_device_acquire_bus(spi_device_handle_t handle, TickType_t wait);
    void uspi_device_release_bus(spi_device_handle_t handle);

    // Transfer operations
    esp_err_t uspi_transmit(spi_device_handle_t handle, const uint8_t *tx_buffer,
                            size_t length, uint32_t timeout_ms);
    esp_err_t uspi_receive(spi_device_handle_t handle, uint8_t *rx_buffer,
                           size_t length, uint32_t timeout_ms);
    esp_err_t uspi_transfer(spi_device_handle_t handle, const uint8_t *tx_buffer,
                            uint8_t *rx_buffer, size_t length, uint32_t timeout_ms);

    // Command/Address/Data operations (for displays, flash, etc.)
    esp_err_t uspi_write_cmd(spi_device_handle_t handle, uint8_t cmd);
    esp_err_t uspi_write_cmd_data(spi_device_handle_t handle, uint8_t cmd,
                                  const uint8_t *data, size_t len);
    esp_err_t uspi_write_cmd_addr_data(spi_device_handle_t handle, uint8_t cmd,
                                       uint32_t addr, const uint8_t *data, size_t len);

    // DMA operations for large transfers
    esp_err_t uspi_transmit_dma(spi_device_handle_t handle, const uint8_t *tx_buffer,
                                size_t length, uint32_t timeout_ms);
    esp_err_t uspi_transfer_dma(spi_device_handle_t handle, const uint8_t *tx_buffer,
                                uint8_t *rx_buffer, size_t length, uint32_t timeout_ms);

    // Polling transfer (for small amounts of data)
    esp_err_t uspi_polling_transmit(spi_device_handle_t handle, const uint8_t *tx_buffer,
                                    size_t length);
    esp_err_t uspi_polling_transfer(spi_device_handle_t handle, const uint8_t *tx_buffer,
                                    uint8_t *rx_buffer, size_t length);

    // Get device information
    esp_err_t uspi_get_device_info(spi_device_handle_t handle, uspi_device_config_t *info);
    esp_err_t uspi_get_device_count(spi_host_device_t host, uint8_t *count);

#ifdef __cplusplus
}
#endif

#endif // USPI_H_
