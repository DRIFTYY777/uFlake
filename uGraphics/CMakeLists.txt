# Automatically discover all apps in the Apps directory
file(GLOB APP_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Apps/*")
list(FILTER APP_DIRS EXCLUDE REGEX ".*\\.(c|h|md|txt)$")



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
        ${APP_DIRS}

)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
