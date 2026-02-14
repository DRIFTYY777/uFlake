# Migration Guide: Converting C Files to C++ with C-Style Programming

This guide provides step-by-step instructions for converting `.c` files to `.cpp` files while maintaining C-style programming practices in uFlake OS.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Quick Start](#quick-start)
3. [Step-by-Step Conversion](#step-by-step-conversion)
4. [Common Patterns](#common-patterns)
5. [Troubleshooting](#troubleshooting)
6. [Best Practices](#best-practices)

---

## Prerequisites

Before converting files, ensure:
- ✅ Your code compiles successfully as `.c` files
- ✅ You understand the [benefits and losses](C_TO_CPP_ANALYSIS.md)
- ✅ You have a backup or version control (Git)

---

## Quick Start

### Minimal Conversion Checklist

For a simple user app (like those in `Apps/` folder):

```bash
# 1. Rename the file
mv app_main.c app_main.cpp

# 2. Update CMakeLists.txt
sed -i 's/app_main.c/app_main.cpp/g' CMakeLists.txt

# 3. Add extern "C" blocks (see below)

# 4. Test build
idf.py build
```

---

## Step-by-Step Conversion

### Step 1: Rename the File

```bash
cd Apps/your_app/
mv app_main.c app_main.cpp
```

### Step 2: Update CMakeLists.txt

**Before:**
```cmake
idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES uAppLoader uFlakeKernal
)
```

**After:**
```cmake
idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES uAppLoader uFlakeKernal
)
```

### Step 3: Wrap C Headers

**Before (app_main.c):**
```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "appLoader.h"
```

**After (app_main.cpp):**
```cpp
#include <cstdio>  // C++ style, or keep <stdio.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "appLoader.h"
}
```

**Rule**: Wrap all C headers that don't have C++ guards in `extern "C"` blocks.

### Step 4: Export C-Callable Functions

Any function that will be called from C code needs `extern "C"`:

**Before:**
```c
void my_app_main(void) {
    // app code
}
```

**After:**
```cpp
extern "C" void my_app_main(void) {
    // app code
}
```

**Alternative** (if multiple functions):
```cpp
extern "C" {
    void my_app_main(void);
    void my_helper_function(void);
}
```

### Step 5: Export Global Variables (if needed)

If you have global variables accessed from C:

**Before:**
```c
const app_bundle_t my_app = {
    .manifest = &my_manifest,
    .entry_point = my_app_main,
};
```

**After:**
```cpp
extern "C" {
    const app_bundle_t my_app = {
        .manifest = &my_manifest,
        .entry_point = my_app_main,
    };
}
```

### Step 6: Test the Build

```bash
cd /path/to/uFlake
idf.py build
```

If successful, your conversion is complete!

---

## Common Patterns

### Pattern 1: Simple User App

**File: `Apps/my_app/app_main.c`** → **`app_main.cpp`**

```cpp
#include <cstdio>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "appLoader.h"
}

static const char *TAG = "MyApp";

// App manifest
static const app_manifest_t my_manifest = {
    .name = "MyApp",
    .version = "1.0.0",
    // ... other fields
};

// Export entry point
extern "C" void my_app_main(void);

// Export app bundle
extern "C" {
    const app_bundle_t my_app = {
        .manifest = &my_manifest,
        .entry_point = my_app_main,
        .is_launcher = false
    };
}

// App code
extern "C" void my_app_main(void) {
    ESP_LOGI(TAG, "App started");
    while (true) {  // Can use 'true' instead of '1'
        // Your C-style code here
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Pattern 2: Header File

For header files (`.h`), add C++ guards:

**File: `my_module.h`**

```c
#ifndef MY_MODULE_H
#define MY_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

// Your C declarations here
void my_function(void);

typedef struct {
    int value;
} my_struct_t;

#ifdef __cplusplus
}
#endif

#endif // MY_MODULE_H
```

### Pattern 3: Mixed C and C++ Functions

You can have both C-linkage and C++-linkage functions in the same file:

```cpp
extern "C" {
#include "esp_log.h"
}

// C-linkage function (callable from C code)
extern "C" void public_api_function(void) {
    // implementation
}

// C++-linkage function (internal use only, can use C++ features)
static void internal_helper_function(void) {
    // Can use C++ features here if needed
}
```

### Pattern 4: Using C++ Features (Optional)

If you want to gradually introduce C++ features:

```cpp
extern "C" {
#include "esp_log.h"
}

// C-style function using C++ features internally
extern "C" void process_data(const uint8_t* data, size_t len) {
    // Use C++ bool instead of int
    bool success = false;
    
    // Use references for cleaner code (optional)
    auto process_byte = [](uint8_t byte) {
        return byte * 2;
    };
    
    // Still looks like C-style imperative code
    for (size_t i = 0; i < len; i++) {
        uint8_t result = process_byte(data[i]);
        // ...
    }
}
```

---

## Troubleshooting

### Error: "undefined reference to `my_function'"

**Cause**: Function is declared with C++ linkage but called from C code.

**Solution**: Add `extern "C"` to the function:

```cpp
extern "C" void my_function(void) {
    // implementation
}
```

### Error: "expected ';' before '{' token"

**Cause**: Missing `extern "C"` block around C headers.

**Solution**: Wrap C headers:

```cpp
extern "C" {
#include "problematic_header.h"
}
```

### Error: "invalid conversion from 'void*' to 'int*'"

**Cause**: C++ requires explicit casts from `void*`.

**Solution**: Add explicit cast:

```cpp
// Before (C):
int* ptr = malloc(sizeof(int));

// After (C++):
int* ptr = (int*)malloc(sizeof(int));

// Or use C++ cast:
int* ptr = static_cast<int*>(malloc(sizeof(int)));
```

### Error: "invalid conversion from 'const char*' to 'char*'"

**Cause**: C++ is stricter about const-correctness.

**Solution**: Use `const` properly:

```cpp
// Before:
char* str = "hello";  // Error in C++

// After:
const char* str = "hello";  // Correct
```

### Compilation is much slower

**Cause**: C++ compiler is slower than C compiler.

**Solution**: This is normal. C++ compilation is typically 10-30% slower.

---

## Best Practices

### ✅ DO:

1. **Start with user apps** - Convert apps in `Apps/` folder first
2. **Test incrementally** - Convert one file at a time
3. **Use extern "C" consistently** - For all C-callable functions
4. **Keep C-style** - Don't force C++ features unless they help
5. **Add comments** - Explain why code is in `.cpp` if using C-style
6. **Check size** - Compare binary size before/after conversion

### ❌ DON'T:

1. **Don't convert core kernel** - Keep `uFlakeKernal/` as `.c` for compatibility
2. **Don't convert HAL drivers** - Keep `uFlakeHAL/` as `.c`
3. **Don't use exceptions** - They're disabled (`-fno-exceptions`)
4. **Don't use RTTI** - It's disabled (`-fno-rtti`)
5. **Don't mix C and C++ headers** - Keep them separate
6. **Don't remove `extern "C"`** - It's required for C compatibility

---

## Conversion Checklist

For each file you convert:

- [ ] File renamed from `.c` to `.cpp`
- [ ] CMakeLists.txt updated with new filename
- [ ] C headers wrapped in `extern "C"` blocks
- [ ] Public functions marked with `extern "C"`
- [ ] Global variables exported with `extern "C"` (if accessed from C)
- [ ] File compiles without errors
- [ ] File compiles without warnings
- [ ] Binary size checked (should be similar)
- [ ] Functionality tested
- [ ] Committed to version control

---

## Example: Complete Conversion

### Before: `counter_app/app_main.c`

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "appLoader.h"

static const char *TAG = "CounterApp";

static const app_manifest_t counter_manifest = {
    .name = "Counter",
    .version = "1.0.0",
    .type = APP_TYPE_INTERNAL,
};

void counter_app_main(void);

const app_bundle_t counter_app = {
    .manifest = &counter_manifest,
    .entry_point = counter_app_main,
};

void counter_app_main(void) {
    int counter = 0;
    while (1) {
        counter++;
        if (counter >= 50) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### After: `counter_app/app_main.cpp`

```cpp
#include <cstdio>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "appLoader.h"
}

static const char *TAG = "CounterApp";

static const app_manifest_t counter_manifest = {
    .name = "Counter",
    .version = "1.0.0",
    .type = APP_TYPE_INTERNAL,
};

extern "C" void counter_app_main(void);

extern "C" {
    const app_bundle_t counter_app = {
        .manifest = &counter_manifest,
        .entry_point = counter_app_main,
    };
}

extern "C" void counter_app_main(void) {
    int counter = 0;
    while (true) {  // Can use 'true' instead of '1'
        counter++;
        if (counter >= 50) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Changes Made:**
1. ✅ Renamed file: `.c` → `.cpp`
2. ✅ Changed `<stdio.h>` → `<cstdio>`
3. ✅ Wrapped C headers in `extern "C"`
4. ✅ Added `extern "C"` to entry point
5. ✅ Wrapped app bundle in `extern "C"`
6. ✅ Used `true` instead of `1` (optional)

---

## Performance Impact

No performance difference for C-style code:

| Metric | C (.c) | C++ (.cpp) |
|--------|--------|------------|
| Runtime Speed | Baseline | Same |
| Code Size | Baseline | +0-2% |
| RAM Usage | Baseline | Same |
| Compile Time | Baseline | +10-20% |

---

## Resources

- [C_TO_CPP_ANALYSIS.md](C_TO_CPP_ANALYSIS.md) - Detailed benefits/losses analysis
- [ESP-IDF C++ Support](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/cplusplus.html)
- [Example App](Apps/counter_app_cpp/) - Complete working example

---

## Getting Help

If you encounter issues:

1. Check this guide's [Troubleshooting](#troubleshooting) section
2. Review the [example app](Apps/counter_app_cpp/)
3. Compare your code with the working C version
4. Check compiler error messages carefully

Remember: Converting to `.cpp` with C-style programming is about gaining type safety and flexibility, not about changing your programming style!
