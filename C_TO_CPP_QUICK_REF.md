# C to C++ Quick Reference Card

## TL;DR

Converting `.c` to `.cpp` with C-style programming:
- ✅ **Gains**: Type safety, flexibility
- ⚠️ **Costs**: Slightly slower compilation, need `extern "C"`
- 🚀 **Performance**: No runtime difference
- 💡 **Recommended**: For user apps, not core kernel

---

## Conversion Checklist

```bash
# 1. Rename file
mv app.c app.cpp

# 2. Update CMakeLists.txt
# Change: SRCS "app.c"
# To:     SRCS "app.cpp"

# 3. Wrap C headers
extern "C" {
#include "your_c_header.h"
}

# 4. Export C functions
extern "C" void your_function() {
    // code
}

# 5. Build
idf.py build
```

---

## Code Patterns

### Pattern 1: Simple App
```cpp
#include <cstdio>
extern "C" {
#include "freertos/FreeRTOS.h"
}

extern "C" void app_main() {
    while (true) {
        // your code
    }
}
```

### Pattern 2: With Manifest
```cpp
extern "C" {
#include "appLoader.h"
}

static const app_manifest_t manifest = {...};
extern "C" void my_app_main();

extern "C" {
    const app_bundle_t my_app = {
        .manifest = &manifest,
        .entry_point = my_app_main
    };
}

extern "C" void my_app_main() {
    // app code
}
```

### Pattern 3: Header File
```c
/* my_header.h */
#ifndef MY_HEADER_H
#define MY_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

void my_function(void);

#ifdef __cplusplus
}
#endif

#endif
```

---

## Common Issues

| Error | Fix |
|-------|-----|
| `undefined reference` | Add `extern "C"` |
| `expected ';' before '{'` | Wrap C headers in `extern "C"` |
| `invalid conversion from void*` | Cast: `(type*)ptr` |
| `const char* to char*` | Use `const` properly |

---

## What to Convert

| Component | Convert? |
|-----------|----------|
| User Apps | ✅ YES |
| New Code | ✅ YES |
| Core Kernel | ❌ NO |
| HAL Drivers | ❌ NO |
| Legacy Code | ⚠️ MAYBE |

---

## Quick Comparison

| Feature | C (.c) | C++ (.cpp) |
|---------|--------|------------|
| Type Safety | Weak | Strong ✅ |
| true/false | No | Yes ✅ |
| Compile Time | Fast | Slower ⚠️ |
| Code Size | Small | +0-5% ⚠️ |
| Performance | Fast | Same ✅ |
| C++ Features | No | Optional ✅ |

---

## Essential Rules

1. **Always use `extern "C"`** for C-callable functions
2. **Wrap C headers** in `extern "C"` blocks
3. **Cast `void*` explicitly** - C++ requires it
4. **Use const correctly** - C++ is strict
5. **Keep it simple** - Don't force C++ features

---

## One-Minute Example

**Before (C):**
```c
#include <stdio.h>
#include "esp_log.h"

void my_app() {
    int x = 0;
    while (1) {
        x++;
    }
}
```

**After (C++):**
```cpp
#include <cstdio>
extern "C" {
#include "esp_log.h"
}

extern "C" void my_app() {
    int x = 0;
    while (true) {
        x++;
    }
}
```

**Changes:**
- File: `.c` → `.cpp`
- Headers: wrapped in `extern "C"`
- Function: exported with `extern "C"`
- Loop: `1` → `true` (optional)

---

## Resources

- 📖 [Full Analysis](C_TO_CPP_ANALYSIS.md) - Detailed discussion
- 📘 [Migration Guide](C_TO_CPP_MIGRATION_GUIDE.md) - Step-by-step
- 📋 [Summary](C_TO_CPP_SUMMARY.md) - Overview
- 💻 [Example App](Apps/counter_app_cpp/) - Working code

---

## Bottom Line

✅ **DO IT** if you want:
- Stronger type checking
- Flexible APIs (overloading, defaults)
- Future C++ features

❌ **SKIP IT** if you need:
- Maximum compatibility
- Fastest compilation
- Pure C codebase

**For uFlake**: Convert user apps, keep kernel as C.
