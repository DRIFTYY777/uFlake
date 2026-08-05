#include "uFlakeCore.h"

#include <stdio.h>

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
#include "uBootScreen.h"

#include "uGui_simple.h" // NEW: Updated to use new GUI system

#include "esp_elf.h"

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
        UFLAKE_LOGE(TAG, "Failed to initialize SD card");
        return;
    }
}

void config_and_init_display()
{
    UFLAKE_LOGI(TAG, "Configuring display...");

    // Configure display structure
    display.pin_cs = GPIO_NUM_10;
    display.pin_reset = GPIO_NUM_46;
    display.pin_dc = GPIO_NUM_14;

    display.display_width = 320;  // Landscape mode width
    display.display_height = 240; // Landscape mode height
    display.orientation = 0;      // Landscape mode
    display.spi_host = USPI_HOST_SPI3;
    display.spi_speed = USPI_FREQ_80MHZ;
    display.buffer_size = 320 * 20; // 20 lines buffer for landscape width

    // Initialize backlight PWM at 0% (boot animation will fade in)
    ugpio_pwm_start(GPIO_NUM_3, 1000, 0);

    // Initialize display
    if (!ST7789_init(&display))
    {
        UFLAKE_LOGE(TAG, "Failed to initialize display");
        return;
    }

    ST7789_invert_display(&display, false);

    UFLAKE_LOGI(TAG, "Display initialized successfully");
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
        UFLAKE_LOGE(TAG, "Failed to initialize NRF24L01+");
        return;
    }

    UFLAKE_LOGI(TAG, "NRF24L01+ initialized successfully");

    // check connection
    if (!Nrf24_isConnected(&nrf24_dev))
    {
        UFLAKE_LOGI(TAG, "NRF24L01+ is connected");
    }
    else
    {
        UFLAKE_LOGE(TAG, "NRF24L01+ is NOT connected");
    }
}

#include "input.h"

// ============== ELF Loading & Execution ==============
typedef int (*app_entry_t)(int argc, char *argv[]);

static int load_and_run_elf(const char *elf_path, int argc, char *argv[])
{
    ESP_LOGI(TAG, "Loading ELF from: %s", elf_path);

    // Load ELF file
    elf_img_handle_t handle = elf_loader_load(elf_path);
    if (handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to load ELF: %s", elf_path);
        return -1;
    }

    ESP_LOGI(TAG, "ELF loaded successfully");

    // Get entry point
    uint32_t entry_point = elf_loader_get_entry_point(handle);
    ESP_LOGI(TAG, "Entry point: 0x%08x", entry_point);

    // Get symbol (if entry point is not direct)
    // Example: Look for "app_main" symbol
    uint32_t app_main_addr = elf_loader_get_symbol(handle, "app_main");
    if (app_main_addr != 0)
    {
        ESP_LOGI(TAG, "Found app_main at: 0x%08x", app_main_addr);
        entry_point = app_main_addr;
    }

    // Cast to function pointer
    app_entry_t entry_fn = (app_entry_t)entry_point;

    // Execute the loaded app
    ESP_LOGI(TAG, "Executing loaded app...");
    int result = entry_fn(argc, argv);

    ESP_LOGI(TAG, "App returned: %d", result);

    // Cleanup
    elf_loader_unload(handle);

    return result;
}

// ============== Main Task ==============
static void elf_loader_task(void *pvParameters)
{
    ESP_LOGI(TAG, "ELF Loader Task Started");

    // Initialize SD card
    if (init_sdcard() != ESP_OK)
    {
        ESP_LOGE(TAG, "SD card initialization failed");
        vTaskDelete(NULL);
    }

    // Prepare arguments for the ELF app
    const char *elf_path = MOUNT_POINT "/elf_app.elf";
    char *argv[] = {
        "elf_app",
        "arg1",
        "arg2",
        NULL};
    int argc = 3;

    // Load and run the ELF app
    load_and_run_elf(elf_path, argc, argv);

    vTaskDelete(NULL);
}

void uflake_core_init(void)
{
    // Initialize the kernel
    uflake_kernel_init();

    // Start the kernel
    uflake_kernel_start();

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
    vTaskDelay(pdMS_TO_TICKS(1000)); // Show for 6 seconds
    uboot_screen_stop();

    config_and_init_nrf24();
    config_and_init_sd_card();

    uGui_init();

    register_builtin_apps();

    UFLAKE_LOGI(TAG, "uFlake Core initialized successfully");
}