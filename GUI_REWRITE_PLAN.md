# GUI & AppLoader Complete Rewrite Plan - Flipper Zero-style Architecture

**Status**: Plan created, awaiting approval to implement

## Problem Statement

Current GUI system is:
- **Buggy**: Undefined variable definitions, initialization race conditions, focus failures
- **Laggy**: Task flooding (200 calls/sec), overcomplex LVGL management  
- **Overcomplicated**: Too many layers, multi-window system, complex lifecycle
- **Hard to debug**: Multiple async operations, unclear execution flow

**Solution**: Complete rewrite using **Flipper Zero** architecture principles:
- Single-screen model (no multi-window complexity)
- Simple menu stack (no group navigation mess)
- Direct app control flow (no async complications)
- Minimal RAM footprint
- Fast UI response (<20ms interaction latency)

---

## Architecture Overview

### App Types (4 Variants)

```
1. Internal Non-GUI Apps
   - Linked into firmware
   - Run in background
   - No display, no user interaction
   - Example: battery monitor service

2. Internal GUI Apps  
   - Linked into firmware
   - Full LVGL + user input
   - Simple menu-driven UI
   - Example: Settings, Counter

3. External Non-GUI Apps (ELF)
   - Loaded from SD card as .elf
   - Run in isolated context
   - No display access
   - Example: sensor logging

4. External GUI Apps (ELF)
   - Loaded from SD card as .elf
   - Access to GUI/input
   - Loaded dynamically
   - Example: third-party tools
```

### Core Components

#### 1. **Simple Menu System** (replaces complex group/focus)
```
Menu Structure:
- Home screen (app list or menu)
- Per-app UI (direct lv_obj hierarchy)
- Context stack (home -> app -> submenu -> ...)
```

#### 2. **RAM-Aware App Loader**
```
- Check available RAM before launching app
- Refuse launch if insufficient memory
- Clean shutdown if memory critical
- Track app memory usage
```

#### 3. **Direct Event Loop**
```
- LVGL handler called with lv_timer_handler() return value
- Single GUI task, simple state machine
- No async complications
- Direct app callbacks
```

#### 4. **Notification Bar** (optional, minimal)
```
- Fixed 24px bar at top
- Battery, time, status icons
- App name during execution
```

---

## File Structure (PROPOSED NEW)

### Remove These Files
```
uGraphics/
  - uGUI.cpp               (OLD - completely rewrite)
  - uGui.h                 (OLD - completely rewrite)
  - uGui_new.h             (REMOVE)
  - include/*              (OLD - remove all)
  - src/*                  (OLD - remove all)

uAppLoader/
  - appLoader.cpp          (OLD - completely rewrite)
  - appLoader.h            (OLD - completely rewrite)
  - appLifecycle.cpp       (OLD - remove)
  - appLifecycle.h         (OLD - remove)
  - appManifest.cpp        (OLD - remove)
  - appManifest.h          (OLD - keep types only)
  - appService.cpp         (OLD - remove)
  - appService.h           (OLD - remove)
  - app_context.cpp        (OLD - remove)
  - app_context.h          (OLD - remove)
  - app_force_exit.cpp     (OLD - remove)
  - app_force_exit.h       (OLD - remove)
  - memory_aware_launcher.cpp (OLD - remove)
  - memory_aware_launcher.h   (OLD - remove)
```

### Create These Files (NEW)
```
uGraphics/
├── CMakeLists.txt        (NEW)
├── include/
│   ├── uGui_simple.h     (NEW - Main GUI API)
│   ├── uGui_menu.h       (NEW - Menu system)
│   ├── uGui_memory.h     (NEW - RAM management)
│   └── uGui_types.h      (NEW - Core types)
└── src/
    ├── uGui_simple.cpp   (NEW - Core implementation)
    ├── uGui_menu.cpp     (NEW - Menu stack)
    ├── uGui_memory.cpp   (NEW - RAM tracking)
    ├── uGui_input.cpp    (NEW - Input handling)
    └── uGui_theme.cpp    (NEW - Colors/fonts, minimal)

uAppLoader/
├── CMakeLists.txt        (NEW)
├── include/
│   ├── appLoader_simple.h (NEW - Main API)
│   ├── app_types.h       (NEW - Type definitions)
│   └── app_registry.h    (NEW - App registry)
└── src/
    ├── appLoader_simple.cpp (NEW - Core loader)
    ├── app_registry.cpp   (NEW - App list management)
    ├── app_lifecycle.cpp  (NEW - Launch/exit)
    └── app_memory.cpp     (NEW - RAM checks)
```

---

## Core APIs (SIMPLIFIED)

### GUI API (uGui_simple.h)
```c
// Initialization
uflake_result_t uGui_init(void);
void uGui_task(void);              // Single task to call repeatedly

// App control
lv_obj_t* uGui_start_app(void);    // Clear screen, app creates UI here
void uGui_exit_app(void);          // Return to home screen

// Input handling (called by input driver)
void uGui_input_button(button_t btn, bool pressed);

// Memory query
uint32_t uGui_get_free_ram(void);
bool uGui_has_memory(uint32_t bytes);  // Check if enough RAM

// Get current state
bool uGui_is_home_screen(void);
```

### App Loader API (appLoader_simple.h)
```c
// Initialization
uflake_result_t app_loader_init(void);

// App management
uflake_result_t app_loader_register_app(const app_descriptor_t *app);
uflake_result_t app_loader_launch_app_by_name(const char *name);
void app_loader_request_exit(void);

// App queries
uint32_t app_loader_get_app_count(void);
app_descriptor_t* app_loader_get_app_list(uint32_t *count);

// Memory-aware launch
bool app_loader_can_launch(const char *name);  // Check memory before launch
```

---

## Implementation Phases

### Phase 1: Core GUI System (High Priority)
- [ ] Remove old uGUI.cpp/uGui.h completely
- [ ] Create new uGui_simple.cpp with minimal LVGL wrapper
- [ ] Implement uGui_menu.cpp (stack-based menu)
- [ ] Implement uGui_input.cpp (button callbacks)
- [ ] Create uGui_theme.cpp (minimal colors)

**Expected outcome**: Can render simple UI, handle input

### Phase 2: App Loader Rewrite (High Priority)  
- [ ] Remove old appLoader.cpp, appLifecycle.cpp, etc.
- [ ] Create new appLoader_simple.cpp with clean API
- [ ] Create app_registry.cpp (simple list)
- [ ] Create app_lifecycle.cpp (minimal: launch/exit)
- [ ] Keep appManifest.h types, update parser if needed

**Expected outcome**: Can register and launch apps

### Phase 3: Memory Management (Medium Priority)
- [ ] Track RAM usage per app
- [ ] Refuse launch if insufficient memory
- [ ] Handle memory pressure events
- [ ] API for apps to query free memory

**Expected outcome**: No OOM crashes, graceful memory handling

### Phase 4: Update Existing Apps (Low Priority)
- [ ] Update launcher (use new menu API)
- [ ] Update counter, settings, other apps
- [ ] Test each app with new system
- [ ] Fix compatibility issues

**Expected outcome**: All apps work with new system

### Phase 5: Testing & Optimization (Low Priority)
- [ ] Boot time measurement
- [ ] App launch timing (<200ms target)
- [ ] Memory usage validation
- [ ] Input latency test (<50ms target)
- [ ] Stress test (launch/exit cycles)

**Expected outcome**: Performance targets met

---

## Key Design Decisions

### 1. Input Model
**OLD**: LVGL focus groups, complex navigation
**NEW**: Direct button callbacks, app handles focus

### 2. App Lifecycle
**OLD**: Async launch, complex lifecycle state machine
**NEW**: Direct function call, app_main() entry point

### 3. Memory Model
**OLD**: No tracking, potential OOM crashes
**NEW**: Query before launch, refuse if insufficient

### 4. UI Structure
**OLD**: Multi-window, complex layers
**NEW**: Single screen, menu stack

### 5. Event Loop
**OLD**: Multiple async operations, LVGL groups
**NEW**: Simple poll loop, direct state machine

---

## Expected Improvements

| Metric | Old | New | Target |
|--------|-----|-----|--------|
| Boot time | ~10s | ~5s | < 5s ✓ |
| App launch | ~500ms | <200ms | < 200ms ✓ |
| Input latency | ~100ms | <50ms | < 50ms ✓ |
| CPU idle | 200 calls/sec | 20 calls/sec | -90% ✓ |
| Codebase | 3000+ lines | 1500 lines | -50% ✓ |
| Memory footprint | 150KB | 80KB | -50% ✓ |
| Crash rate | High (crashes) | Zero (defined behavior) | Stable ✓ |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Breaking existing apps | High | Keep compatibility layer, provide examples |
| Performance worse | High | Benchmark early, iterate quickly |
| Memory issues remain | Medium | Implement Phase 3 early in process |
| Removing code breaks something | Medium | Full git history preserved, can revert |
| SD card loading not working | Low | Test external app loading carefully |

---

## Testing Strategy

### Phase 1-2 Checkpoint (GUI + Loader)
- [ ] Compiles without errors
- [ ] Boots to home screen
- [ ] Can launch launcher app
- [ ] Input works (buttons visible)

### Phase 3 Checkpoint (Memory)
- [ ] RAM tracked correctly
- [ ] Can't launch if low memory
- [ ] Memory query API works
- [ ] No OOM crashes

### Final Testing
- [ ] All apps launch and exit
- [ ] No undefined behavior crashes
- [ ] Performance targets met
- [ ] Memory stable under stress
- [ ] Fast input response

---

## Files to Create (Priority Order)

1. **uGui_simple.h** - Core GUI API
2. **uGui_types.h** - Shared types
3. **uGui_simple.cpp** - Main implementation
4. **appLoader_simple.h** - Core app loader API
5. **appLoader_simple.cpp** - Implementation
6. **app_types.h** - App type definitions
7. **app_registry.cpp** - App list
8. **app_lifecycle.cpp** - Launch/exit
9. **uGui_input.cpp** - Input handling
10. **uGui_menu.cpp** - Menu stack
11. **app_memory.cpp** - RAM tracking
12. **uGui_memory.h** - Memory API
13. **uGui_memory.cpp** - Memory impl
14. **uGui_theme.cpp** - Colors/fonts

---

## Timeline Estimate

- **Phase 1** (GUI core): 2 hours
- **Phase 2** (App loader): 2 hours
- **Phase 3** (Memory): 1 hour
- **Phase 4** (App updates): 1 hour
- **Phase 5** (Testing): 1-2 hours
- **Total**: 7-9 hours

---

## Success Criteria

When complete:
- ✅ No more undefined behavior crashes
- ✅ Codebase is 50% smaller
- ✅ CPU usage 80% lower when idle
- ✅ App launch < 200ms
- ✅ Input response < 50ms
- ✅ All existing apps work
- ✅ Easy to add new apps
- ✅ Memory-aware operation
- ✅ Flipper Zero-like simplicity

---

**READY FOR APPROVAL TO IMPLEMENT**

