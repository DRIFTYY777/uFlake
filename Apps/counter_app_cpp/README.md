# Counter App (C++ Version)

This is an example demonstrating a C++ file (`.cpp`) with C-style programming in uFlake OS.

## Purpose

This example shows how to:
1. Convert a `.c` file to `.cpp` while maintaining C-style programming
2. Properly use `extern "C"` for interoperability
3. Gain type safety benefits without changing programming style

## Comparison with C Version

### Original C Version (`../counter_app/app_main.c`)
- Pure C code
- Standard C headers
- No extern "C" needed

### This C++ Version (`app_main.cpp`)
- Same functionality
- Same C-style programming
- C headers wrapped in `extern "C"`
- Public functions exported with `extern "C"`
- Can use `true`/`false` keywords (optional)

## Key Changes

1. **File extension**: `.c` → `.cpp`
2. **C headers wrapped**:
   ```cpp
   extern "C" {
   #include "freertos/FreeRTOS.h"
   #include "esp_log.h"
   }
   ```
3. **Entry point exported**:
   ```cpp
   extern "C" void counter_cpp_app_main(void) {
       // implementation
   }
   ```

## Benefits Gained

- ✅ Stricter type checking catches more bugs
- ✅ Better compiler error messages
- ✅ Can use `true`/`false` keywords
- ✅ Compatible with C++ libraries
- ✅ Can gradually add C++ features if needed

## What's the Same

- ✅ Same runtime performance
- ✅ Same memory usage (with -fno-exceptions -fno-rtti)
- ✅ Same C-style programming approach
- ✅ Works with FreeRTOS and ESP-IDF

## Building

This app is built automatically by ESP-IDF when included in the project:

```bash
idf.py build
```

The CMakeLists.txt automatically detects `.cpp` files and compiles them with the C++ compiler.

## When to Use This Approach

Use `.cpp` with C-style programming for:
- ✅ New user applications
- ✅ Code that benefits from type safety
- ✅ Projects that might need C++ features later

Keep `.c` for:
- ✅ Hardware drivers
- ✅ Core kernel code
- ✅ Legacy C codebases

## Learn More

- [C_TO_CPP_ANALYSIS.md](../../C_TO_CPP_ANALYSIS.md) - Detailed analysis
- [C_TO_CPP_MIGRATION_GUIDE.md](../../C_TO_CPP_MIGRATION_GUIDE.md) - Step-by-step guide
- [counter_app](../counter_app/) - Original C version

## Notes

This is a demonstration app. To actually use it in your project, you would need to:
1. Register it in `main.cpp`
2. Include it in your component dependencies
3. Build and flash to the device

See the original counter_app for a working implementation.
