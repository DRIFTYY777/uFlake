# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add both the kernel and the application components
set(EXTRA_COMPONENT_DIRS 
    "uFlakeKernal"
    "S3ZERO/uFlake"
    "S3ZERO/uFlakeHAL"
    "uLibraries"
    "uAppLoader"
    "Apps/launcher"
    "Apps/counter_app"
    # "S3ZERO/boot_screen"
)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project definition
project(uFlake)

# Project-wide settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Add project-wide compile definitions
add_compile_definitions(
    UFLAKE_PROJECT=1
    UFLAKE_TARGET_ESP32S3=1
)

# Set project version
set(PROJECT_VER "1.0.0")
