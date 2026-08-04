idf_component_register(
    SRCS 
        "appLoader_simple.cpp"
    
    INCLUDE_DIRS 
        "."
    
    REQUIRES 
        freertos
        esp_common
        uFlakeCore
        uGraphics
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
