/**
 * @file ST7789_example.c
 * @brief Example usage of ST7789 display driver with uFlake SPI and LVGL
 * 
 * This example demonstrates how to initialize and use the ST7789 display
 * with the uFlake HAL SPI interface and LVGL graphics library.
 */

#include "ST7789.h"
#include "uSPI.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ST7789_EXAMPLE";

// Example configuration for ESP32-S3 with ST7789 240x320 display
#define EXAMPLE_LCD_HOST        USPI_HOST_SPI2
#define EXAMPLE_LCD_MOSI        GPIO_NUM_35
#define EXAMPLE_LCD_MISO        GPIO_NUM_NC
#define EXAMPLE_LCD_SCLK        GPIO_NUM_36
#define EXAMPLE_LCD_CS          GPIO_NUM_34
#define EXAMPLE_LCD_DC          GPIO_NUM_37
#define EXAMPLE_LCD_RST         GPIO_NUM_38
#define EXAMPLE_LCD_BL          GPIO_NUM_45

#define EXAMPLE_LCD_WIDTH       240
#define EXAMPLE_LCD_HEIGHT      320
#define EXAMPLE_LCD_FREQ        USPI_FREQ_40MHZ

// Global display instance
static st7789_lvgl_t display;

/**
 * @brief LVGL tick task for proper timing
 */
static void lvgl_tick_task(void *arg)
{
    while (1)
    {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief LVGL handler task
 */
static void lvgl_handler_task(void *arg)
{
    while (1)
    {
        uint32_t time_till_next = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(time_till_next > 0 ? time_till_next : 5));
    }
}

/**
 * @brief Create a simple LVGL UI demo
 */
static void create_demo_ui(void)
{
    // Create a label
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "uFlake ST7789\nDisplay Driver");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);

    // Create a button
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_size(btn, 120, 50);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Button");
    lv_obj_center(btn_label);

    // Create a bar
    lv_obj_t *bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(bar, 200, 20);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 100);
    lv_bar_set_value(bar, 70, LV_ANIM_OFF);
}

/**
 * @brief Initialize display and LVGL
 */
esp_err_t example_display_init(void)
{
    ESP_LOGI(TAG, "Initializing display...");

    // Initialize LVGL
    lv_init();

    // Initialize uFlake SPI bus first
    uflake_result_t result = uspi_bus_init(
        EXAMPLE_LCD_HOST,
        EXAMPLE_LCD_MOSI,
        EXAMPLE_LCD_MISO,
        EXAMPLE_LCD_SCLK,
        USPI_MAX_TRANSFER_SIZE
    );

    if (result != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", result);
        return ESP_FAIL;
    }

    // Configure display
    display.cs_pin = EXAMPLE_LCD_CS;
    display.reset_pin = EXAMPLE_LCD_RST;
    display.dc_pin = EXAMPLE_LCD_DC;
    display.bl_pin = EXAMPLE_LCD_BL;
    display.width = EXAMPLE_LCD_WIDTH;
    display.height = EXAMPLE_LCD_HEIGHT;
    display.host = EXAMPLE_LCD_HOST;
    display.frequency = EXAMPLE_LCD_FREQ;
    display.orientation = 0; // Portrait

    // Initialize display
    esp_err_t ret = st7789_init(&display);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start LVGL tasks
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 2048, NULL, 5, NULL);
    xTaskCreate(lvgl_handler_task, "lvgl_handler", 4096, NULL, 5, NULL);

    // Create demo UI
    create_demo_ui();

    ESP_LOGI(TAG, "Display initialized successfully");

    return ESP_OK;
}

/**
 * @brief Change display orientation example
 */
void example_change_orientation(uint8_t orientation)
{
    st7789_set_orientation(&display, orientation);
}

/**
 * @brief Control backlight example
 */
void example_backlight_control(bool on)
{
    st7789_backlight(&display, on);
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ST7789 Example Starting...");

    // Initialize display
    if (example_display_init() == ESP_OK)
    {
        ESP_LOGI(TAG, "Display running");

        // Example: Change orientation after 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
        example_change_orientation(2); // Landscape

        // Example: Toggle backlight
        vTaskDelay(pdMS_TO_TICKS(5000));
        example_backlight_control(false);
        vTaskDelay(pdMS_TO_TICKS(1000));
        example_backlight_control(true);
    }
    else
    {
        ESP_LOGE(TAG, "Display initialization failed");
    }
}
