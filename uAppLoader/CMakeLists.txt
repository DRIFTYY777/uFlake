idf_component_register(
    SRCS 
        "appLoader.c"
    
    INCLUDE_DIRS 
        "."
    
    REQUIRES 
        uFlakeKernal
        uFlakeHAL
        nvs_flash
)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)
