idf_component_register(
    SRCS 
        "uBootScreen.cpp"
       
    INCLUDE_DIRS "."
    REQUIRES 
        uFlakeKernel
        uFlakeHAL
        uLibraries
        freertos
        esp_timer
        lvgl__lvgl
)
