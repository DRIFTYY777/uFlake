#if !defined(ST7789_DRIVER_H_)
#define ST7789_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct st7789_driver;
    typedef uint16_t st7789_color_t;

    typedef struct st7789_transaction_data
    {
        struct st7789_driver *driver;
        bool data;
    } st7789_transaction_data_t;

    typedef struct st7789_command
    {
        uint8_t command;
        uint8_t wait_ms;
        uint8_t data_size;
        const uint8_t *data;
    } st7789_command_t;

    typedef enum
    {
        ST7789_COLOR_ORDER_RGB, // Default Red-Green-Blue
        ST7789_COLOR_ORDER_BGR  // Blue-Green-Red
    } st7789_color_order_t;

    typedef struct
    {
        gpio_num_t cs_pin;    // chip-select pin (for device registration)
        gpio_num_t reset_pin; // reset pin (optional, set GPIO_NUM_NC to skip)
        gpio_num_t dc_pin;    // data/command pin
        gpio_num_t bl_pin;    // backlight pin (optional, set GPIO_NUM_NC to skip)

        uint16_t width;  // display width in pixels
        uint16_t height; // display height in pixels

        spi_device_handle_t spi; // filled after init
        spi_host_device_t host;  // SPI host (USPI_HOST_SPI2 / USPI_HOST_SPI3)
        uint32_t frequency;      // SPI frequency (use USPI_FREQ_... macros)

        size_t buffer_size;
        st7789_transaction_data_t data;
        st7789_transaction_data_t command;
        st7789_color_t *buffer;
        st7789_color_t *buffer_primary;
        st7789_color_t *buffer_secondary;
        st7789_color_t *current_buffer;
        spi_transaction_t trans_a;
        spi_transaction_t trans_b;

    } st7789_t;

    // Initialize the device and GPIOs, add SPI device. Returns ESP_OK on success.
    esp_err_t st7789_init(st7789_t *dev);
    esp_err_t st7789_deinit();

    // Basic operations
    esp_err_t st7789_reset();
    esp_err_t st7789_set_rotation(uint8_t madctl);
    esp_err_t st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    esp_err_t st7789_push_colors(const uint16_t *colors, size_t count);
    esp_err_t st7789_clear();

    void ST7789_fill_area(st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height);
    void ST7789_write_pixels(st7789_color_t *pixels, size_t length);
    void ST7789_swap_buffers(st7789_t *dev);
    uint16_t swap_rgb(uint16_t color);

    // Backlight
    esp_err_t st7789_backlight(bool on);

#ifdef __cplusplus
}
#endif

#endif // ST7789_DRIVER_H_