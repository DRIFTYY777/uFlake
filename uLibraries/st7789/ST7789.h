#ifndef ST7789_LVGL_H
#define ST7789_LVGL_H

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "lvgl.h"
#include "uSPI.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ST7789_SPI_QUEUE_SIZE 7
#define ST7789_CMD_CASET 0x2A
#define ST7789_CMD_RASET 0x2B
#define ST7789_CMD_RAMWR 0x2C
#define ST7789_CMD_SLPIN 0x10
#define ST7789_CMD_SWRESET 0x01
#define ST7789_CMD_SLPOUT 0x11
#define ST7789_CMD_MADCTL 0x36
#define ST7789_CMD_COLMOD 0x3A
#define ST7789_CMD_INVON 0x21
#define ST7789_CMD_INVOFF 0x20
#define ST7789_CMD_PORCTRL 0xB2
#define ST7789_CMD_GCTRL 0xB7
#define ST7789_CMD_VCOMS 0xBB
#define ST7789_CMD_VDVVRHEN 0xC2
#define ST7789_CMD_VRHSET 0xC3
#define ST7789_CMD_VDVSET 0xC4
#define ST7789_CMD_PWCTRL1 0xD0
#define ST7789_CMD_FRCTR2 0xC6
#define ST7789_CMD_GAMSET 0x26
#define ST7789_CMD_PVGAMCTRL 0xE0
#define ST7789_CMD_NVGAMCTRL 0xE1
#define ST7789_CMD_RAMCTRL 0xB0
#define ST7789_CMD_DISPON 0x29
#define ST7789_CMDLIST_END 0x00

    typedef struct st7789_driver st7789_driver_t;

    typedef struct st7789_transaction_data
    {
        st7789_driver_t *driver;
        bool data;
    } st7789_transaction_data_t;
    typedef uint16_t st7789_color_t;

    typedef struct st7789_command
    {
        uint8_t command;
        uint8_t wait_ms;
        uint8_t data_size;
        const uint8_t *data;
    } st7789_command_t;

    // DMA completion callback type
    typedef void (*st7789_flush_cb_t)(void *user_data);

    struct st7789_driver
    {
        // Pin configuration
        gpio_num_t pin_cs;
        gpio_num_t pin_reset;
        gpio_num_t pin_dc;

        // Display parameters
        uint16_t display_width;
        uint16_t display_height;
        uint8_t orientation;

        // SPI configuration
        spi_device_handle_t spi;
        spi_host_device_t spi_host;
        uint32_t spi_speed;

        // Buffer management
        st7789_color_t *buffer;
        st7789_color_t *buffer_primary;
        st7789_color_t *buffer_secondary;
        st7789_color_t *current_buffer;
        size_t buffer_size;
        uint8_t queue_fill;

        // SPI transaction data
        st7789_transaction_data_t data;
        st7789_transaction_data_t command;
        spi_transaction_t trans_a;
        spi_transaction_t trans_b;
    };

    bool ST7789_init(st7789_driver_t *driver);
    bool ST7789_deinit(st7789_driver_t *driver);
    void ST7789_reset(st7789_driver_t *driver);
    void ST7789_fill_area(st7789_driver_t *driver, st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height);
    void ST7789_write_pixels(st7789_driver_t *driver, st7789_color_t *pixels, size_t length);
    void ST7789_write_lines(st7789_driver_t *driver, int ypos, int xpos, int width, uint16_t *linedata, int lineCount);
    void ST7789_swap_buffers(st7789_driver_t *driver);
    void ST7789_set_window(st7789_driver_t *driver, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y);
    void ST7789_queue_empty(st7789_driver_t *driver);
    void ST7789_set_endian(st7789_driver_t *driver);
    void ST7789_invert_display(st7789_driver_t *driver, bool invert);


#ifdef __cplusplus
}
#endif

#endif // ST7789_LVGL_H
