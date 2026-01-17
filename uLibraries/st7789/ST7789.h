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

#ifndef ST7789_INVERT_COLORS
#define ST7789_INVERT_COLORS 0
#endif

/* ST7789 commands */
#define ST7789_NOP 0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID 0x04
#define ST7789_RDDST 0x09

#define ST7789_RDDPM 0x0A      // Read display power mode
#define ST7789_RDD_MADCTL 0x0B // Read display MADCTL
#define ST7789_RDD_COLMOD 0x0C // Read display pixel format
#define ST7789_RDDIM 0x0D      // Read display image mode
#define ST7789_RDDSM 0x0E      // Read display signal mode
#define ST7789_RDDSR 0x0F      // Read display self-diagnostic result (ST7789V)

#define ST7789_SLPIN 0x10
#define ST7789_SLPOUT 0x11
#define ST7789_PTLON 0x12
#define ST7789_NORON 0x13

#define ST7789_INVOFF 0x20
#define ST7789_INVON 0x21
#define ST7789_GAMSET 0x26 // Gamma set
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON 0x29
#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_RGBSET 0x2D // Color setting for 4096, 64K and 262K colors
#define ST7789_RAMRD 0x2E

#define ST7789_PTLAR 0x30
#define ST7789_VSCRDEF 0x33 // Vertical scrolling definition (ST7789V)
#define ST7789_TEOFF 0x34   // Tearing effect line off
#define ST7789_TEON 0x35    // Tearing effect line on
#define ST7789_MADCTL 0x36  // Memory data access control
#define ST7789_IDMOFF 0x38  // Idle mode off
#define ST7789_IDMON 0x39   // Idle mode on
#define ST7789_RAMWRC 0x3C  // Memory write continue (ST7789V)
#define ST7789_RAMRDC 0x3E  // Memory read continue (ST7789V)
#define ST7789_COLMOD 0x3A

#define ST7789_RAMCTRL 0xB0   // RAM control
#define ST7789_RGBCTRL 0xB1   // RGB control
#define ST7789_PORCTRL 0xB2   // Porch control
#define ST7789_FRCTRL1 0xB3   // Frame rate control
#define ST7789_PARCTRL 0xB5   // Partial mode control
#define ST7789_GCTRL 0xB7     // Gate control
#define ST7789_GTADJ 0xB8     // Gate on timing adjustment
#define ST7789_DGMEN 0xBA     // Digital gamma enable
#define ST7789_VCOMS 0xBB     // VCOMS setting
#define ST7789_LCMCTRL 0xC0   // LCM control
#define ST7789_IDSET 0xC1     // ID setting
#define ST7789_VDVVRHEN 0xC2  // VDV and VRH command enable
#define ST7789_VRHS 0xC3      // VRH set
#define ST7789_VDVSET 0xC4    // VDV setting
#define ST7789_VCMOFSET 0xC5  // VCOMS offset set
#define ST7789_FRCTR2 0xC6    // FR Control 2
#define ST7789_CABCCTRL 0xC7  // CABC control
#define ST7789_REGSEL1 0xC8   // Register value section 1
#define ST7789_REGSEL2 0xCA   // Register value section 2
#define ST7789_PWMFRSEL 0xCC  // PWM frequency selection
#define ST7789_PWCTRL1 0xD0   // Power control 1
#define ST7789_VAPVANEN 0xD2  // Enable VAP/VAN signal output
#define ST7789_CMD2EN 0xDF    // Command 2 enable
#define ST7789_PVGAMCTRL 0xE0 // Positive voltage gamma control
#define ST7789_NVGAMCTRL 0xE1 // Negative voltage gamma control
#define ST7789_DGMLUTR 0xE2   // Digital gamma look-up table for red
#define ST7789_DGMLUTB 0xE3   // Digital gamma look-up table for blue
#define ST7789_GATECTRL 0xE4  // Gate control
#define ST7789_SPI2EN 0xE7    // SPI2 enable
#define ST7789_PWCTRL2 0xE8   // Power control 2
#define ST7789_EQCTRL 0xE9    // Equalize time control
#define ST7789_PROMCTRL 0xEC  // Program control
#define ST7789_PROMEN 0xFA    // Program mode enable
#define ST7789_NVMSET 0xFC    // NVM setting
#define ST7789_PROMACT 0xFE   // Program action

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
    void ST7789_reset(st7789_driver_t *driver);
    void ST7789_fill_area(st7789_driver_t *driver, st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height);
    void ST7789_write_pixels(st7789_driver_t *driver, st7789_color_t *pixels, size_t length);
    void ST7789_write_lines(st7789_driver_t *driver, int ypos, int xpos, int width, uint16_t *linedata, int lineCount);
    void ST7789_swap_buffers(st7789_driver_t *driver);
    void ST7789_set_window(st7789_driver_t *driver, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y);
    void ST7789_queue_empty(st7789_driver_t *driver);
    void ST7789_set_endian(st7789_driver_t *driver);
    void ST7789_invert_display(st7789_driver_t *driver, bool invert);
    void ST7789_rgb_order(st7789_driver_t *driver, uint8_t rgb_order);
    void drawPixel(st7789_driver_t *driver, int16_t x, int16_t y, uint16_t color);
    void drawCircle(st7789_driver_t *driver, int16_t x0, int16_t y0, int16_t r, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif // ST7789_LVGL_H
