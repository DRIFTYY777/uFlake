idf_component_register(
    SRCS 
        "src/uI2c.c"
        "src/uSPI.c"
        "src/uNVS.c" 
        "src/uUART.c"
    
    INCLUDE_DIRS 
        "include"  
    
    REQUIRES 
        driver
        esp_driver_i2c
        esp_driver_spi
        nvs_flash 
        esp_driver_uart
        uFlakeKernal

)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
