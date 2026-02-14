# CMakeLists.txt for C++ app with C-style programming
# Note: No special changes needed! ESP-IDF automatically handles .cpp files

idf_component_register(
    SRCS "app_main.cpp"   # Just use .cpp extension instead of .c
    INCLUDE_DIRS "."
    REQUIRES uAppLoader uFlakeKernel
)

# Optional: If you want to enforce specific C++ standards or flags
# target_compile_options(${COMPONENT_LIB} PRIVATE
#     -Wno-error=deprecated  # If you want to allow deprecated features
# )
