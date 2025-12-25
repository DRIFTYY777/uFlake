idf_component_register(
    SRCS 
        "uI2c/uI2c.c"
        "uSPI/uSPI.c"
        "uNVS/uNVS.c" 
    
    INCLUDE_DIRS 
        "."
        "uI2c"
        "uSPI"
        "uNVS"  
    
    REQUIRES 
        driver
        esp_driver_i2c
        esp_driver_spi
        nvs_flash 
        uFlakeKernal
)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
