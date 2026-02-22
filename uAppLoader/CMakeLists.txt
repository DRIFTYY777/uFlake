idf_component_register(
    SRCS 
        "appLoader.cpp"
        "appLifecycle.cpp"
        "appService.cpp"
        "appManifest.cpp"
    
    INCLUDE_DIRS 
        "."
    
    REQUIRES 
        uFlakeKernel
        uFlakeHAL
        nvs_flash
)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)
