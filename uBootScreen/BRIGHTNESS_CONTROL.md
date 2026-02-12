# Boot Screen Brightness Control

## Overview
The boot screen now features smooth brightness fade-in and fade-out effects synchronized with the animation, creating a professional boot experience.

## Features

### Automatic Brightness Control
The brightness is automatically controlled during the boot animation:

- **Fade In** (Frames 0-30, ~0.5 seconds)
  - Smooth cubic ease-out curve from 0% to 100%
  - Creates a gentle appearance of the boot animation

- **Full Brightness** (Frames 30-90, ~1.0 seconds)
  - Maintains 100% brightness during main animation
  - Ensures logo and effects are clearly visible

- **Fade Out** (Frames 90-120, ~0.5 seconds)  
  - Smooth cubic ease-in curve from 100% to 0%
  - Elegant transition to the main application

### Manual Control
You can also manually control the brightness:

```c
#include "uBootScreen.h"

// Set brightness to 50%
uboot_screen_set_brightness(50.0f);

// Set full brightness
uboot_screen_set_brightness(100.0f);

// Turn off backlight
uboot_screen_set_brightness(0.0f);
```

## Technical Details

### Hardware Configuration
- **Backlight Pin**: GPIO 3
- **PWM Frequency**: 1 kHz
- **Duty Cycle Range**: 0-100%

### Brightness Curves
The implementation uses cubic easing functions for smooth transitions:

**Fade In (Ease-Out Cubic)**:
```
brightness = max * (1 - (1-t)³)
```

**Fade Out (Ease-In Cubic)**:
```
brightness = max * (1 - t³)
```

Where `t` is normalized time (0.0 to 1.0).

## Implementation Details

The brightness control is integrated into the boot screen task:
1. Each frame, `update_brightness()` calculates the target brightness
2. PWM duty cycle is updated via `ugpio_pwm_set_duty()`
3. After animation completes, brightness is set to 100%

This ensures the display is fully visible when the main application takes over.

## Customization

You can adjust the timing by modifying the constants in `uBootScreen.c`:

```c
#define BRIGHTNESS_FADE_IN_FRAMES 30    // Fade in duration
#define BRIGHTNESS_FADE_OUT_START 90    // When to start fade out
#define BRIGHTNESS_MAX 100.0f           // Maximum brightness %
```

## API Reference

### uboot_screen_set_brightness()
```c
esp_err_t uboot_screen_set_brightness(float brightness);
```

Manually set the backlight brightness.

**Parameters**:
- `brightness`: Brightness level (0.0 - 100.0%)

**Returns**:
- `ESP_OK`: Success
- Error code on failure

**Note**: Manual brightness changes during boot animation will be overridden by the automatic control on the next frame update.
