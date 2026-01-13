#ifndef ST7789_LVGL_H
#define ST7789_LVGL_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t cs_pin;
    gpio_num_t reset_pin;
    gpio_num_t dc_pin;
    gpio_num_t bl_pin;
    
    uint16_t width;
    uint16_t height;
    
    spi_device_handle_t spi;
    spi_host_device_t host;
    uint32_t frequency;
    bool dc_level;
} st7789_lvgl_t;

esp_err_t st7789_lvgl_init(st7789_lvgl_t *dev);
esp_err_t st7789_lvgl_deinit(st7789_lvgl_t *dev);
esp_err_t st7789_lvgl_set_window(st7789_lvgl_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
esp_err_t st7789_lvgl_write_pixels(st7789_lvgl_t *dev, const uint16_t *colors, size_t length);
esp_err_t st7789_lvgl_backlight(st7789_lvgl_t *dev, bool on);

#ifdef __cplusplus
}
#endif

#endif // ST7789_LVGL_H
