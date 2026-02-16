# uLibraries - Hardware driver components for uFlake
# This file registers all hardware driver components

idf_component_register(
    SRCS 
        "input/input.c"

    
    INCLUDE_DIRS 
        "."
        "input"
        
    
    REQUIRES 
        driver
        freertos
        log
        esp_common
        soc
        fatfs 
        hal
        uFlakeHAL
        uLibraries
        uAppLoader
        uFlakeKernel
    PRIV_REQUIRES
        esp_hw_support
        esp_system
        sdmmc
)

# Set compiler flags for hardware drivers
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
    -ffunction-sections
    -fdata-sections
)
