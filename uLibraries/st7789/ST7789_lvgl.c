#include "ST7789_lvgl.h"
#include "ST7789_regs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ST7789_LVGL";

static void st7789_spi_pre_cb(spi_transaction_t *t)
{
    st7789_lvgl_t *dev = (st7789_lvgl_t *)t->user;
    gpio_set_level(dev->dc_pin, dev->dc_level);
}

static esp_err_t send_cmd(st7789_lvgl_t *dev, uint8_t cmd)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = dev;
    dev->dc_level = 0;
    return spi_device_polling_transmit(dev->spi, &t);
}

static esp_err_t send_data(st7789_lvgl_t *dev, const uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = dev;
    dev->dc_level = 1;
    return spi_device_polling_transmit(dev->spi, &t);
}

esp_err_t st7789_lvgl_init(st7789_lvgl_t *dev)
{
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dev->reset_pin) | (1ULL << dev->dc_pin),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    
    if (dev->bl_pin != GPIO_NUM_NC) {
        gpio_reset_pin(dev->bl_pin);
        gpio_set_direction(dev->bl_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(dev->bl_pin, 1);
    }
    
    // SPI device config
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = dev->frequency,
        .mode = 0,
        .spics_io_num = dev->cs_pin,
        .queue_size = 7,
        .pre_cb = st7789_spi_pre_cb,
        .flags = SPI_DEVICE_NO_DUMMY,
    };
    
    ESP_ERROR_CHECK(spi_bus_add_device(dev->host, &devcfg, &dev->spi));
    
    // Reset
    gpio_set_level(dev->reset_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(dev->reset_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Init sequence
    const uint8_t caset[4] = {0, 0, (dev->width-1)>>8, (dev->width-1)&0xFF};
    const uint8_t raset[4] = {0, 0, (dev->height-1)>>8, (dev->height-1)&0xFF};
    
    send_cmd(dev, ST7789_CMD_SWRESET); vTaskDelay(pdMS_TO_TICKS(150));
    send_cmd(dev, ST7789_CMD_SLPOUT); vTaskDelay(pdMS_TO_TICKS(120));
    send_cmd(dev, ST7789_CMD_MADCTL); send_data(dev, (uint8_t[]){0x00}, 1);
    send_cmd(dev, ST7789_CMD_COLMOD); send_data(dev, (uint8_t[]){0x55}, 1);
    send_cmd(dev, ST7789_CMD_CASET); send_data(dev, caset, 4);
    send_cmd(dev, ST7789_CMD_RASET); send_data(dev, raset, 4);
    send_cmd(dev, ST7789_CMD_INVON); vTaskDelay(pdMS_TO_TICKS(10));
    send_cmd(dev, ST7789_CMD_NORON); vTaskDelay(pdMS_TO_TICKS(10));
    send_cmd(dev, ST7789_CMD_DISPON); vTaskDelay(pdMS_TO_TICKS(120));
    
    // ESP_LOGI(TAG, \"Display %dx%d @ %dMHz ready\", dev->width, dev->height, dev->frequency/1000000);
    return ESP_OK;
}

esp_err_t st7789_lvgl_deinit(st7789_lvgl_t *dev)
{
    if (dev->spi) {
        spi_bus_remove_device(dev->spi);
        dev->spi = NULL;
    }
    return ESP_OK;
}

esp_err_t st7789_lvgl_set_window(st7789_lvgl_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t caset[4] = {x0>>8, x0&0xFF, x1>>8, x1&0xFF};
    uint8_t raset[4] = {y0>>8, y0&0xFF, y1>>8, y1&0xFF};
    
    send_cmd(dev, ST7789_CMD_CASET);
    send_data(dev, caset, 4);
    send_cmd(dev, ST7789_CMD_RASET);
    send_data(dev, raset, 4);
    return ESP_OK;
}

esp_err_t st7789_lvgl_write_pixels(st7789_lvgl_t *dev, const uint16_t *colors, size_t length)
{
    send_cmd(dev, ST7789_CMD_RAMWR);
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = length * 16;
    t.tx_buffer = colors;
    t.user = dev;
    dev->dc_level = 1;
    
    return spi_device_transmit(dev->spi, &t);
}

esp_err_t st7789_lvgl_backlight(st7789_lvgl_t *dev, bool on)
{
    if (dev->bl_pin == GPIO_NUM_NC) return ESP_ERR_NOT_SUPPORTED;
    gpio_set_level(dev->bl_pin, on ? 1 : 0);
    return ESP_OK;
}
