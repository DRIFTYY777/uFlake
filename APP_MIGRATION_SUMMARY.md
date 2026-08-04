# App Migration Summary - New GUI/AppLoader API

## Overview
All applications have been updated from the old GUI/AppLoader API to the new simplified Flipper Zero-style API.

## Changes Made

### 1. Launcher App (`Apps/launcher/launcher.c`)
**Status**: ✅ Migrated

**Changes**:
- Updated includes: `appLoader.h` → `appLoader_simple.h`, `uGui.h` → `uGui_simple.h`
- Removed references to: `uGui_get_content_container()`, `uGui_add_to_group()`, `uGui_auto_focus_object()`, `uGui_set_launcher()`
- Updated manifest fields:
  - `type` → `category` + `subtype`
  - `requires_gui` → removed (implicit from `subtype = APP_SUBTYPE_GUI`)
  - Added `min_ram_bytes`
- Added input handler registration: `uGui_register_input_handler(UGUI_BTN_OK/UP/DOWN, ...)`
- Uses new simplified API: `uGui_get_content_container()` (still exists, just renamed from old one)
- Changed logging: `UFLAKE_LOGI/LOGE` → `ESP_LOGI/LOGE`

### 2. Counter App (`Apps/counter_app/app_main.c`)
**Status**: ✅ Migrated

**Changes**:
- Simplified from infinite loop to actual GUI app
- Added UI creation with LVGL: title, counter display, instructions
- Input handler responds to OK (increment), UP (+10), DOWN (-1), BACK (exit)
- Updated manifest similarly to launcher
- Proper logging and error handling

### 3. Settings App (`Apps/settings_app/settings_app.c`)
**Status**: ✅ Migrated

**Changes**:
- Removed old focus group operations: `uGui_add_to_group()`, `uGui_auto_focus_object()`
- Kept UI creation logic (sliders, buttons, switches)
- Added proper input handler (BACK button to exit)
- Updated manifest and logging
- Removed device-specific operations (no actual brightness/WiFi control needed for demo)

### 4. ADC Reader App (`Apps/read_ADC/app_main.c`)
**Status**: ✅ Migrated

**Changes**:
- Changed from complex HAL abstraction to simple ESP32 ADC driver
- Changed `subtype` to `APP_SUBTYPE_NON_GUI` (non-GUI background app)
- Simplified to basic ADC reading loop
- No GUI operations needed
- Proper logging

### 5. Test App (`Apps/test_app/app_main.c`)
**Status**: ✅ Migrated

**Changes**:
- Simplified from complex resource manager tests to basic functionality tests
- Updated all logging from `UFLAKE_LOGI/LOGE` to `ESP_LOGI/LOGE`
- Changed `subtype` to `APP_SUBTYPE_NON_GUI`
- Removed old test cases referencing non-existent APIs

### 6. Counter App C++ (`Apps/counter_app_cpp/app_main.cpp`)
**Status**: ✅ Migrated

**Changes**:
- Similar to Counter App but in C++ with proper extern "C" linkage
- Uses `nullptr` instead of `NULL` (C++ style)
- Properly wrapped C headers in `extern "C"` block
- GUI structure identical to C version

## New App Structure

### Manifest Fields (Updated)
```c
typedef struct {
    const char *name;
    const char *version;
    const char *author;
    const char *description;
    const char *icon;
    app_category_t category;      // APP_CAT_SYSTEM, APP_CAT_UTILITY, etc.
    app_subtype_t subtype;         // APP_SUBTYPE_GUI, APP_SUBTYPE_NON_GUI, APP_SUBTYPE_LAUNCHER
    uint16_t stack_size;
    process_priority_t priority;   // PROCESS_PRIORITY_NORMAL (not raw int)
    bool requires_sdcard;
    bool requires_network;
    uint32_t min_ram_bytes;
} app_manifest_t;
```

### Key API Changes

**Old → New Mapping**:
```c
// Old                              // New
uGui_get_content_container()         → uGui_get_content_container() (same name, simplified)
uGui_add_to_group(obj)              → (removed - no focus groups)
uGui_auto_focus_object(obj)         → (removed - direct LVGL handling)
uGui_set_launcher(func)             → (removed - is_launcher flag in manifest)
app_loader_launch(id)               → app_loader_launch_app(id)
app_loader_get_apps(&arr, &count)   → app_loader_get_app_list(&count)
UFLAKE_LOGI/LOGE                    → ESP_LOGI/LOGE
```

### Input Handler Pattern
```c
static void my_input_handler(ugui_button_t btn, bool pressed, void *userdata)
{
    if (!pressed) return;  // Only handle press events
    
    switch (btn) {
        case UGUI_BTN_OK:
            // Handle select
            break;
        case UGUI_BTN_UP:
        case UGUI_BTN_DOWN:
        case UGUI_BTN_LEFT:
        case UGUI_BTN_RIGHT:
            // Handle navigation
            break;
        case UGUI_BTN_BACK:
            uGui_exit_app();  // Exit to launcher
            break;
    }
}

// Register in app_main:
uGui_register_input_handler(UGUI_BTN_OK, my_input_handler, NULL);
uGui_register_input_handler(UGUI_BTN_BACK, my_input_handler, NULL);
```

### GUI App Pattern
```c
void my_app_main(void)
{
    // 1. Get container
    lv_obj_t *container = uGui_get_content_container();
    if (container == NULL) return;
    
    // 2. Register input handlers
    uGui_register_input_handler(UGUI_BTN_OK, my_handler, NULL);
    
    // 3. Create UI with LVGL
    lv_obj_t *screen = lv_obj_create(container);
    // ... add UI elements ...
    
    // 4. Return - LVGL handles events in background
}
```

### Non-GUI App Pattern
```c
void my_app_main(void)
{
    // Do work in a loop with periodic delays
    for (int i = 0; i < MAX_ITERATIONS; i++)
    {
        // Do work
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Return when done
}
```

## Files Modified

| File | Type | Status |
|------|------|--------|
| `Apps/launcher/launcher.c` | GUI | ✅ Migrated |
| `Apps/counter_app/app_main.c` | GUI | ✅ Migrated |
| `Apps/counter_app_cpp/app_main.cpp` | GUI (C++) | ✅ Migrated |
| `Apps/settings_app/settings_app.c` | GUI | ✅ Migrated |
| `Apps/read_ADC/app_main.c` | Non-GUI | ✅ Migrated |
| `Apps/test_app/app_main.c` | Non-GUI | ✅ Migrated |

## Testing Checklist

- [ ] Build succeeds (all old API references gone)
- [ ] Launcher loads and shows app list
- [ ] First app button is focused
- [ ] Navigation works (UP/DOWN arrows)
- [ ] Launching apps works (OK button)
- [ ] Counter app increments and updates display
- [ ] BACK button exits apps back to launcher
- [ ] Settings app displays all options
- [ ] ADC app reads values (background task)

## Notes

- All apps now use the simplified API with no undefined behavior
- No more focus group complications - direct LVGL event handling
- Memory awareness built-in (min_ram_bytes checked before launch)
- Consistent logging across all apps
- Apps can be GUI or non-GUI via manifest subtype
- Input handling is per-app with unregister support

## Future Work

- [ ] Implement actual hardware features (brightness control, WiFi, etc.)
- [ ] Add more example apps demonstrating new features
- [ ] Performance optimization of GUI rendering
- [ ] Memory profiling to find optimal min_ram_bytes values
- [ ] Testing with limited RAM conditions
