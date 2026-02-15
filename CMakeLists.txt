# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Automatically discover all apps in the Apps directory
file(GLOB APP_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Apps/*")
list(FILTER APP_DIRS EXCLUDE REGEX ".*\\.(c|cpp|h|md|txt)$")



# Add both the kernel and the application components
set(EXTRA_COMPONENT_DIRS 
    "S3ZERO/uFlake"
    "uFlakeCore"
    "uFlakeKernel"
    "uFlakeHAL"
    "uLibraries"
    "uAppLoader"
    "uGraphics"
    "uBootScreen"
    ${APP_DIRS}
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
