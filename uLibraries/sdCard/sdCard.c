#include "sdCard.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <sys/stat.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "uSPI.h"
#include "kernel.h"

#define MOUNT_POINT "/sd"
static const char *TAG = "sdCard";

// make config global to be accessible in read/write functions
static SD_CardConfig *config = NULL;
static bool sd_is_mounted = false;
static volatile bool init_requested = false;
static volatile bool deinit_requested = false;

int sdCard_init(SD_CardConfig *cfg)
{
    /* make the supplied config available to other functions in this file */
    config = cfg;

    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // Use the host and clock from the provided config so the SDSPI mount uses
    // the same SPI bus and the desired initial clock frequency.
    host.slot = cfg->host;
    host.max_freq_khz = cfg->clockSpeedHz / 1000;

    // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
    // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
    // and the internal LDO power supply, we need to initialize the power supply first.
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return 0;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    // NOTE: Do NOT add a separate SPI device with `uspi_device_add` for the SD card.
    // The SDSPI mount will create and manage the SPI device on the bus; adding another
    // device with the same CS pin can cause conflicts and timeouts.

    // spi_bus_config_t bus_cfg = {0};
    // ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to initialize bus.");
    //     return 0;
    // }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = config->csPin;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    /* give SD card time to power up / settle */

    vTaskDelay(pdMS_TO_TICKS(100));

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
            check_sd_card_pins(&cfg, pin_count);
#endif
        }
        return 0;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    sd_is_mounted = true;
    return 1;
}

void sdCard_deinit(void)
{
    if (!sd_is_mounted)
    {
        ESP_LOGW(TAG, "SD card is not mounted, skipping unmount");
        return;
    }

    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, NULL);

    // remove from the spi bus
    uspi_device_remove(config->spi);

    sd_is_mounted = false;
    ESP_LOGI(TAG, "SD card unmounted");
}

// Handler tasks for init and deinit
static void sd_init_handler(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(200)); // Debounce delay

    if (gpio_get_level((gpio_num_t)config->sdDetectPin) == 0 && !sd_is_mounted)
    {
        ESP_LOGI(TAG, "Initializing SD card from interrupt...");
        int ret = sdCard_init(config);
        if (ret != 1)
        {
            ESP_LOGE(TAG, "SD card initialization failed from interrupt");
            // Publish error event
            uflake_event_publish("sd.init.failed", EVENT_TYPE_ERROR, NULL, 0);
        }
        else
        {
            ESP_LOGI(TAG, "SD card initialized successfully from interrupt");
            // Publish success event
            uflake_event_publish("sd.init.success", EVENT_TYPE_HARDWARE, NULL, 0);
        }
    }

    init_requested = false;
    // Process terminates automatically when function returns
}

static void sd_deinit_handler(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(100)); // Small delay

    if (gpio_get_level((gpio_num_t)config->sdDetectPin) == 1 && sd_is_mounted)
    {
        ESP_LOGI(TAG, "Unmounting SD card from interrupt...");
        sdCard_deinit();
        // Publish removal event
        uflake_event_publish("sd.removed", EVENT_TYPE_HARDWARE, NULL, 0);
    }

    deinit_requested = false;
    // Process terminates automatically when function returns
}

// GPIO ISR handler for SD card detect pin
static void IRAM_ATTR sd_detect_isr_handler(void *arg)
{
    // Read the pin state (assumes active LOW for card present)
    int pin_state = gpio_get_level((gpio_num_t)(int)arg);

    // If card is detected (pin LOW), trigger initialization
    if (pin_state == 0 && !init_requested && !sd_is_mounted)
    {
        ESP_EARLY_LOGI(TAG, "SD card inserted");
        init_requested = true;

        // Create a one-shot process to handle initialization
        uint32_t pid;
        uflake_process_create(
            "sd_init",             // Process name
            sd_init_handler,       // Entry point
            NULL,                  // Arguments
            4096,                  // Stack size
            PROCESS_PRIORITY_HIGH, // Priority
            &pid                   // PID output
        );
    }
    else if (pin_state == 1 && !deinit_requested && sd_is_mounted)
    {
        ESP_EARLY_LOGI(TAG, "SD card removed");
        deinit_requested = true;

        // Create a one-shot process to handle deinitialization
        uint32_t pid;
        uflake_process_create(
            "sd_deinit",           // Process name
            sd_deinit_handler,     // Entry point
            NULL,                  // Arguments
            2048,                  // Stack size
            PROCESS_PRIORITY_HIGH, // Priority
            &pid                   // PID output
        );
    }
}

int sdCard_setupDetectInterrupt(SD_CardConfig *cfg)
{
    if (cfg->sdDetectPin == SD_DETECT_PIN_DISABLED)
    {
        ESP_LOGI(TAG, "SD detect pin disabled, skipping interrupt setup");
        return 1;
    }

    config = cfg;

    // Configure the detect pin as input with pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << cfg->sdDetectPin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE // Trigger on both edges (insert/remove)
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure SD detect pin: %s", esp_err_to_name(ret));
        return 0;
    }

    // Install GPIO ISR service if not already installed
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return 0;
    }

    // Attach the interrupt handler
    ret = gpio_isr_handler_add((gpio_num_t)cfg->sdDetectPin, sd_detect_isr_handler, (void *)(int)cfg->sdDetectPin);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        return 0;
    }

    ESP_LOGI(TAG, "SD card detect interrupt configured on pin %d", cfg->sdDetectPin);

    // Check current state and initialize if card is already present
    if (gpio_get_level((gpio_num_t)cfg->sdDetectPin) == 0)
    {
        ESP_LOGI(TAG, "SD card already present, initializing...");
        vTaskDelay(pdMS_TO_TICKS(200));
        int init_ret = sdCard_init(cfg);
        if (init_ret != 1)
        {
            ESP_LOGW(TAG, "Initial SD card initialization failed");
        }
    }

    return 1;
}

void sdCard_removeDetectInterrupt(void)
{
    if (config != NULL && config->sdDetectPin != SD_DETECT_PIN_DISABLED)
    {
        gpio_isr_handler_remove((gpio_num_t)config->sdDetectPin);
        ESP_LOGI(TAG, "SD card detect interrupt removed");
    }
}
