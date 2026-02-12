#include "uFlakeCore.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "kernel.h"
#include "uI2c.h"
#include "uSPI.h"
#include "unvs.h"
#include "uUART.h"
#include "uGPIO.h"

#include "uFlakeAppReg.h"

#include "nrf24.h"
#include "ST7789.h"
#include "sdCard.h"
#include "uGui.h"
#include "uBootScreen.h"

static const char *TAG = "UFLAKE_CORE";

static st7789_driver_t display;

// Configure and initialize SD card
void config_and_init_sd_card(void)
{
    // initialize SD card
    SD_CardConfig sd_config = {};
    sd_config.csPin = GPIO_NUM_39;            // Chip Select pin
    sd_config.clockSpeedHz = USPI_FREQ_20MHZ; // 20 MHz for initialization
    sd_config.host = USPI_HOST_SPI2;          // Use SPI2 host

    if (!sdCard_init(&sd_config))
    {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }
}

void config_and_init_display()
{
    ESP_LOGI(TAG, "Configuring display...");

    // Configure display structure
    display.pin_cs = GPIO_NUM_10;
    display.pin_reset = GPIO_NUM_46;
    display.pin_dc = GPIO_NUM_14;

    display.display_width = 240;
    display.display_height = 320;
    display.orientation = 0;
    display.spi_host = USPI_HOST_SPI3;
    display.spi_speed = USPI_FREQ_80MHZ;
    display.buffer_size = 240 * 20; // 20 lines buffer

    // Initialize backlight PWM at 0% (boot animation will fade in)
    ugpio_pwm_start(GPIO_NUM_3, 1000, 0);

    // Initialize display
    if (!ST7789_init(&display))
    {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    ST7789_invert_display(&display, false);

    ESP_LOGI(TAG, "Display initialized successfully");
}

// Configure and initialize NRF24L01+
void config_and_init_nrf24()
{
    NRF24_t nrf24_dev = {};
    nrf24_dev.cePin = GPIO_NUM_48;
    nrf24_dev.csnPin = GPIO_NUM_45;
    nrf24_dev.channel = 76;
    nrf24_dev.payload = 16;
    nrf24_dev.spiHost = USPI_HOST_SPI3;
    nrf24_dev.frequency = USPI_FREQ_20MHZ;
    nrf24_dev.status = 0; // initialize status to a known value

    if (!Nrf24_init(&nrf24_dev))
    {
        ESP_LOGE(TAG, "Failed to initialize NRF24L01+");
        return;
    }

    ESP_LOGI(TAG, "NRF24L01+ initialized successfully");

    // check connection
    if (!Nrf24_isConnected(&nrf24_dev))
    {
        ESP_LOGI(TAG, "NRF24L01+ is connected");
    }
    else
    {
        ESP_LOGE(TAG, "NRF24L01+ is NOT connected");
    }
}

void uflake_core_init(void)
{
    // Initialize the kernel
    uflake_kernel_init();

    // Start the kernel
    uflake_kernel_start();

    // initialize nvs subsystem
    unvs_init();

    // initialize I2C
    i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ);

    // INITIALIZE the FIRST SPI BUS  - before adding any devices
    uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 32768);
    // INITIALIZE the Second SPI BUS FIRST - before adding any devices
    uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096);

    config_and_init_display();

    // Show splash immediately
    uboot_screen_start(&display);
    vTaskDelay(pdMS_TO_TICKS(6000)); // Show for 6 seconds
    uboot_screen_stop();

    config_and_init_nrf24();
    config_and_init_sd_card();

    uGui_init(&display);

    register_builtin_apps();

    ESP_LOGI(TAG, "uFlake Core initialized successfully");
}