idf_component_register(
    SRCS 
        "uI2c/uI2c.c"
        "uSPI/uSPI.c"
        "uNVS/uNVS.c"  # ✅ Add NVS source
    
    INCLUDE_DIRS 
        "."
        "uI2c"
        "uSPI"
        "uNVS"  # ✅ Add NVS include
    
    REQUIRES 
        driver
        esp_driver_i2c
        esp_driver_spi
        nvs_flash  # ✅ Add nvs_flash dependency
        uFlakeKernal
)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
