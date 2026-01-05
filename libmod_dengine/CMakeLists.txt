cmake_minimum_required(VERSION 3.5)
project(mod_doom)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")

set(CMAKE_MODULE_PATH ../../cmake/Modules)

# Buscar paquetes necesarios
find_package(SDL2 REQUIRED)

add_definitions(-D__LIBMOD_DOOM ${EXTRA_CFLAGS})

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS}
                   ../../core/include
                   ../../core/bgdrtm
                   ../../modules/libbggfx
                   ../../modules/libsdlhandler
                   ../../vendor/sdl-gpu/include
                   ${INCLUDE_DIRECTORIES})

# Archivos fuente del módulo
file(GLOB SOURCES_LIBMOD_DOOM
     "../../modules/libmod_dengine/*.c"
     )

add_library(mod_doom ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_DOOM})

target_link_libraries(mod_doom
    ${SDL2_LIBRARY}
    ${SDL2_LIBRARIES}
    -L../../bin
    bgdrtm
    bggfx
    sdlhandler
    ${STDLIBSFLAGS}
    -lm)
