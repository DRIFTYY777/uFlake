idf_component_register(
    SRCS 
        "uBootScreen.c"
        "uBootScreenLVGL.c"
    INCLUDE_DIRS "."
    REQUIRES 
        uFlakeHAL
        uLibraries
        uGraphics
        freertos
        esp_timer
        lvgl__lvgl
)
