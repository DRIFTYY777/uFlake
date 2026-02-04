# Boot Screen Quick Start Guide

## 🚀 5-Minute Integration

### Step 1: Add to uFlakeCore.c

```c
// At top of file
#include "uBootScreen.h"

// In uflake_core_init(), after config_and_init_display():
uboot_screen_start(&display);

// Continue with rest of init - animation runs in background
config_and_init_nrf24();
config_and_init_sd_card();
uGui_init(&display);

// Wait for animation to complete
while (uboot_screen_is_running()) {
    vTaskDelay(pdMS_TO_TICKS(100));
}

register_builtin_apps();
```

### Step 2: Build

```bash
idf.py build flash monitor
```

### Step 3: Enjoy! 🎉

Your device will now show a beautiful plasma animation during boot.

---

## 📝 Complete Integration Example

### Full Code for uFlakeCore.c

```c
#include "uFlakeCore.h"
#include "kernel.h"
#include "timer_manager.h"
#include "memory_manager.h"
#include "process_manager.h"
#include "synchronization.h"
#include "uFlakeAppReg.h"
#include "uGui.h"
#include "uSPI.h"
#include "ui2c.h"
#include "unvs.h"
#include "sdCard.h"
#include "nrf24_radio.h"
#include "ST7789.h"
#include "uBootScreen.h"  // ⭐ ADD THIS
#include "esp_log.h"

static const char *TAG = "uFlakeCore";

// Your existing display and other configs...
static st7789_driver_t display = {
    .pin_cs = GPIO_NUM_10,
    .pin_reset = GPIO_NUM_14,
    .pin_dc = GPIO_NUM_39,
    .display_width = 240,
    .display_height = 240,
    .orientation = 0,
    .spi_host = USPI_HOST_SPI3,
    .spi_speed = 80000000,
    .buffer_size = 240 * 20,
};

void config_and_init_display(void)
{
    ESP_LOGI(TAG, "Configuring ST7789 display...");
    if (!ST7789_init(&display)) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    ESP_LOGI(TAG, "Display initialized successfully");
}

void uflake_core_init(void)
{
    // Initialize kernel
    uflake_kernel_init();
    uflake_kernel_start();

    // Initialize subsystems
    unvs_init();
    i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ);
    uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 32768);
    uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096);

    // Initialize display
    config_and_init_display();

    // ⭐⭐⭐ START BOOT ANIMATION ⭐⭐⭐
    ESP_LOGI(TAG, "Starting boot screen animation");
    esp_err_t boot_ret = uboot_screen_start(&display);
    if (boot_ret != ESP_OK) {
        ESP_LOGW(TAG, "Boot screen failed, continuing without animation");
    }

    // Continue initialization (animation runs in parallel)
    ESP_LOGI(TAG, "Initializing peripherals...");
    config_and_init_nrf24();
    config_and_init_sd_card();
    
    ESP_LOGI(TAG, "Initializing GUI...");
    uGui_init(&display);

    // Wait for boot screen to complete
    ESP_LOGI(TAG, "Waiting for boot animation...");
    int timeout = 40; // 4 seconds max
    while (uboot_screen_is_running() && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (uboot_screen_is_running()) {
        uboot_screen_stop();
    }

    // Load applications
    ESP_LOGI(TAG, "Loading applications...");
    register_builtin_apps();

    ESP_LOGI(TAG, "✨ uFlake Core initialized with boot screen! ✨");
}
```

---

## 🎨 Customization Options

### Change Animation Duration

In `uBootScreen.h`:
```c
#define BOOT_SCREEN_DURATION_FRAMES 80   // 2 sec instead of 3
```

### Change Frame Rate

```c
#define BOOT_SCREEN_FPS 30   // 30 FPS instead of 40
```

### Change Text

In `uBootScreen.c`, find line ~162:
```c
render_text_overlay(buffer, buffer_y, frame, "YOUR TEXT", 
                    text_x, text_y, width, height);
```

### Change Colors

In `render_plasma_line()` function:
```c
// Original: Blue-Purple-Red
uint8_t color_r = plasma_r;
uint8_t color_g = (plasma_r >> 1) + (plasma_b >> 1);
uint8_t color_b = plasma_b;

// Green theme:
uint8_t color_r = plasma_r >> 2;
uint8_t color_g = plasma_r;
uint8_t color_b = plasma_b >> 2;

// Cyan theme:
uint8_t color_r = 0;
uint8_t color_g = plasma_r;
uint8_t color_b = plasma_b;
```

---

## ⚡ Performance Tuning

### For Faster Boot
```c
#define BOOT_SCREEN_DURATION_FRAMES 60   // 1.5 seconds
#define BOOT_SCREEN_FPS 40
```

### For Smoother Animation (uses more CPU)
```c
#define BOOT_SCREEN_FPS 60
#define BOOT_SCREEN_STRIP_HEIGHT 10  // Smaller strips
```

### For Less Memory Usage
```c
#define BOOT_SCREEN_STRIP_HEIGHT 10   // 240×10×2×2 = 9.6KB
```

---

## 🐛 Troubleshooting

### Animation doesn't show
✅ Make sure display is initialized BEFORE calling `uboot_screen_start()`
✅ Check that `config_and_init_display()` succeeds
✅ Verify SPI3 bus is initialized with DMA

### Display shows garbage
✅ Ensure buffer size in display driver is >= 240×20
✅ Check SPI DMA is enabled (`SPI_DMA_CH_AUTO`)
✅ Verify GPIO pins are correct

### Animation is choppy
✅ Reduce FPS to 30
✅ Increase strip height to 40
✅ Make sure task is on Core 1

### Build errors
```bash
# Clean and rebuild
idf.py fullclean
idf.py build
```

---

## 📊 Expected Results

✅ **Boot time**: Add ~3 seconds (animation duration)  
✅ **Memory usage**: ~25KB  
✅ **CPU impact**: Core 1 only, Core 0 remains free  
✅ **Visual effect**: Smooth 40 FPS plasma animation  
✅ **Display**: Colorful flowing waves with optional text  

---

## 🔍 Verification

Run and look for these log messages:

```
I (123) uFlakeCore: Starting boot screen animation
I (124) uBootScreen: Creating boot screen task
I (125) uBootScreen: Boot screen started
... (animation runs for ~3 seconds) ...
I (3456) uBootScreen: Boot screen completed
I (3457) uFlakeCore: ✨ uFlake Core initialized with boot screen! ✨
```

---

## 📚 More Information

- [README.md](README.md) - Full documentation
- [ANIMATION_GUIDE.md](ANIMATION_GUIDE.md) - Visual guide to the animation
- [EXAMPLES.md](EXAMPLES.md) - Usage examples
- [integration_example.c](integration_example.c) - Complete code examples

---

## 🎯 That's It!

You now have a professional boot animation on your uFlake device! 🎉

The animation uses the same plasma effect algorithm as your original `boot_screen.c`, 
but is optimized for CMake/ESP-IDF and integrates seamlessly with your existing 
uFlake architecture.

Enjoy your beautiful boot screen! ✨
