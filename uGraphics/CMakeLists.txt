idf_component_register(
    SRCS 
        "uGUI.cpp"
        "src/uInputs.cpp"
        "src/gui_types.cpp"
        "src/uGui_notification.cpp"
        "src/uGui_theme.cpp"
    
    INCLUDE_DIRS 
        "."
        "include"  
    
    REQUIRES 
        driver
        esp_driver_i2c
        esp_driver_spi
        nvs_flash 
        esp_driver_uart
        uFlakeKernel
        lvgl__lvgl
        uLibraries
        uFlakeHAL
        uServices
        
        
        
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
