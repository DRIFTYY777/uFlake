# ST7789 Display Driver for uFlake

A complete ST7789 TFT display driver integrated with uFlake SPI HAL and LVGL v9 graphics library.

## Features

- ✅ **uFlake SPI Integration**: Uses uFlake HAL SPI interface with DMA support
- ✅ **Full LVGL v9 Support**: Complete LVGL v9 display driver implementation
- ✅ **DMA Transfers**: Efficient pixel data transfer using DMA
- ✅ **Multiple Orientations**: Support for portrait and landscape modes
- ✅ **Backlight Control**: Software control of display backlight
- ✅ **Memory Efficient**: Uses DMA-capable memory for buffers
- ✅ **Double Buffering**: LVGL double buffering for smooth rendering

## Hardware Requirements

- ESP32-S3 (or compatible ESP32 chip)
- ST7789 TFT display (240x320 or other resolutions)
- SPI interface connections:
  - MOSI (Master Out Slave In)
  - SCLK (Serial Clock)
  - CS (Chip Select)
  - DC (Data/Command)
  - RST (Reset)
  - BL (Backlight - optional)

## Pin Configuration Example

```c
#define LCD_HOST        USPI_HOST_SPI2
#define LCD_MOSI        GPIO_NUM_35
#define LCD_SCLK        GPIO_NUM_36
#define LCD_CS          GPIO_NUM_34
#define LCD_DC          GPIO_NUM_37
#define LCD_RST         GPIO_NUM_38
#define LCD_BL          GPIO_NUM_45
```

## Usage

### 1. Initialize LVGL

```c
#include "lvgl.h"
#include "ST7789.h"
#include "uSPI.h"

// Initialize LVGL library
lv_init();
```

### 2. Initialize SPI Bus

```c
// Initialize uFlake SPI bus
uflake_result_t result = uspi_bus_init(
    USPI_HOST_SPI2,     // SPI host
    GPIO_NUM_35,        // MOSI
    GPIO_NUM_NC,        // MISO (not used for display)
    GPIO_NUM_36,        // SCLK
    USPI_MAX_TRANSFER_SIZE
);
```

### 3. Configure and Initialize Display

```c
st7789_lvgl_t display = {
    .cs_pin = GPIO_NUM_34,
    .reset_pin = GPIO_NUM_38,
    .dc_pin = GPIO_NUM_37,
    .bl_pin = GPIO_NUM_45,
    .width = 240,
    .height = 320,
    .host = USPI_HOST_SPI2,
    .frequency = USPI_FREQ_40MHZ,
    .orientation = 0  // 0=portrait, 1=portrait inverted, 2=landscape, 3=landscape inverted
};

esp_err_t ret = st7789_init(&display);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Display init failed");
}
```

### 4. Create LVGL Tasks

```c
// LVGL tick task (required)
void lvgl_tick_task(void *arg) {
    while (1) {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// LVGL handler task (required)
void lvgl_handler_task(void *arg) {
    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(time_till_next > 0 ? time_till_next : 5));
    }
}

// Start tasks
xTaskCreate(lvgl_tick_task, "lvgl_tick", 2048, NULL, 5, NULL);
xTaskCreate(lvgl_handler_task, "lvgl_handler", 4096, NULL, 5, NULL);
```

### 5. Create UI

```c
// Create a simple label
lv_obj_t *label = lv_label_create(lv_scr_act());
lv_label_set_text(label, "Hello uFlake!");
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
```

## API Reference

### Initialization Functions

#### `st7789_init()`
```c
esp_err_t st7789_init(st7789_lvgl_t *dev);
```
Initialize the ST7789 display with the given configuration.

**Parameters:**
- `dev`: Pointer to display configuration structure

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_*` on error

#### `st7789_deinit()`
```c
esp_err_t st7789_deinit(st7789_lvgl_t *dev);
```
Deinitialize the display and free resources.

### Control Functions

#### `st7789_backlight()`
```c
esp_err_t st7789_backlight(st7789_lvgl_t *dev, bool on);
```
Control display backlight.

**Parameters:**
- `dev`: Pointer to display structure
- `on`: `true` to turn on, `false` to turn off

#### `st7789_set_orientation()`
```c
esp_err_t st7789_set_orientation(st7789_lvgl_t *dev, uint8_t orientation);
```
Set display orientation.

**Parameters:**
- `dev`: Pointer to display structure
- `orientation`: 
  - `0` = Portrait
  - `1` = Portrait inverted
  - `2` = Landscape
  - `3` = Landscape inverted

### Low-Level Functions

#### `st7789_send_cmd()`
```c
void st7789_send_cmd(st7789_lvgl_t *dev, uint8_t cmd);
```
Send a command to the display.

#### `st7789_send_data()`
```c
void st7789_send_data(st7789_lvgl_t *dev, const void *data, uint16_t length);
```
Send data to the display.

#### `st7789_send_color()`
```c
void st7789_send_color(st7789_lvgl_t *dev, const void *data, size_t length);
```
Send color/pixel data using DMA for efficient transfer.

### LVGL Callback

#### `st7789_flush()`
```c
void st7789_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
```
LVGL v9 flush callback - automatically called by LVGL to render graphics.

## Configuration Structure

```c
typedef struct {
    gpio_num_t cs_pin;          // Chip select pin
    gpio_num_t reset_pin;       // Reset pin
    gpio_num_t dc_pin;          // Data/Command pin
    gpio_num_t bl_pin;          // Backlight pin (GPIO_NUM_NC if not used)
    
    uint16_t width;             // Display width in pixels
    uint16_t height;            // Display height in pixels
    uint8_t orientation;        // Display orientation (0-3)
    
    spi_device_handle_t spi;    // SPI device handle (set by init)
    spi_host_device_t host;     // SPI host (USPI_HOST_SPI2 or USPI_HOST_SPI3)
    uint32_t frequency;         // SPI clock frequency (e.g., USPI_FREQ_40MHZ)
    bool dc_level;              // Internal DC level state
    
    // LVGL v9 structures (initialized by st7789_init)
    lv_display_t *disp;         // LVGL display object
    lv_color_t *buf1;           // Display buffer 1
    lv_color_t *buf2;           // Display buffer 2
    size_t buffer_size;         // Buffer size in pixels
} st7789_lvgl_t;
```

## Memory Usage

- **LVGL Buffers**: Allocated in DMA-capable memory
- **Buffer Size**: `width × 40 lines × 2 bytes` per buffer (double buffered)
- **Example** (240×320): ~19.2KB per buffer, ~38.4KB total

## Performance

- **SPI Speed**: Up to 80MHz (typically 40MHz for stability)
- **DMA**: Enabled for large pixel transfers
- **Refresh Rate**: Depends on content, typically 30-60 FPS for simple UIs

## Troubleshooting

### Display shows nothing
- Check power supply (3.3V)
- Verify all pin connections
- Ensure SPI bus is initialized before display
- Check backlight is on

### Colors are inverted
- Adjust `ST7789_INVERT_COLORS` in sdkconfig or use `CONFIG_LV_INVERT_COLORS`

### Display is rotated incorrectly
- Use `st7789_set_orientation()` to adjust

### LVGL UI not updating
- Ensure both LVGL tasks are running
- Check task priorities and stack sizes

## Example

See [ST7789_example.c](ST7789_example.c) for a complete working example.

## Dependencies

- ESP-IDF v5.0+
- LVGL v9.0+
- uFlake HAL (uSPI module)

## License

Part of the uFlake project.
