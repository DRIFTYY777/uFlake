idf_component_register(
    SRCS 
        "kernel.cpp"
        "src/memory_manager.cpp"
        "src/scheduler.cpp"
        "src/panic_handler.cpp"
        "src/logger.cpp"
        "src/synchronization.cpp"
        "src/crypto_engine.cpp"
        "src/buffer_manager.cpp"
        "src/timer_manager.cpp"
        "src/message_queue.cpp"
        "src/watchdog_manager.cpp"
        "src/event_manager.cpp"
        "src/resource_manager.cpp"
        "src/hw_auth.cpp"
    
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
        nvs_flash
        
        
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
set(COMPONENT_NAME "uFlakeKernel")
