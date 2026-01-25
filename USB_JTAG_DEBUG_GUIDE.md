# ESP32-S3 USB-JTAG Debugging Guide

## Hardware Connection

The ESP32-S3 has a **built-in USB-JTAG adapter** - no external debugger needed!

### Connection Steps:
1. **Connect USB cable** to the **USB port** on your ESP32-S3 board (usually marked as "USB" not "UART")
2. The board will appear as two devices:
   - COM port for serial communication
   - JTAG interface for debugging

## Configuration Applied

### 1. SDK Config (`sdkconfig.defaults.esp32s3`)
```ini
# USB Serial/JTAG enabled as primary console
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_USJ_ENABLE_USB_SERIAL_JTAG=y

# JTAG debugging enabled (not disabled)
# CONFIG_ESP_SYSTEM_DISABLE_JTAG is not set

# Application tracing support
CONFIG_APPTRACE_DEST_JTAG=y
CONFIG_APPTRACE_ENABLE=y
```

### 2. VS Code Debug Configuration (`.vscode/launch.json`)
Three debug configurations:
- **ESP-IDF: USB-JTAG Debug** - Full debug with reset
- **ESP-IDF: USB-JTAG Attach** - Attach to running program
- **ESP-IDF: Core Dump Debug** - Analyze crash dumps

## Using the Debugger

### Method 1: VS Code (Recommended)

1. **Build and Flash** your project:
   ```bash
   idf.py build flash
   ```

2. **Start OpenOCD** (automatically via VS Code):
   - Press `F5` or click "Run and Debug"
   - Select "ESP-IDF: USB-JTAG Debug"
   - OpenOCD will start automatically

3. **Debug Features**:
   - Set breakpoints by clicking left of line numbers
   - Step through code (F10 = step over, F11 = step into)
   - Inspect variables in the Variables panel
   - View call stack
   - Watch expressions

### Method 2: Command Line

1. **Start OpenOCD** in one terminal:
   ```powershell
   openocd -f board/esp32s3-builtin.cfg
   ```
   
   You should see:
   ```
   Info : Listening on port 3333 for gdb connections
   ```

2. **Start GDB** in another terminal:
   ```powershell
   xtensa-esp32s3-elf-gdb -x gdbinit build/uFlake.elf
   ```
   
   Then in GDB:
   ```gdb
   target remote :3333
   mon reset halt
   thb app_main
   c
   ```

## Monitoring Serial Output

### Via USB-JTAG Console:
```powershell
# Use ESP-IDF monitor
idf.py monitor

# Or use any serial terminal
# The COM port will be detected automatically
```

### View both debug and serial output:
- Serial output appears in the "Terminal" panel in VS Code
- Debug logs appear in the "Debug Console" panel

## Common Commands

### In VS Code Debug Console (when debugging):
```gdb
# Set breakpoint
break file.c:123

# Continue execution
continue

# Print variable
print my_variable

# Print structure
print *my_struct_ptr

# Step into function
step

# Step over function
next

# Backtrace (call stack)
bt

# Info threads
info threads

# Switch thread
thread 2
```

## Troubleshooting

### OpenOCD won't connect:
```powershell
# Check if device is detected
idf.py openocd

# If fails, try unplugging and replugging USB cable
# Make sure you're using the USB port, not UART port
```

### "Target not halted" error:
```gdb
# In GDB, manually halt:
mon reset halt
```

### Can't see COM port:
- Install ESP-IDF drivers: `idf.py install-drivers`
- Check Windows Device Manager for "USB Serial Device (COMx)"

### Multiple ESP32 boards connected:
```powershell
# Specify device serial number
openocd -f board/esp32s3-builtin.cfg -c "adapter serial <SERIAL_NUMBER>"
```

## Performance Tips

1. **Optimize for debugging** - Add to `sdkconfig`:
   ```ini
   CONFIG_COMPILER_OPTIMIZATION_DEBUG=y
   CONFIG_COMPILER_OPTIMIZATION_LEVEL_DEBUG=y
   ```

2. **Disable watchdog during debugging**:
   ```c
   #ifdef CONFIG_ESP_TASK_WDT
   esp_task_wdt_deinit();
   #endif
   ```

3. **Use hardware breakpoints** (limited to 2 on ESP32-S3)
4. **Use software breakpoints** for more breakpoints (slightly slower)

## Advanced Features

### Real-time Tracing (App Trace):
```c
#include "esp_app_trace.h"

// Send trace data
esp_apptrace_write(ESP_APPTRACE_DEST_TRAX, data, size, ESP_APPTRACE_TMO_INFINITE);
```

### Core Dump Analysis:
```powershell
# After a crash, retrieve core dump
idf.py coredump-info

# Debug the core dump
idf.py coredump-debug
```

### FreeRTOS Thread-Aware Debugging:
GDB automatically shows all FreeRTOS tasks:
```gdb
info threads
thread 3
bt
```

## Quick Start Example

1. Connect ESP32-S3 via USB
2. Press `F5` in VS Code
3. Set a breakpoint in `uflake_core_init()`
4. Program will halt at breakpoint
5. Use debug controls to step through code

**That's it! Happy debugging! 🎉**

---

## Watchdog and Task Monitoring

### Critical Tasks (Watchdog Enabled)
Tasks that MUST respond regularly - if they hang, the system should reset:
```c
uflake_process_create("GUI_Task", gui_task, NULL, 8192, 
                      PROCESS_PRIORITY_HIGH,
                      PROCESS_FLAG_WATCHDOG_ENABLE,  // ← Monitored
                      &pid);

void gui_task(void *arg) {
    while(1) {
        lv_timer_handler();
        uflake_process_yield(10);  // Feeds watchdog automatically
    }
}
```

### Heavy/Background Tasks (No Watchdog)
Tasks that may run for extended periods:
```c
uflake_process_create("AI_Task", ai_task, NULL, 16384,
                      PROCESS_PRIORITY_NORMAL,
                      PROCESS_FLAG_NONE,  // ← Not monitored
                      &pid);

void ai_task(void *arg) {
    while(1) {
        run_ai_inference();  // May take 60+ seconds
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Rule of Thumb:**
- **Enable watchdog**: UI, input, networking, time-critical tasks
- **Disable watchdog**: Heavy computation, file I/O, user apps, background tasks
