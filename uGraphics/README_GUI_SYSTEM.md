# uFlake GUI System Documentation

## Overview

The uFlake GUI system is a comprehensive, crash-free GUI framework built on LVGL 9 for ESP32-based devices. It solves critical problems with traditional LVGL app development:

✅ **Automatic Focus Management** - No manual `lv_group_focus()` calls  
✅ **Crash-Free Object Deletion** - Safe `lv_obj_del()` with automatic focus handling  
✅ **Apps Without While Loops** - LVGL handles everything, apps just create UI  
✅ **Flipper Zero-Like Architecture** - Transparent UI, notification bar, app windows  
✅ **Theme System** - Background images (JPEG from SD), color themes, transparent overlays  
✅ **Widget Library** - Ready-to-use dialogs, loading indicators, lists  
✅ **Keyboard Navigation** - Button-based navigation with automatic routing  

## Architecture

The GUI system consists of 7 modules:

### 1. **Focus Manager** (`uGui_focus.h/c`)
- **Problem Solved:** Manual focus causes crashes when deleting objects
- **Solution:** Automatic layer-based focus with safe deletion
- **Usage:** Register objects, they auto-focus based on layer priority

### 2. **Notification Bar** (`uGui_notification.h/c`)
- **Size:** 240x25 pixels at top of screen
- **Features:**
  - Battery status (%, charging icon)
  - WiFi/Bluetooth status
  - SD card status
  - Time (HH:MM)
  - App name display (fades after 3 seconds)
  - Loading indicator (Windows mobile style dots)
- **Transparency:** Configurable opacity, theme-aware

### 3. **App Window Manager** (`uGui_appwindow.h/c`)
- **Problem Solved:** Apps crash on exit, manual focus management required
- **Solution:** Safe container with automatic lifecycle
- **Features:**
  - Default: 240x215 pixels (below notification bar)
  - Fullscreen: 240x240 (hides notification bar)
  - Custom sizes supported
  - Automatic focus enter/exit
  - Safe cleanup on app exit

### 4. **Theme Manager** (`uGui_theme.h/c`)
- **Background:** Solid color OR JPEG from SD card
- **Themes:** Dark, Light, Blue (Flipper-like), customizable
- **Transparency:** All components support transparent backgrounds
- **Styling Helpers:** Auto-apply theme to buttons, labels, panels

### 5. **Widget Library** (`uGui_widgets.h/c`)
- **Dialogs:** OK, OK/Cancel, Yes/No
- **Loading:** Dots (Windows mobile), Spinner, Progress bar
- **Lists:** Scrollable item lists with callbacks
- **Helpers:** Themed buttons, labels, panels

### 6. **Navigation System** (`uGui_navigation.h/c`)
- **Buttons:** Up, Down, Left, Right, OK, Back, Menu
- **Routing:** Automatically sends input to focused layer
- **Focus Navigation:** Tab through focusable objects

### 7. **Types** (`uGui_types.h`)
- Common types, structs, callbacks for all modules

## Quick Start

### 1. Initialize GUI (Done in `uGui_init()`)

```c
void uGui_init(st7789_driver_t *drv) {
    // LVGL init
    // Focus manager init
    // Theme manager init
    // Notification bar init
    // App window manager init
    // Navigation system init
    
    // Apply default theme (blue)
    ugui_theme_apply_blue();
}
```

### 2. Create an App (NO WHILE LOOP NEEDED!)

```c
#include "uGui.h"
#include "appLoader.h"

void my_app_main(void) {
    UFLAKE_LOGI("MyApp", "App started");
    
    // Create app window (automatic focus, safe cleanup)
    ugui_appwin_config_t config = {
        .app_name = "My App",
        .width = 0,  // Default size
        .height = 0,
        .flags = UGUI_APPWIN_FLAG_NONE,
        .bg_color = lv_color_hex(0x000000),
        .bg_opacity = 255
    };
    
    ugui_appwin_t *window = ugui_appwindow_create(&config, my_app_id);
    if (!window) {
        UFLAKE_LOGE("MyApp", "Failed to create window");
        return;
    }
    
    // Get content container
    lv_obj_t *content = ugui_appwindow_get_content(window);
    
    // Add UI elements
    lv_obj_t *label = ugui_label_create(content, "Hello uFlake!");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_obj_t *btn = ugui_button_create(content, "Click Me", 100, 40);
    lv_obj_center(btn);
    
    // Register button for focus navigation
    ugui_appwindow_add_focusable(window, btn);
    
    // Add button event
    lv_obj_add_event_cb(btn, button_clicked, LV_EVENT_CLICKED, NULL);
    
    // NO WHILE LOOP! App exits, window stays until app loader closes it
    // When app exits, ugui_appwindow_destroy() safely cleans everything
    
    UFLAKE_LOGI("MyApp", "UI created, app exiting (LVGL handles everything)");
}
```

### 3. Safe App Exit

```c
// When app loader terminates app, it calls:
ugui_appwindow_t *window = ugui_appwindow_get_by_app_id(app_id);
if (window) {
    ugui_appwindow_destroy(window);  // SAFE - no crashes!
}
```

## Key Features Explained

### No While Loop Required

Traditional LVGL apps need a `while(1)` loop. **Not anymore!**

```c
// ❌ OLD WAY (manual loop, watchdog concerns)
void old_app_main(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_scr_load(screen);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        // App never exits, watchdog must be fed
    }
}

// ✅ NEW WAY (LVGL handles loop, app just creates UI)
void new_app_main(void) {
    ugui_appwin_t *window = ugui_appwindow_create(&config, app_id);
    lv_obj_t *content = ugui_appwindow_get_content(window);
    
    // Create UI...
    
    // Exit! LVGL keeps UI alive, no crash on exit
}
```

### Automatic Focus (No Crashes!)

The focus manager tracks all LVGL objects and handles focus **automatically**.

**Problem:** Deleting a focused object crashes LVGL  
**Solution:** Focus manager safely transfers focus before deletion

```c
// ❌ DANGEROUS (can crash)
lv_obj_t *btn = lv_btn_create(parent);
lv_group_add_obj(group, btn);
// ... later ...
lv_obj_del(btn);  // CRASH if btn has focus!

// ✅ SAFE (automatic focus handling)
ugui_appwin_t *window = ugui_appwindow_create(&config, app_id);
lv_obj_t *btn = ugui_button_create(content, "Button", 100, 40);
ugui_appwindow_add_focusable(window, btn);
// ... later ...
ugui_appwindow_destroy(window);  // Safe cleanup, no crash!
```

### Notification Bar Usage

```c
// Update system status
ugui_notification_update_battery(75, true);  // 75%, charging
ugui_notification_update_wifi(true);         // Connected
ugui_notification_update_time(14, 30);       // 14:30

// Show app name (auto-fades after 3 seconds)
ugui_notification_show_app_name("Calculator", 3000);

// Show loading indicator
ugui_notification_show_loading(true);
// ... do work ...
ugui_notification_show_loading(false);

// Hide notification bar (for fullscreen apps)
ugui_notification_hide();
```

### Theme Customization

```c
// Pre-defined themes
ugui_theme_apply_dark();
ugui_theme_apply_light();
ugui_theme_apply_blue();  // Flipper-like

// Custom theme
ugui_theme_t my_theme = {
    .primary = lv_color_hex(0x00FF00),  // Green
    .secondary = lv_color_hex(0xFF0000), // Red
    .background = lv_color_hex(0x000000),
    .text = lv_color_hex(0xFFFFFF),
    .notification_bg = lv_color_hex(0x001100),
    .notification_fg = lv_color_hex(0x00FF00),
    .opacity = 200  // Semi-transparent
};
ugui_theme_set(&my_theme);

// Background wallpaper (JPEG from SD card)
ugui_theme_set_bg_image_sdcard("/sdcard/wallpaper.jpg");

// Or solid color
ugui_theme_set_bg_color(lv_color_hex(0x0000FF));
```

### Widget Library

```c
// Dialog boxes (modal, auto-focus)
ugui_dialog_ok("Title", "Message", callback, userdata);
ugui_dialog_ok_cancel("Save?", "Save changes?", callback, userdata);
ugui_dialog_yes_no("Delete?", "Delete file?", callback, userdata);

// Message box (auto-dismiss)
ugui_show_message("File saved!", 2000);

// Loading indicator
lv_obj_t *loading = ugui_show_loading("Loading...", UGUI_LOADING_DOTS);
// ... do work ...
ugui_hide_loading(loading);

// List widget
const char *items[] = {"Item 1", "Item 2", "Item 3"};
lv_obj_t *list = ugui_list_create(content, items, 3, list_callback, userdata);
```

### Keyboard Navigation

```c
// Button event routing (call from your button handler)
ugui_navigation_button_event(UGUI_NAV_UP, true);     // Button pressed
ugui_navigation_button_event(UGUI_NAV_DOWN, false);  // Button released
ugui_navigation_button_event(UGUI_NAV_OK, true);     // OK pressed
ugui_navigation_button_event(UGUI_NAV_BACK, true);   // Back pressed

// Navigation automatically routes to focused layer
// Up/Down: Navigate between focusable objects
// OK: Trigger button click
// Back: Close app/dialog
```

## Layer System

The GUI uses a **layer-based focus system** (like z-index in web):

| Layer | Priority | Purpose | Example |
|-------|----------|---------|---------|
| **BACKGROUND** | 0 | Wallpaper | Background image |
| **APP_WINDOW** | 1 | App UI | Your app content |
| **NOTIFICATION** | 2 | System status | Battery, time, icons |
| **DIALOG** | 3 | Modal dialogs | OK/Cancel dialogs |
| **SYSTEM** | 4 | System overlays | Loading indicators |

**Higher layers block input to lower layers.** For example:
- Dialog open → App window can't receive input
- Notification bar → Always visible, doesn't block app input

## Integration with App Loader

The app loader automatically manages app windows:

```c
// App loader launches app
app_lifecycle_launch(app, launcher_id, &current_app_id);

// App creates window
ugui_appwin_t *window = ugui_appwindow_create(&config, app_id);

// ... app runs, LVGL handles UI ...

// App exits (or user closes)
ugui_appwindow_t *window = ugui_appwindow_get_by_app_id(app_id);
ugui_appwindow_destroy(window);  // Safe cleanup!

// App loader resumes launcher
app_lifecycle_terminate(app, launcher_id, &current_app_id);
```

## Suggestions for Future Enhancements

1. **Image Support**: Integrate `imageCodec` to load JPEG wallpapers
2. **Animation Library**: Add slide-in/out transitions for app windows
3. **Virtual Keyboard**: On-screen keyboard for text input
4. **Gesture Support**: Swipe gestures for navigation
5. **Multi-Window**: Support multiple app windows (task switcher)
6. **Persistent Settings**: Save theme/wallpaper to NVS
7. **Custom Widgets**: Progress rings, graphs, charts
8. **Sound Effects**: Button clicks, notifications
9. **Haptic Feedback**: Vibration on button press
10. **ELF Loading**: Load external apps from SD card (.fap files)

## Comparison: Traditional vs uFlake GUI

| Feature | Traditional LVGL | uFlake GUI |
|---------|------------------|------------|
| **Focus Management** | Manual `lv_group_focus()` | ✅ Automatic |
| **Object Deletion** | Can crash if focused | ✅ Crash-free |
| **App Lifecycle** | Manual while(1) loop | ✅ LVGL handles |
| **Notification Bar** | Manual creation | ✅ Built-in |
| **Theme System** | Manual styling | ✅ Automatic |
| **Widgets** | Build yourself | ✅ Ready-to-use |
| **Navigation** | Manual routing | ✅ Automatic |
| **Wallpaper** | Manual setup | ✅ JPEG from SD |

## File Structure

```
uGraphics/
├── uGui.h                      # Main header (includes all subsystems)
├── uGui.c                      # Main initialization
├── CMakeLists.txt              # Build configuration
├── include/
│   ├── uGui_types.h            # Common types and definitions
│   ├── uGui_focus.h            # Focus manager (CRITICAL)
│   ├── uGui_notification.h     # Notification bar
│   ├── uGui_appwindow.h        # App window manager
│   ├── uGui_theme.h            # Theme and background
│   ├── uGui_widgets.h          # Widget library
│   └── uGui_navigation.h       # Keyboard navigation
├── src/
│   ├── uGui_focus.c            # Focus implementation
│   ├── uGui_notification.c     # Notification implementation
│   ├── uGui_appwindow.c        # App window implementation
│   ├── uGui_theme.c            # Theme implementation
│   ├── uGui_widgets.c          # Widget implementation
│   └── uGui_navigation.c       # Navigation implementation
└── README_GUI_SYSTEM.md        # This file
```

## License

Part of the uFlake OS project.

---

**Ready to build amazing, crash-free apps on ESP32! 🚀**

For questions or contributions, see the main uFlake README.
