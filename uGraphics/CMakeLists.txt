idf_component_register(
    SRCS 
        "src/uGui_simple.cpp"
    
    INCLUDE_DIRS 
        "include"  
    
    REQUIRES 
        freertos
        esp_common
        lvgl__lvgl
        uFlakeCore
)

# Compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
    -Wno-unused-parameter
    -Wno-sign-compare
    -O2
)
