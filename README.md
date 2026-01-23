# uFlake OS

A lightweight, modular real-time operating system built on ESP-IDF for ESP32-S3 microcontrollers with advanced peripheral support and LVGL-based GUI.

## Features

- **Custom Kernel**: Built on top of FreeRTOS with enhanced subsystems
    - Process & thread management with priority scheduling
    - Memory manager supporting Internal RAM, PSRAM, and DMA-capable memory
    - Message queue system for inter-process communication
    - Event-driven architecture
    - Timer management with high-resolution microsecond delays
    - Watchdog monitoring
    - Cryptographic engine integration
    - Resource and buffer management
    - Panic handler with recovery

- **Hardware Abstraction Layer (HAL)**
    - I2C bus manager with multi-device support
    - SPI bus manager (dual SPI support)
    - UART subsystem
    - NVS (Non-Volatile Storage) wrapper
    - SD Card interface
    - NRF24L01+ wireless module support
    - PCA9555 I/O expander support
    - ST7789 display driver

- **Graphics Subsystem**
    - LVGL 9.x integration
    - Hardware-accelerated display rendering
    - Double-buffered display output
    - DMA-optimized buffer management
    - Touch input support via I/O expander

- **Application Framework**
    - Dynamic app loading system
    - App registry with versioning
    - Modular component architecture
    - Hot-reload capable

## Hardware Requirements

- ESP32-S3 microcontroller
- ST7789 TFT display (240x320 or 320x240)
- SD Card module (SPI)
- NRF24L01+ wireless module (optional)
- PCA9555 I/O expander (for input)
- PSRAM (recommended for LVGL)

## Building

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Configure project
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py flash monitor
```

## Project Structure

```
uFlake/
├── uFlakeCore/         # Core OS initialization
├── uFlakeKernal/       # Kernel subsystems
├── uFlakeHAL/          # Hardware abstraction layer
├── uGraphics/          # GUI subsystem (uGui)
├── uLibraries/         # Driver libraries
├── uAppLoader/         # Application loader
├── Apps/               # User applications
└── S3ZERO/             # Board-specific code
```

## Memory Management

uFlake provides three memory allocation types:
- **UFLAKE_MEM_INTERNAL**: Fast internal RAM
- **UFLAKE_MEM_SPIRAM**: Large PSRAM for buffers
- **UFLAKE_MEM_DMA**: DMA-capable memory for peripherals

## API Example

```c
// Create a process
uint32_t pid;
uflake_process_create("MyTask", task_function, NULL, 
                                            4096, PROCESS_PRIORITY_NORMAL, &pid);

// Allocate memory
void *buffer = uflake_malloc(1024, UFLAKE_MEM_INTERNAL);

// Create message queue
uflake_msgqueue_t *queue;
uflake_msgqueue_create("MyQueue", 10, true, &queue);

// Send message
uflake_message_t msg = {.type = MSG_TYPE_DATA};
uflake_msgqueue_send(queue, &msg, 1000);
```

