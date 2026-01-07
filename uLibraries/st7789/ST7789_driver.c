#include "ST7789_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ST7789_regs.h"

#include "uSPI.h"

static const char *TAG = "ST7789";
static st7789_t *global_dev = NULL; // global pointer to the current device

static void ST7789_send_cmd(const st7789_command_t *command);
static void ST7789_config();
static void ST7789_pre_cb(spi_transaction_t *transaction);
static void ST7789_multi_cmd(const st7789_command_t *sequence);

esp_err_t st7789_init(st7789_t *dev)
{
    // Store the global device pointer
    global_dev = dev;

    // Allocate the buffer memory
    dev->buffer = (st7789_color_t *)uflake_malloc(dev->buffer_size * 2 * sizeof(st7789_color_t), UFLAKE_MEM_INTERNAL); //
    if (dev->buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for display buffer");
        return ESP_ERR_NO_MEM;
    }

    // Set up the display buffers
    dev->buffer_primary = dev->buffer;
    dev->buffer_secondary = dev->buffer + dev->buffer_size;
    dev->current_buffer = dev->buffer_primary;
    // dev->queue_fill = 0;

    dev->data.driver = (struct st7789_driver *)dev;
    dev->data.data = true;
    dev->command.driver = (struct st7789_driver *)dev;
    dev->command.data = false;

    // Set the RESET and DC PIN
    gpio_reset_pin(dev->reset_pin);
    gpio_reset_pin(dev->dc_pin);
    gpio_set_direction(dev->reset_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(dev->dc_pin, GPIO_MODE_OUTPUT);

    // initialize the display brightness pin if configured
    if (dev->bl_pin != GPIO_NUM_NC)
    {
        gpio_reset_pin(dev->bl_pin);
        gpio_set_direction(dev->bl_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(dev->bl_pin, 1); // Turn on backlight
    }

    // Register the SPI device
    uspi_device_config_t spi_config = {
        .cs_pin = dev->cs_pin,
        .clock_speed_hz = dev->frequency,
        .mode = USPI_MODE_0,
        .queue_size = 7,
        .cs_ena_pretrans = false,
        .cs_ena_posttrans = false,
        .address_bits = 0,
        .command_bits = 0,
        .dummy_bits = 0,
        .device_type = USPI_DEVICE_DISPLAY,
        .device_name = "ST7789",
    };

    esp_err_t ret = uspi_device_add(dev->host, &spi_config, &dev->spi);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add SPI device");
        uflake_free(dev->buffer);
        return ret;
    }

    st7789_reset();
    ST7789_config();
    return ESP_OK;
}

esp_err_t st7789_deinit()
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Free the allocated buffer memory
    uflake_free(global_dev->buffer);
    global_dev->buffer = NULL;

    // Remove the SPI device
    uspi_device_remove(global_dev->spi);

    global_dev->spi = NULL;

    global_dev = NULL; // Clear the global device pointer

    ST7789_config();

    return ESP_OK;
}

esp_err_t st7789_reset()
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Toggle the reset pin
    gpio_set_level(global_dev->reset_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(global_dev->reset_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

esp_err_t st7789_set_rotation(uint8_t madctl)
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    const st7789_command_t rotation_cmd = {
        .command = ST7789_CMD_MADCTL,
        .wait_ms = 0,
        .data_size = 1,
        .data = &madctl,
    };

    ST7789_send_cmd(&rotation_cmd);
    return ESP_OK;
}

esp_err_t st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t caset[4] = {
        (x0 >> 8) & 0xFF,
        x0 & 0xFF,
        (x1 >> 8) & 0xFF,
        x1 & 0xFF};
    uint8_t raset[4] = {
        (y0 >> 8) & 0xFF,
        y0 & 0xFF,
        (y1 >> 8) & 0xFF,
        y1 & 0xFF};

    const st7789_command_t caset_cmd = {
        .command = ST7789_CMD_CASET,
        .wait_ms = 0,
        .data_size = 4,
        .data = caset,
    };
    const st7789_command_t raset_cmd = {
        .command = ST7789_CMD_RASET,
        .wait_ms = 0,
        .data_size = 4,
        .data = raset,
    };

    ST7789_send_cmd(&caset_cmd);
    ST7789_send_cmd(&raset_cmd);

    return ESP_OK;
}

esp_err_t st7789_push_colors(const uint16_t *colors, size_t count)
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    const st7789_command_t ramwr_cmd = {
        .command = ST7789_CMD_RAMWR,
        .wait_ms = 0,
        .data_size = count * 2,
        .data = (const uint8_t *)colors,
    };

    ST7789_send_cmd(&ramwr_cmd);
    return ESP_OK;
}

esp_err_t st7789_clear()
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ST7789_fill_area(0x0000, 0, 0, global_dev->width, global_dev->height);
    return ESP_OK;
}

void ST7789_fill_area(st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height)
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return;
    }

    size_t total_pixels = width * height;
    size_t buffer_pixels = global_dev->buffer_size;

    // Set the drawing window
    st7789_set_window(start_x, start_y, start_x + width - 1, start_y + height - 1);

    // Fill the buffer with the specified color
    for (size_t i = 0; i < buffer_pixels; i++)
    {
        global_dev->current_buffer[i] = color;
    }

    // Push the color data in chunks
    size_t pixels_sent = 0;
    while (pixels_sent < total_pixels)
    {
        size_t pixels_to_send = (total_pixels - pixels_sent) > buffer_pixels ? buffer_pixels : (total_pixels - pixels_sent);
        st7789_push_colors(global_dev->current_buffer, pixels_to_send);
        pixels_sent += pixels_to_send;
    }
}

void ST7789_write_pixels(st7789_color_t *pixels, size_t length)
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return;
    }

    st7789_push_colors(pixels, length);
}

void ST7789_swap_buffers(st7789_t *dev)
{
    if (dev->current_buffer == dev->buffer_primary)
    {
        dev->current_buffer = dev->buffer_secondary;
    }
    else
    {
        dev->current_buffer = dev->buffer_primary;
    }
}

uint16_t swap_rgb(uint16_t color)
{
    return (color >> 11) | (color & 0x07E0) | (color << 11);
}

esp_err_t st7789_backlight(bool on)
{
    if (global_dev == NULL)
    {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (global_dev->bl_pin == GPIO_NUM_NC)
    {
        ESP_LOGW(TAG, "Backlight pin not configured");
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(global_dev->bl_pin, on ? 1 : 0);
    return ESP_OK;
}

void ST7789_send_cmd(const st7789_command_t *command)
{
    gpio_set_level(global_dev->dc_pin, 0); // Command mode
    esp_err_t ret = uspi_transmit(global_dev->spi, &command->command, 1, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send command 0x%02X", command->command);
        return;
    }
    if (command->data_size > 0 && command->data != NULL)
    {
        gpio_set_level(global_dev->dc_pin, 1); // Data mode
        ret = uspi_transmit(global_dev->spi, command->data, command->data_size, 100);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send data for command 0x%02X", command->command);
            return;
        }
    }
}

void ST7789_config()
{
    const uint8_t caset[4] = {
        0x00,
        0x00,
        (global_dev->width - 1) >> 8,
        (global_dev->width - 1) & 0xff};
    const uint8_t raset[4] = {
        0x00,
        0x00,
        (global_dev->height - 1) >> 8,
        (global_dev->height - 1) & 0xff};
    const st7789_command_t init_sequence[] = {
        // Sleep
        {ST7789_CMD_SLPIN, 10, 0, NULL},                    // Sleep
        {ST7789_CMD_SWRESET, 200, 0, NULL},                 // Reset
        {ST7789_CMD_SLPOUT, 120, 0, NULL},                  // Sleep out
        {ST7789_CMD_MADCTL, 0, 1, (const uint8_t *)"\x00"}, // Page / column address order
        {ST7789_CMD_COLMOD, 0, 1, (const uint8_t *)"\x55"}, // 16 bit RGB
        {ST7789_CMD_INVOFF, 0, 0, NULL},                    // Inversion OFF (Corrected)
        {ST7789_CMD_CASET, 0, 4, (const uint8_t *)&caset},  // Set width
        {ST7789_CMD_RASET, 0, 4, (const uint8_t *)&raset},  // Set height

        // Porch setting
        {ST7789_CMD_PORCTRL, 0, 5, (const uint8_t *)"\x0c\x0c\x00\x33\x33"},
        // Set VGH to 12.54V and VGL to -9.6V
        {ST7789_CMD_GCTRL, 0, 1, (const uint8_t *)"\x14"},
        // Set VCOM to 1.475V
        {ST7789_CMD_VCOMS, 0, 1, (const uint8_t *)"\x37"},
        // Enable VDV/VRH control
        {ST7789_CMD_VDVVRHEN, 0, 2, (const uint8_t *)"\x01\xff"},
        // VAP(GVDD) = 4.45+(vcom+vcom offset+vdv)
        {ST7789_CMD_VRHSET, 0, 1, (const uint8_t *)"\x12"},
        // VDV = 0V
        {ST7789_CMD_VDVSET, 0, 1, (const uint8_t *)"\x20"},
        // AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V
        {ST7789_CMD_PWCTRL1, 0, 2, (const uint8_t *)"\xa4\xa1"},
        // 60 fps
        {ST7789_CMD_FRCTR2, 0, 1, (const uint8_t *)"\x0f"},
        // Gama 2.2
        {ST7789_CMD_GAMSET, 0, 1, (const uint8_t *)"\x01"},
        // Gama curve
        {ST7789_CMD_PVGAMCTRL, 0, 14, (const uint8_t *)"\xd0\x08\x11\x08\x0c\x15\x39\x33\x50\x36\x13\x14\x29\x2d"},
        {ST7789_CMD_NVGAMCTRL, 0, 14, (const uint8_t *)"\xd0\x08\x10\x08\x06\x06\x39\x44\x51\x0b\x16\x14\x2f\x31"},

        // Little endian
        {ST7789_CMD_RAMCTRL, 0, 2, (const uint8_t *)"\x00\xc8"},
        {ST7789_CMDLIST_END, 0, 0, NULL}, // End of commands
    };
    ST7789_multi_cmd(init_sequence);
    ST7789_fill_area(0x0000, 0, 0, global_dev->width, global_dev->height);
    const st7789_command_t init_sequence2[] = {
        {ST7789_CMD_DISPON, 100, 0, NULL}, // Display on
        {ST7789_CMD_SLPOUT, 100, 0, NULL}, // Sleep out
        {ST7789_CMD_CASET, 0, 4, caset},
        {ST7789_CMD_RASET, 0, 4, raset},
        {ST7789_CMD_RAMWR, 0, 0, NULL},
        {ST7789_CMDLIST_END, 0, 0, NULL}, // End of commands
    };
    ST7789_multi_cmd(init_sequence2);
}

void ST7789_pre_cb(spi_transaction_t *transaction)
{
    st7789_transaction_data_t *trans_data = (st7789_transaction_data_t *)transaction->user;
    gpio_set_level(global_dev->dc_pin, trans_data->data ? 1 : 0);
}

void ST7789_multi_cmd(const st7789_command_t *sequence)
{
    while (sequence->command != ST7789_CMDLIST_END)
    {
        ST7789_send_cmd(sequence);
        if (sequence->wait_ms > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(sequence->wait_ms));
        }
        sequence++;
    }
}
