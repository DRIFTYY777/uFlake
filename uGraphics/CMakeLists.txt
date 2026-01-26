idf_component_register(
    SRCS 
        "uGUI.c"

    INCLUDE_DIRS 
        "."
        "include"  
    
    REQUIRES 
        driver
        esp_driver_i2c
        esp_driver_spi
        nvs_flash 
        esp_driver_uart
        uFlakeKernal
        lvgl__lvgl
        uLibraries
        uFlakeHAL

)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
