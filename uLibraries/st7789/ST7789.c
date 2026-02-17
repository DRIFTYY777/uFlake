#include "ST7789.h"
#include "esp_heap_caps.h"

#include "kernel.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/param.h>

static const char *TAG = "ST7789_LVGL";

static void ST7789_send_cmd(st7789_driver_t *driver, const st7789_command_t *command);
static void ST7789_config(st7789_driver_t *driver);
static void ST7789_multi_cmd(st7789_driver_t *driver, const st7789_command_t *sequence);

bool ST7789_init(st7789_driver_t *driver)
{
    UFLAKE_LOGI(TAG, "Initializing ST7789 display...");

    // Allocate buffer memory using uFlake kernel memory manager
    driver->buffer = (st7789_color_t *)uflake_malloc(driver->buffer_size * 2 * sizeof(st7789_color_t), UFLAKE_MEM_DMA);
    if (driver->buffer == NULL)
    {
        UFLAKE_LOGE(TAG, "Display buffer allocation fail");
        return false;
    }

    UFLAKE_LOGI(TAG, "Display buffer allocated with size: %zu bytes", driver->buffer_size * 2 * sizeof(st7789_color_t));

    // Set-up the display buffers
    driver->buffer_primary = driver->buffer;
    driver->buffer_secondary = driver->buffer + driver->buffer_size;
    driver->current_buffer = driver->buffer_primary;
    driver->queue_fill = 0;

    driver->data.driver = driver;
    driver->data.data = true;
    driver->command.driver = driver;
    driver->command.data = false;

    // Configure GPIO pins
    gpio_reset_pin(driver->pin_reset);
    gpio_reset_pin(driver->pin_dc);
    gpio_set_direction(driver->pin_reset, GPIO_MODE_OUTPUT);
    gpio_set_direction(driver->pin_dc, GPIO_MODE_OUTPUT);

    UFLAKE_LOGI(TAG, "GPIO configured - RST: %d, DC: %d", driver->pin_reset, driver->pin_dc);

    // Configure SPI device using uFlake HAL
    uspi_device_config_t spi_config = {
        .cs_pin = driver->pin_cs,
        .clock_speed_hz = driver->spi_speed,
        .mode = USPI_MODE_3,
        .queue_size = ST7789_SPI_QUEUE_SIZE,
        .cs_ena_pretrans = false,
        .cs_ena_posttrans = false,
        .address_bits = 0,
        .command_bits = 0,
        .dummy_bits = 0,
        .device_type = USPI_DEVICE_DISPLAY,
        .device_name = "ST7789"};

    esp_err_t ret = uspi_device_add(driver->spi_host, &spi_config, &driver->spi);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        uflake_free(driver->buffer);
        return false;
    }

    UFLAKE_LOGI(TAG, "SPI device configured via uFlake HAL");

    // Initialize the display
    ST7789_reset(driver);
    ST7789_config(driver);

    UFLAKE_LOGI(TAG, "Display configured and ready (%dx%d)", driver->display_width, driver->display_height);

    return true;
}

bool ST7789_deinit(st7789_driver_t *driver)
{
    UFLAKE_LOGI(TAG, "Deinitializing ST7789 display...");

    // Remove SPI device
    esp_err_t ret = uspi_device_remove(driver->spi);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to remove SPI device: %s", esp_err_to_name(ret));
        return false;
    }

    UFLAKE_LOGI(TAG, "SPI device removed");

    // Free allocated buffer memory
    if (driver->buffer)
    {
        uflake_free(driver->buffer);
        driver->buffer = NULL;
        UFLAKE_LOGI(TAG, "Display buffer freed");
    }

    return true;
}

void ST7789_reset(st7789_driver_t *driver)
{
    gpio_set_level(driver->pin_reset, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    gpio_set_level(driver->pin_reset, 1);
    vTaskDelay(130 / portTICK_PERIOD_MS);
}

void ST7789_fill_area(st7789_driver_t *driver, st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height)
{
    // Fill the buffer with the selected color
    for (size_t i = 0; i < driver->buffer_size * 2; ++i)
    {
        driver->buffer[i] = color;
    }

    // Set the working area on the screen
    ST7789_set_window(driver, start_x, start_y, start_x + width - 1, start_y + height - 1);

    // Set DC high for pixel data
    gpio_set_level(driver->pin_dc, 1);

    size_t bytes_to_write = width * height * 2;
    size_t transfer_size = driver->buffer_size * 2 * sizeof(st7789_color_t);

    spi_transaction_t trans;
    spi_transaction_t *rtrans;

    memset(&trans, 0, sizeof(trans));
    trans.tx_buffer = driver->buffer;
    trans.length = transfer_size * 8;
    trans.rxlength = 0;

    while (bytes_to_write > 0)
    {
        if (driver->queue_fill >= ST7789_SPI_QUEUE_SIZE)
        {
            spi_device_get_trans_result(driver->spi, &rtrans, portMAX_DELAY);
            driver->queue_fill--;
        }
        if (bytes_to_write < transfer_size)
        {
            transfer_size = bytes_to_write;
        }
        spi_device_queue_trans(driver->spi, &trans, portMAX_DELAY);
        driver->queue_fill++;
        bytes_to_write -= transfer_size;
    }

    ST7789_queue_empty(driver);
}

void ST7789_write_pixels(st7789_driver_t *driver, st7789_color_t *pixels, size_t length)
{
    (void)pixels; // Unused parameter - uses current_buffer instead
    ST7789_queue_empty(driver);

    // Set DC high for pixel data
    gpio_set_level(driver->pin_dc, 1);

    spi_transaction_t *trans = driver->current_buffer == driver->buffer_primary ? &driver->trans_a : &driver->trans_b;
    memset(trans, 0, sizeof(spi_transaction_t));
    trans->tx_buffer = driver->current_buffer;
    trans->length = length * sizeof(st7789_color_t) * 8;
    trans->rxlength = 0;

    spi_device_queue_trans(driver->spi, trans, portMAX_DELAY);
    driver->queue_fill++;
}

void ST7789_write_lines(st7789_driver_t *driver, int ypos, int xpos, int width, uint16_t *linedata, int lineCount)
{
    // ST7789_set_window(driver,xpos,ypos,240,ypos +20);
    (void)xpos;      // Unused parameter
    (void)linedata;  // Unused parameter
    (void)lineCount; // Unused parameter
    (void)width;     // Unused parameter

    // driver->buffer_secondary = linedata;
    // driver->current_buffer = driver->buffer_secondary;

    // ST7789_write_pixels(driver, driver->buffer_primary, size);
    driver->buffer_size = 240 * 20;
    ST7789_set_window(driver, 0, ypos, 240, ypos + 20);
    // ST7789_write_pixels(driver, driver->current_buffer, driver->buffer_size);
    ST7789_swap_buffers(driver);
}

void ST7789_swap_buffers(st7789_driver_t *driver)
{
    ST7789_write_pixels(driver, driver->current_buffer, driver->buffer_size);
    driver->current_buffer = driver->current_buffer == driver->buffer_primary ? driver->buffer_secondary : driver->buffer_primary;
}

void ST7789_set_window(st7789_driver_t *driver, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y)
{
    uint8_t caset[4];
    uint8_t raset[4];

    caset[0] = (uint8_t)(start_x >> 8) & 0xFF;
    caset[1] = (uint8_t)(start_x & 0xff);
    caset[2] = (uint8_t)(end_x >> 8) & 0xFF;
    caset[3] = (uint8_t)(end_x & 0xff);
    raset[0] = (uint8_t)(start_y >> 8) & 0xFF;
    raset[1] = (uint8_t)(start_y & 0xff);
    raset[2] = (uint8_t)(end_y >> 8) & 0xFF;
    raset[3] = (uint8_t)(end_y & 0xff);

    st7789_command_t sequence[] = {
        {ST7789_CMD_CASET, 0, 4, caset},
        {ST7789_CMD_RASET, 0, 4, raset},
        {ST7789_CMD_RAMWR, 0, 0, NULL},
        {ST7789_CMDLIST_END, 0, 0, NULL},
    };

    ST7789_multi_cmd(driver, sequence);
}

void ST7789_set_endian(st7789_driver_t *driver)
{
    const st7789_command_t init_sequence2[] = {
        {ST7789_CMD_RAMCTRL, 0, 2, (const uint8_t *)"\x00\xc0"},
        {ST7789_CMDLIST_END, 0, 0, NULL},
    };
    ST7789_multi_cmd(driver, init_sequence2);
}

void ST7789_invert_display(st7789_driver_t *driver, bool invert)
{
    const st7789_command_t init_sequence2[] = {
        {invert ? ST7789_CMD_INVON : ST7789_CMD_INVOFF, 0, 0, NULL},
        {ST7789_CMDLIST_END, 0, 0, NULL},
    };
    ST7789_multi_cmd(driver, init_sequence2);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void ST7789_config(st7789_driver_t *driver)
{

    const uint8_t caset[4] = {
        0x00,
        0x00,
        (driver->display_width - 1) >> 8,
        (driver->display_width - 1) & 0xff};
    const uint8_t raset[4] = {
        0x00,
        0x00,
        (driver->display_height - 1) >> 8,
        (driver->display_height - 1) & 0xff};
    const st7789_command_t init_sequence[] = {
        // Sleep
        {ST7789_CMD_SLPIN, 10, 0, NULL},    // Sleep
        {ST7789_CMD_SWRESET, 200, 0, NULL}, // Reset
        {ST7789_CMD_SLPOUT, 120, 0, NULL},  // Sleep out

        {ST7789_CMD_MADCTL, 0, 1, (const uint8_t *)"\xA0"}, // Landscape mode rotated 180° (MY=1, MV=1)
        {ST7789_CMD_COLMOD, 0, 1, (const uint8_t *)"\x55"}, // 16 bit RGB
        {ST7789_CMD_INVON, 0, 0, NULL},                     // Inversion on
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
    ST7789_multi_cmd(driver, init_sequence);
    ST7789_fill_area(driver, 0x0000, 0, 0, driver->display_width, driver->display_height);
    const st7789_command_t init_sequence2[] = {
        {ST7789_CMD_DISPON, 100, 0, NULL}, // Display on
        {ST7789_CMD_SLPOUT, 100, 0, NULL}, // Sleep out
        {ST7789_CMD_CASET, 0, 4, caset},
        {ST7789_CMD_RASET, 0, 4, raset},
        {ST7789_CMD_RAMWR, 0, 0, NULL},
        {ST7789_CMDLIST_END, 0, 0, NULL}, // End of commands
    };
    ST7789_multi_cmd(driver, init_sequence2);
}

static void ST7789_send_cmd(st7789_driver_t *driver, const st7789_command_t *command)
{
    spi_transaction_t *return_trans;
    spi_transaction_t data_trans;

    // Check if the SPI queue is empty
    ST7789_queue_empty(driver);

    // Set DC low for command
    gpio_set_level(driver->pin_dc, 0);

    // Send the command
    memset(&data_trans, 0, sizeof(data_trans));
    data_trans.length = 8; // 8 bits
    data_trans.tx_buffer = &command->command;

    spi_device_queue_trans(driver->spi, &data_trans, portMAX_DELAY);
    spi_device_get_trans_result(driver->spi, &return_trans, portMAX_DELAY);

    // Send the data if the command has.
    if (command->data_size > 0)
    {
        // Set DC high for data
        gpio_set_level(driver->pin_dc, 1);

        memset(&data_trans, 0, sizeof(data_trans));
        data_trans.length = command->data_size * 8;
        data_trans.tx_buffer = command->data;

        spi_device_queue_trans(driver->spi, &data_trans, portMAX_DELAY);
        spi_device_get_trans_result(driver->spi, &return_trans, portMAX_DELAY);
    }

    // Wait the required time
    if (command->wait_ms > 0)
    {
        vTaskDelay(command->wait_ms / portTICK_PERIOD_MS);
    }
}

static void ST7789_multi_cmd(st7789_driver_t *driver, const st7789_command_t *sequence)
{
    while (sequence->command != ST7789_CMDLIST_END)
    {
        ST7789_send_cmd(driver, sequence);
        sequence++;
    }
}

void ST7789_queue_empty(st7789_driver_t *driver)
{
    spi_transaction_t *return_trans;
    const TickType_t timeout_ticks = pdMS_TO_TICKS(1000); // 1 second timeout
    uint32_t timeout_count = 0;

    while (driver->queue_fill > 0)
    {
        // ✅ FIX: Use bounded timeout instead of portMAX_DELAY
        // If SPI gets stuck (DMA issue, bus contention), this prevents infinite hang
        esp_err_t ret = spi_device_get_trans_result(driver->spi, &return_trans, timeout_ticks);
        
        if (ret == ESP_OK)
        {
            driver->queue_fill--;
            timeout_count = 0; // Reset timeout counter on success
        }
        else if (ret == ESP_ERR_TIMEOUT)
        {
            timeout_count++;
            UFLAKE_LOGW(TAG, "SPI transaction timeout #%lu (queue_fill=%d)", timeout_count, driver->queue_fill);
            
            // After 3 consecutive timeouts, force reset queue to prevent deadlock
            if (timeout_count >= 3)
            {
                UFLAKE_LOGE(TAG, "SPI queue stuck after 3 timeouts, forcing reset");
                driver->queue_fill = 0; // Emergency reset
                break;
            }
        }
        else
        {
            UFLAKE_LOGE(TAG, "SPI transaction error: %d", ret);
            driver->queue_fill = 0; // Reset on error
            break;
        }
    }
}
