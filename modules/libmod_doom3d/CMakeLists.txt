cmake_minimum_required(VERSION 3.10)
project(libmod_doom3d C)

set(CMAKE_C_STANDARD 99)

add_library(libmod_doom3d MODULE
    libmod_doom3d.c
)

target_include_directories(libmod_doom3d PRIVATE
    $ENV{DEVKITPRO}/modules/include
    ${CMAKE_SOURCE_DIR}/modules/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(libmod_doom3d SDL2 SDL2_gfx)

# Asegura que se genera como DLL (Windows) y no como .so o .dylib
set_target_properties(libmod_doom3d PROPERTIES
    PREFIX ""                       # No anteponer 'lib'
    SUFFIX ".dll"                  # Forzar a .dll
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/modules/libmod_doom3d
)
