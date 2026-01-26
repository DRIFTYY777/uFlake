idf_component_register(
    SRCS 
    "app_main.c"         
    "image.c"


    INCLUDE_DIRS "."
    REQUIRES uAppLoader uFlakeKernal lvgl__lvgl
)
