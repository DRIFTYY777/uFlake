# uFlake Apps

This folder contains internal apps that are linked into the firmware.

## **IMPORTANT: Internal Apps Cannot Be Auto-Discovered!**

Unlike external apps on SD card, **internal apps compiled into firmware must be manually registered** in your main.cpp. There is no way for the system to automatically find them.

## App Structure

Each app should have its own folder with:
- `app_main.c` - Main app code with `<appname>_app_main()` function
- `CMakeLists.txt` - Build configuration

## App Entry Point

Every app must implement a unique entry function:

```c
void myapp_app_main(void) {
    // Your app code here
    // Run loop until user exits with Right+Back buttons
}
```

**Note:** Do NOT name it `app_main()` - that conflicts with ESP-IDF's main!

## How to Register Apps

In your main.cpp, register each app manually:

```c
#include "appLoader.h"

// Declare app entry points
extern void launcher_app_main(void);
extern void counter_app_main(void);

void setup_apps(void) {
    // Initialize app loader
    app_loader_init();
    
    // Register launcher
    app_manifest_t launcher_manifest = {
        .name = "Launcher",
        .version = "1.0.0",
        .author = "Your Name",
        .description = "Home screen",
        .type = APP_TYPE_LAUNCHER,
        .stack_size = 8192,
        .priority = 10,
        .requires_gui = true
    };
    
    uint32_t launcher_id = app_loader_register_internal(
        &launcher_manifest,
        launcher_app_main,  // Function pointer
        true                // is_launcher
    );
    
    // Register other apps...
    
    // Launch launcher
    app_loader_launch(launcher_id);
}
```

See `HOWTO_REGISTER_APPS.c` for complete example.

## App Lifecycle

1. **Registration**: Manually register in main.cpp using `app_loader_register_internal()`
2. **Launch**: Launcher displays registered apps, user selects one
3. **Execution**: App loader creates a uFlake kernel task running your app function
4. **Exit**: User holds Right+Back for 2 seconds to force exit
5. **Return**: App loader returns to launcher automatically

## Important Notes

- Apps are completely passive - no API calls needed
- App loader manages all lifecycle
- Use uFlake kernel tasks, not FreeRTOS directly
- Keep app functions responsive
- Don't block for long periods

## Example Apps

- `launcher/` - Home screen / app menu (special app)
- `counter_app/` - Simple counter demonstration
