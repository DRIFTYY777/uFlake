# uBootScreen - Plasma Animation Boot Screen

A beautiful animated boot screen for uFlake with colorful plasma effects and text overlay, optimized for ESP32-S3 with SPI DMA.

## Features

- ✨ **Colorful plasma animation** using procedural sine wave generation
- 📝 **Animated text overlay** with fade in/out effects
- 🚀 **SPI DMA optimized** for smooth 40 FPS rendering
- 💾 **Memory efficient** strip-based rendering (20 lines at a time)
- 🎨 **Dithering** for smooth color gradients on RGB565 displays
- 🔄 **Double buffering** for flicker-free animation
- 📦 **Two implementations**: Direct display and LVGL-integrated

## Animation Effect

The boot screen features a mesmerizing plasma effect created using:
- Multiple sine wave layers with different frequencies
- Smooth color interpolation (blue → purple → red spectrum)
- Fade in/out transitions
- Text overlay that fades in and out

Based on the classic demo scene plasma effect, optimized for embedded displays.

## Usage

### Option 1: Direct Display Rendering (Recommended for boot)

```c
#include "uBootScreen.h"
#include "ST7789.h"

// In your initialization code
st7789_driver_t display;
// ... initialize display ...

// Start boot screen
uboot_screen_start(&display);

// Do your system initialization while animation plays
init_wifi();
init_filesystem();
load_config();

// Animation will auto-complete after 3 seconds
// Or manually stop it:
uboot_screen_stop();
```

### Option 2: LVGL Canvas Integration

```c
#include "uBootScreenLVGL.h"
#include "lvgl.h"

// After LVGL is initialized
uboot_screen_lvgl_init();

// Render frames in your timer or task
for (int frame = 0; frame < 120; frame++) {
    uboot_screen_lvgl_render_frame(frame);
    lv_task_handler();
    vTaskDelay(pdMS_TO_TICKS(25)); // 40 FPS
}

// Cleanup
uboot_screen_lvgl_deinit();
```

### Integration with uFlakeCore

Add to [uFlakeCore.c](../uFlakeCore/uFlakeCore.c) initialization:

```c
void uflake_core_init(void)
{
    // Kernel init
    uflake_kernel_init();
    uflake_kernel_start();

    // Initialize busses
    i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ);
    uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 32768);

    // Initialize display
    config_and_init_display();

    // ⭐ START BOOT SCREEN
    uboot_screen_start(&display);

    // Continue initialization (happens in parallel with animation)
    config_and_init_nrf24();
    config_and_init_sd_card();
    
    // Initialize LVGL
    uGui_init(&display);

    // Wait for boot screen to finish
    while (uboot_screen_is_running()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Register apps and show main UI
    register_builtin_apps();
}
```

## Configuration

Edit [uBootScreen.h](uBootScreen.h) to customize:

```c
#define BOOT_SCREEN_WIDTH 240              // Display width
#define BOOT_SCREEN_HEIGHT 240             // Display height
#define BOOT_SCREEN_STRIP_HEIGHT 20        // Lines per DMA transfer
#define BOOT_SCREEN_FPS 40                 // Animation frame rate
#define BOOT_SCREEN_DURATION_FRAMES 120    // Total frames (3 sec @ 40 FPS)
#define BOOT_SCREEN_TASK_PRIORITY 5        // FreeRTOS task priority
#define BOOT_SCREEN_TASK_CORE 1            // CPU core (0 or 1)
```

## Performance

- **Frame rate**: 40 FPS smooth animation
- **Memory**: ~10KB (20 lines × 240 pixels × 2 bytes × 2 buffers)
- **DMA transfers**: Chunked in 9.6KB transfers (well under 32KB limit)
- **CPU usage**: Runs on Core 1, minimal impact on Core 0 tasks

## How It Works

### Plasma Algorithm

```c
// For each pixel (x, y) at frame f:
plasma_r = sin(4x + 2y + 4f) + sin(sin(2(2y+f) + x) + 7f)
plasma_b = sin(x + 4y + 2f) + sin(sin(2(2x+f) + y) + 2f)
color = RGB(plasma_r, (plasma_r+plasma_b)/2, plasma_b)
```

### Rendering Pipeline

1. **Strip-based rendering**: Renders 20 lines at a time to minimize memory
2. **Dithering**: Adds noise to smooth color gradients on RGB565
3. **DMA transfer**: Sends each strip via SPI DMA
4. **Double buffering**: Renders next strip while displaying current

### Performance Optimization

- **Pre-computed sine table**: Fast lookups instead of math.h sin()
- **Fixed-point arithmetic**: No floating point operations
- **DMA-capable buffers**: Allocated with `UFLAKE_MEM_DMA` flag
- **Async transfers**: Queue SPI transactions for pipelining

## Customization

### Change Text

Edit [uBootScreen.c](uBootScreen.c):

```c
// Line ~162
render_text_overlay(buffer, buffer_y, frame, "Your Text Here", 
                    BOOT_SCREEN_WIDTH / 2 - 80, 100,
                    BOOT_SCREEN_WIDTH, strip_height);
```

### Change Colors

Modify plasma calculation in [uBootScreen.c](uBootScreen.c):

```c
// Line ~149-155 - adjust the color mixing
uint8_t color_r = plasma_r;
uint8_t color_g = (plasma_r >> 1) + (plasma_b >> 1);  // Mix red and blue
uint8_t color_b = plasma_b;

// Try different mixes:
// Green-heavy: color_g = plasma_r;
// Purple-heavy: color_r = color_b = plasma_r;
```

### Change Animation Speed

Adjust frame calculation multipliers:

```c
// Faster: multiply frame values
const int frame_2 = frame << 3;  // Was << 2, now << 3 (2x faster)

// Slower: use smaller multipliers
const int frame_1 = frame;       // Was << 1
```

## Troubleshooting

**Animation is choppy:**
- Reduce `BOOT_SCREEN_FPS` to 30
- Increase `BOOT_SCREEN_STRIP_HEIGHT` to 40 (fewer DMA transfers)

**Display shows garbage:**
- Ensure display is initialized before calling `uboot_screen_start()`
- Check that SPI DMA is enabled (`SPI_DMA_CH_AUTO`)
- Verify buffers are DMA-capable memory

**Text doesn't show:**
- LVGL version: Ensure LVGL is initialized first
- Direct version: Implement `render_text_overlay()` function

**Too much memory:**
- Reduce `BOOT_SCREEN_STRIP_HEIGHT` to 10
- Use single buffering (modify code to reuse same buffer)

## Files

- `uBootScreen.c/h` - Direct display rendering (standalone)
- `uBootScreenLVGL.c/h` - LVGL canvas integration
- `CMakeLists.txt` - Build configuration
- `README.md` - This file

## Credits

Plasma effect inspired by classic demoscene productions. Implementation optimized for ESP32-S3 with modern DMA techniques.

## License

Part of the uFlake project. See main project license.
