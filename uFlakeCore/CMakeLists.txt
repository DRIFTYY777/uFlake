# Automatically discover all apps in the Apps directory
file(GLOB APP_PATHS "${CMAKE_SOURCE_DIR}/Apps/*")
set(APP_COMPONENTS "")
foreach(APP_PATH ${APP_PATHS})
    if(IS_DIRECTORY ${APP_PATH})
        get_filename_component(APP_NAME ${APP_PATH} NAME)
        # Exclude non-component files
        if(NOT APP_NAME MATCHES ".*\\.(c|h|md|txt)$")
            list(APPEND APP_COMPONENTS ${APP_NAME})
            message(STATUS "uFlake: Found app component: ${APP_NAME}")
        endif()
    endif()
endforeach()

message(STATUS "uFlake: APP_COMPONENTS = ${APP_COMPONENTS}")

idf_component_register(
    SRCS 
        "uFlakeAppReg.c"
        "uFlakeCore.c"

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
        pthread
        uFlakeKernal
        uFlakeHAL
        uAppLoader
        uLibraries
        uGraphics
        uBootScreen
        lvgl__lvgl

        ${APP_COMPONENTS}
        
        
    PRIV_REQUIRES
        esp_pm
        esp_mm
        bootloader_support
        ${APP_COMPONENTS}
)

# Force linking of all app components to prevent symbol elimination
foreach(APP_COMP ${APP_COMPONENTS})
    idf_component_get_property(app_lib ${APP_COMP} COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PUBLIC "-Wl,--whole-archive" ${app_lib} "-Wl,--no-whole-archive")
endforeach()

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