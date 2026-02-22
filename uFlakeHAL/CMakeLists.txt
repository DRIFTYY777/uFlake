idf_component_register(
    SRCS 
        "uHal.cpp"
        "src/uGPIO.cpp"
        "src/uI2c.cpp"
        "src/uSPI.cpp"
        "src/uNVS.cpp" 
        "src/uUART.cpp"
        "src/uGPIO.cpp"
        "src/uADC.cpp"
    
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
