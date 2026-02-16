# Automatically discover all apps in the Apps directory
file(GLOB APP_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Apps/*")
list(FILTER APP_DIRS EXCLUDE REGEX ".*\\.(c|h|md|txt)$")



idf_component_register(
    SRCS 
        "uGui.c"
        "src/gui_input.c"
        "src/uGui_focus.c"
        "src/uGui_notification.c"
        "src/uGui_appwindow.c"
        "src/uGui_theme.c"
        "src/uGui_widgets.c"
        "src/uGui_navigation.c"

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
        ${APP_DIRS}

)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
