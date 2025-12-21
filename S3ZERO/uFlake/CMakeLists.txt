idf_component_register(
    SRCS 
        "main.cpp"
    
    INCLUDE_DIRS 
        "."
    
    REQUIRES 
        driver
        log
        freertos
        mbedtls
        esp_hw_support
        esp_system
        heap
        esp_timer
        esp_common
        soc
        hal
        esp_rom
        newlib
        pthread
        uFlakeKernal
        uFlakeHAL  
        
    PRIV_REQUIRES
        esp_pm
        esp_mm
        bootloader_support
)

# Set compiler flags for the application
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
    -Wno-unused-parameter
    -Wno-sign-compare
    -O2
    -ffunction-sections
    -fdata-sections
)

# Set C++ standard
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD_REQUIRED ON)

# Link time optimization for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${COMPONENT_LIB} PRIVATE -flto)
    target_link_options(${COMPONENT_LIB} PRIVATE -flto)
endif()
