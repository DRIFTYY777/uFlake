idf_component_register(
    SRCS 
        "uI2c/uI2c.c"
    
    INCLUDE_DIRS 
        "."
        "uI2c"
    
    REQUIRES 
        driver
        log
        freertos
        esp_system
        uFlakeKernal
)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
