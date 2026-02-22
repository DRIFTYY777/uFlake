# uLibraries - Hardware driver components for uFlake
# This file registers all hardware driver components

idf_component_register(
    SRCS 
        "st7789/ST7789.cpp"
        "pca9555/PCA9555.cpp"
        "sdCard/sdCard.cpp"
        "nrf24/nrf24.cpp"
        "imageCodec/imageCodec.cpp"
    
    INCLUDE_DIRS 
        "."
        "pca9555"
        "sdCard"
        "nrf24"
        "st7789"
        "imageCodec"
    
    REQUIRES 
        driver
        freertos
        log
        esp_common
        soc
        fatfs 
        hal
        uFlakeHAL
        uFlakeKernel
        lvgl__lvgl
        esp_new_jpeg
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
