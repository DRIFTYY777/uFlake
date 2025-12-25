idf_component_register(
    SRCS 
        "boot_screen.c"
        "unicode.c"
        "font_render/font_render.c"


        INCLUDE_DIRS
        "."
        "font_render/include"
        "font_render/freetype2/include"

    
    REQUIRES 
        uLibraries
    
)

# Set compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
