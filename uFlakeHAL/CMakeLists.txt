idf_component_register(
    SRCS 
        "uHal.c"
        "src/uGPIO.c"
        "src/uI2c.c"
        "src/uSPI.c"
        "src/uNVS.c" 
        "src/uUART.c"
        "src/uGPIO.c"
        "src/uADC.c"
    
    INCLUDE_DIRS 
        "."
        "include"  
    
    REQUIRES 
        driver
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        esp_driver_ledc
        nvs_flash 
        esp_driver_uart
        driver esp_adc

        uFlakeKernel

)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
