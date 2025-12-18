idf_component_register(
    SRCS 
        "kernel.c"
        "memory/memory_manager.c"
        "scheduler/scheduler.c"
        "panic/panic_handler.c"
        "logging/logger.c"
        "sync/synchronization.c"
        "crypto/crypto_engine.c"
        "buffer/buffer_manager.c"
        "timer/timer_manager.c"
        "ipc/message_queue.c"
        "watchdog/watchdog_manager.c"
        "event/event_manager.c"
        "resource/resource_manager.c"
    
    INCLUDE_DIRS 
        "."
        "memory"
        "scheduler"
        "panic"
        "logging"
        "sync"
        "crypto"
        "buffer"
        "timer"
        "ipc"
        "watchdog"
        "event"
        "resource"
    
    REQUIRES 
        driver
        log
        freertos
        mbedtls
        esp_hw_support
        esp_system
        heap
        esp_timer
        esp_common
        soc
        hal
        esp_rom
        newlib
        pthread
        
    PRIV_REQUIRES
        esp_pm
        esp_mm
        bootloader_support
)

# Set compiler flags for the kernel
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
    -Wno-unused-parameter
    -Wno-sign-compare
    -O2
    -ffunction-sections
    -fdata-sections
    -DUFLAKE_KERNEL_BUILD=1
)

# Add preprocessor definitions
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    UFLAKE_KERNEL_VERSION_MAJOR=1
    UFLAKE_KERNEL_VERSION_MINOR=0
    UFLAKE_KERNEL_VERSION_PATCH=0
)

# Add kernel configuration
if(CONFIG_SPIRAM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE UFLAKE_SPIRAM_SUPPORT=1)
endif()

if(CONFIG_ESP_TASK_WDT_EN)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE UFLAKE_WATCHDOG_SUPPORT=1)
endif()

# Link time optimization for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${COMPONENT_LIB} PRIVATE -flto)
    target_link_options(${COMPONENT_LIB} PRIVATE -flto)
endif()

# Set component name for better identification
set(COMPONENT_NAME "uFlakeKernal")
