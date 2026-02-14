idf_component_register(
    SRCS 
        "uBootScreen.c"
       
    INCLUDE_DIRS "."
    REQUIRES 
        uFlakeKernel
        uFlakeHAL
        uLibraries
        uGraphics
        freertos
        esp_timer
        lvgl__lvgl
)
