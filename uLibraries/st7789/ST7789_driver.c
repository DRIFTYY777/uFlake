#include "ST7789_driver.h"
#include "uSPI.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ST7789_regs.h"

#include "kernel.h"

static const char *TAG = "ST7789";

static void st7789_write_cmd(st7789_t *drv, uint8_t cmd)
{
    gpio_set_level(drv->pin_dc, 0); // Command mode
    esp_err_t ret = uspi_transmit(drv->spi, &cmd, 1, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write command: %s", esp_err_to_name(ret));
    }
}

static void st7789_write_data(st7789_t *drv, const uint8_t *data, size_t len)
{
    gpio_set_level(drv->pin_dc, 1); // Data mode
    esp_err_t ret = uspi_transmit(drv->spi, data, len, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write data: %s", esp_err_to_name(ret));
    }
}

static void st7789_write_cmd_data(st7789_t *drv, uint8_t cmd, const uint8_t *data, size_t len)
{
    st7789_write_cmd(drv, cmd);
    if (data && len > 0)
    {
        st7789_write_data(drv, data, len);
    }
}

static void st7789_reset(st7789_t *drv)
{
    ESP_LOGI(TAG, "Resetting display");
    gpio_set_level(drv->pin_reset, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(drv->pin_reset, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

bool st7789_init(st7789_t *drv)
{
    ESP_LOGI(TAG, "Initializing ST7789 display");

    // Configure reset and DC pins
    gpio_set_direction(drv->pin_reset, GPIO_MODE_OUTPUT);
    gpio_set_direction(drv->pin_dc, GPIO_MODE_OUTPUT);

    // Reset the display
    st7789_reset(drv);

    // Software reset
    st7789_write_cmd(drv, ST7789_CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Sleep out
    st7789_write_cmd(drv, ST7789_CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Set color mode to 16-bit (RGB565)
    uint8_t colmod = 0x55;
    st7789_write_cmd_data(drv, ST7789_CMD_COLMOD, &colmod, 1);

    // Set memory access control (rotation)
    uint8_t madctl = 0x00; // Default orientation
    st7789_write_cmd_data(drv, ST7789_CMD_MADCTL, &madctl, 1);

    // Display on
    st7789_write_cmd(drv, ST7789_CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_LOGI(TAG, "ST7789 initialization complete");
    return true;
}

static void st7789_set_window(st7789_t *drv, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    // Column address set
    uint8_t caset[4] = {
        (x1 >> 8) & 0xFF, x1 & 0xFF,
        (x2 >> 8) & 0xFF, x2 & 0xFF};
    st7789_write_cmd_data(drv, ST7789_CMD_CASET, caset, 4);

    // Row address set
    uint8_t raset[4] = {
        (y1 >> 8) & 0xFF, y1 & 0xFF,
        (y2 >> 8) & 0xFF, y2 & 0xFF};
    st7789_write_cmd_data(drv, ST7789_CMD_RASET, raset, 4);

    // Memory write
    st7789_write_cmd(drv, ST7789_CMD_RAMWR);
}

void st7789_fill_area(st7789_t *drv, uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (x >= drv->width || y >= drv->height)
        return;
    if (x + w > drv->width)
        w = drv->width - x;
    if (y + h > drv->height)
        h = drv->height - y;

    st7789_set_window(drv, x, y, x + w - 1, y + h - 1);

    // Prepare color data (convert to big-endian for ST7789)
    uint8_t color_data[2] = {
        (color >> 8) & 0xFF,
        color & 0xFF};

    // Send pixel data
    gpio_set_level(drv->pin_dc, 1); // Data mode

    // For large areas, we could optimize this by sending chunks
    uint32_t total_pixels = w * h;
    for (uint32_t i = 0; i < total_pixels; i++)
    {
        uspi_transmit(drv->spi, color_data, 2, 100);
    }
}

void st7789_draw_pixel(st7789_t *drv, uint16_t x, uint16_t y, uint16_t color)
{
    st7789_fill_area(drv, color, x, y, 1, 1);
}
