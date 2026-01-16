idf_component_register(
    SRCS 
        "kernel.c"
        "src/memory_manager.c"
        "src/scheduler.c"
        "src/panic_handler.c"
        "src/logger.c"
        "src/synchronization.c"
        "src/crypto_engine.c"
        "src/buffer_manager.c"
        "src/timer_manager.c"
        "src/message_queue.c"
        "src/watchdog_manager.c"
        "src/event_manager.c"
        "src/resource_manager.c"
        "src/hw_auth.c"
    
    INCLUDE_DIRS 
        "."
        "include"  
    
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
        pthread
        efuse
        bootloader_support
        esp_partition
        espressif__esp_secure_cert_mgr
        
        
    PRIV_REQUIRES
        esp_pm
        esp_mm
        
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
