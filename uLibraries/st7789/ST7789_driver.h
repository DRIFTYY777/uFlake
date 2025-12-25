#if !defined(ST7789_DRIVER_H_)
#define ST7789_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        gpio_num_t pin_reset;
        gpio_num_t pin_dc;
        spi_device_handle_t spi;
        uint16_t width;
        uint16_t height;
    } st7789_t;

    bool st7789_init(st7789_t *drv);
    void st7789_fill_area(st7789_t *drv, uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void st7789_draw_pixel(st7789_t *drv, uint16_t x, uint16_t y, uint16_t color);
   
  

#ifdef __cplusplus
}
#endif


#endif // ST7789_DRIVER_H_