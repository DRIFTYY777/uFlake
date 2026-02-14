idf_component_register(
    SRCS 
        "appLoader.c"
        "appLifecycle.c"
        "appService.c"
        "appManifest.c"
    
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
