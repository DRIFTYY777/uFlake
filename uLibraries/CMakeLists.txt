# uLibraries - Hardware driver components for uFlake
# This file registers all hardware driver components

idf_component_register(
    SRCS 
        "st7789/ST7789_driver.c"
        "pca9555/PCA9555.c"
        # "sd_card/sd_card_driver.c"
    
    INCLUDE_DIRS 
        "st7789"
        "pca9555"
        # "sd_card"
    
    REQUIRES 
        driver
        freertos
        log
        esp_common
        soc
        hal
        uFlakeHAL
        
    PRIV_REQUIRES
        esp_hw_support
        esp_system
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