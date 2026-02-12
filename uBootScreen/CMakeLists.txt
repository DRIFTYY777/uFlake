idf_component_register(
    SRCS 
        "uBootScreen.c"
       
    INCLUDE_DIRS "."
    REQUIRES 
        uFlakeKernal
        uFlakeHAL
        uLibraries
        uGraphics
        freertos
        esp_timer
        lvgl__lvgl
)
