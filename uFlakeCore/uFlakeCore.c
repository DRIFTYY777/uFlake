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
#include "uFlakeAppReg.h"

#include "sdCard.h"
#include "nrf24.h"
#include "pca9555.h"
#include "ST7789.h"
#include "lvgl.h"

static const char *TAG = "UFLAKE_CORE";

// LVGL display and buffers
static st7789_driver_t display;
static lv_display_t *lv_disp = NULL;
static lv_color_t *lv_buf1 = NULL;
static lv_color_t *lv_buf2 = NULL;
static SemaphoreHandle_t xGuiSemaphore = NULL;

#define LV_TICK_PERIOD_MS 10

// Forward declarations
static void lv_tick_timer_cb(void *arg);
static void gui_task(void *arg);
static void create_lvgl_demo_ui(void);
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

// Input reader task for button handling
static void input_read_task(void *arg)
{
    ESP_LOGI(TAG, "[INPUT_TASK] Starting input read task");

    // initialize PCA9555 as input
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for PCA9555 to stabilize

    ESP_LOGI(TAG, "[INPUT_TASK] Initialization complete, entering main loop");

    while (1)
    {
        uint16_t inputs_value = read_pca9555_inputs(UI2C_PORT_0, PCA9555_ADDRESS);
        if (!((inputs_value >> 0) & 0x01))
        {
            ESP_LOGI(TAG, "_Up pressed");
        }

        if (!((inputs_value >> 1) & 0x01))
        {
            ESP_LOGI(TAG, "_Down pressed");
        }

        if (!((inputs_value >> 2) & 0x01))
        {
            ESP_LOGI(TAG, "_Right pressed");
        }

        if (!((inputs_value >> 3) & 0x01))
        {
            ESP_LOGI(TAG, "_Left pressed");
        }

        if (!((inputs_value >> 4) & 0x01))
        {
            // Not used
        }

        if (!((inputs_value >> 5) & 0x01))
        {
            ESP_LOGI(TAG, "_Menu pressed");
        }

        if (!((inputs_value >> 6) & 0x01))
        {
            ESP_LOGI(TAG, "_Back pressed");
        }

        if (!((inputs_value >> 7) & 0x01))
        {
            ESP_LOGI(TAG, "_OK pressed");
        }

        if (!((inputs_value >> 8) & 0x01))
        {
            ESP_LOGI(TAG, "_Home pressed");
        }

        if (!((inputs_value >> 9) & 0x01))
        {
            ESP_LOGI(TAG, "_A pressed");
        }
        if (!((inputs_value >> 10) & 0x01))
        {
            ESP_LOGI(TAG, "_B pressed");
        }

        if (!((inputs_value >> 11) & 0x01))
        {
            ESP_LOGI(TAG, "_Y pressed");
        }
        if (!((inputs_value >> 12) & 0x01))
        {
            ESP_LOGI(TAG, "_X pressed");
        }

        if (!((inputs_value >> 13) & 0x01))
        {
            ESP_LOGI(TAG, "_L1 pressed");
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100 ms
    }
}

// Configure and initialize SD card
void config_and_init_sd_card(void)
{
    // initialize SD card
    SD_CardConfig sd_config = {};
    sd_config.csPin = GPIO_NUM_39;            // Chip Select pin
    sd_config.clockSpeedHz = USPI_FREQ_20MHZ; // 40 MHz for initialization
    sd_config.host = USPI_HOST_SPI2;          // Use SPI2 host

    if (!sdCard_init(&sd_config))
    {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }
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
    if (Nrf24_isConnected(&nrf24_dev))
    {
        ESP_LOGI(TAG, "NRF24L01+ is connected");
    }
    else
    {
        ESP_LOGE(TAG, "NRF24L01+ is NOT connected");
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
    display.spi_speed = USPI_FREQ_40MHZ;
    display.buffer_size = 240 * 20; // 20 lines buffer

    // make gpio 3 output for backlight
    gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_3, 1); // turn on backlight

    // Initialize display
    if (!ST7789_init(&display))
    {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    ESP_LOGI(TAG, "Display initialized successfully");

    // Initialize LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // Allocate LVGL draw buffers using kernel memory manager (DMA-capable)
    size_t buf_size = display.display_width * 30; // 30 lines buffer
    lv_buf1 = (lv_color_t *)uflake_malloc(buf_size * sizeof(lv_color_t), UFLAKE_MEM_DMA);
    lv_buf2 = (lv_color_t *)uflake_malloc(buf_size * sizeof(lv_color_t), UFLAKE_MEM_DMA);

    if (!lv_buf1 || !lv_buf2)
    {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        if (lv_buf1)
            uflake_free(lv_buf1);
        if (lv_buf2)
            uflake_free(lv_buf2);
        return;
    }

    ESP_LOGI(TAG, "LVGL buffers allocated: %zu bytes each", buf_size * sizeof(lv_color_t));

    // Create LVGL display
    lv_disp = lv_display_create(display.display_width, display.display_height);
    if (!lv_disp)
    {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        uflake_free(lv_buf1);
        uflake_free(lv_buf2);
        return;
    }

    // Configure LVGL display with double buffering
    lv_display_set_buffers(lv_disp, lv_buf1, lv_buf2,
                           buf_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_set_user_data(lv_disp, &display);

    ESP_LOGI(TAG, "LVGL display configured with double buffering");

    // Create semaphore for LVGL thread safety
    xGuiSemaphore = xSemaphoreCreateMutex();
    if (xGuiSemaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed to create GUI semaphore");
        return;
    }

    // Create ESP timer for LVGL ticks
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_timer_cb,
        .name = "lvgl_tick"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "LVGL tick timer started");

    // Create GUI task using kernel process manager
    uint32_t gui_pid;
    if (uflake_process_create("GUI_Task", gui_task, NULL, 1024 * 8,
                              PROCESS_PRIORITY_HIGH, &gui_pid) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create GUI process");
        return;
    }

    ESP_LOGI(TAG, "GUI process created (PID: %lu)", gui_pid);
}

// LVGL flush callback - sends pixel data to display
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    st7789_driver_t *driver = (st7789_driver_t *)lv_display_get_user_data(disp);

    if (!driver || !px_map)
    {
        ESP_LOGE(TAG, "Invalid flush parameters");
        lv_display_flush_ready(disp);
        return;
    }

    // Calculate buffer size
    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);

    // Set drawing window
    ST7789_set_window(driver, area->x1, area->y1, area->x2, area->y2);

    // Use driver's buffer management
    driver->current_buffer = (st7789_color_t *)px_map;
    driver->buffer_size = size;

    // Send data using driver's swap buffers mechanism
    ST7789_swap_buffers(driver);

    // Inform LVGL that flushing is done
    lv_display_flush_ready(disp);
}

// LVGL tick timer callback
static void lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

// GUI task - handles LVGL with semaphore protection
static void gui_task(void *arg)
{
    (void)arg;

    // Small delay to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(100));

    // Create the demo UI after LVGL task is running
    create_lvgl_demo_ui();

    ESP_LOGI(TAG, "GUI task entering main loop");

    while (1)
    {
        if (xGuiSemaphore != NULL)
        {
            if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE)
            {
                lv_timer_handler();
                xSemaphoreGive(xGuiSemaphore);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay for better task yielding
    }
}

// Create LVGL demo UI
static void create_lvgl_demo_ui(void)
{
    // Take semaphore before creating UI elements
    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE)
    {
        ESP_LOGI(TAG, "Creating LVGL demo UI...");

        // Create a title label
        lv_obj_t *title = lv_label_create(lv_screen_active());
        lv_label_set_text(title, "uFlake LVGL Demo");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);

        // Create a button
        lv_obj_t *btn = lv_button_create(lv_screen_active());
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, -30);
        lv_obj_set_size(btn, 150, 60);

        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, "Click Me!");
        lv_obj_center(btn_label);

        // Create a slider
        lv_obj_t *slider = lv_slider_create(lv_screen_active());
        lv_obj_align(slider, LV_ALIGN_CENTER, 0, 40);
        lv_obj_set_width(slider, 180);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);

        // Create colored rectangles
        lv_obj_t *rect = lv_obj_create(lv_screen_active());
        lv_obj_set_size(rect, 60, 60);
        lv_obj_align(rect, LV_ALIGN_BOTTOM_LEFT, 20, -20);
        lv_obj_set_style_bg_color(rect, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_border_width(rect, 0, 0);

        lv_obj_t *rect2 = lv_obj_create(lv_screen_active());
        lv_obj_set_size(rect2, 60, 60);
        lv_obj_align(rect2, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
        lv_obj_set_style_bg_color(rect2, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_border_width(rect2, 0, 0);

        ESP_LOGI(TAG, "LVGL demo UI created");

        xSemaphoreGive(xGuiSemaphore);
    }
}

void uflake_core_init(void)
{
    printf("Internal heap: %zu bytes\n", (size_t)esp_get_free_heap_size());
    printf("PSRAM heap: %zu bytes\n",
           (size_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Initialize the kernel
    if (uflake_kernel_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize uFlake Kernel");
        return;
    }

    // Start the kernel
    if (uflake_kernel_start() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to start uFlake Kernel");
        return;
    }

    // initialize nvs subsystem
    if (unvs_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS subsystem");
        return;
    }

    // initialize I2C
    if (i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return;
    }

    // INITIALIZE the FIRST SPI BUS  - before adding any devices
    // Need larger transfer size for display (240x40 pixels x 2 bytes = 19200 bytes)
    if (uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 32768) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return;
    }

    // INITIALIZE the Second SPI BUS FIRST - before adding any devices
    if (uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return;
    }

    config_and_init_nrf24();
    config_and_init_sd_card();

    // Create input reader process
    uint32_t input_pid;
    uflake_result_t result = uflake_process_create(
        "InputReader",
        input_read_task,
        NULL,
        4096,
        PROCESS_PRIORITY_NORMAL,
        &input_pid);

    if (result != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create Input Reader process");
        return;
    }

    config_and_init_display();

    register_builtin_apps();
    // Note: create_lvgl_demo_ui() is now called from within gui_task

    ESP_LOGI(TAG, "uFlake Core initialized successfully");
}