cmake_minimum_required(VERSION 3.5)
project(mod_ray)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")

set(CMAKE_MODULE_PATH ../../cmake/Modules)

# Buscar SDL2 (requerido por libbggfx)
find_package(SDL2 REQUIRED)

add_definitions(-D__LIBMOD_RAY ${EXTRA_CFLAGS})

include_directories(
    ${SDL2_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ../../core/include
    ../../core/bgdrtm
    ../../modules/libbggfx
    ../../modules/libsdlhandler
    ../../vendor/sdl-gpu/include
    ${INCLUDE_DIRECTORIES}
)

# Archivos fuente del módulo raycaster
set(SOURCES_LIBMOD_RAY
    libmod_ray.c
    libmod_ray_shape.c
    libmod_ray_raycasting.c
    libmod_ray_render.c
    libmod_ray_map.c
    libmod_ray_sectors.c
    libmod_ray_portals.c
    libmod_ray_portal_projection.c
    libmod_ray_portal_render.c
)

add_library(mod_ray ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_RAY})

target_link_libraries(mod_ray
    ${SDL2_LIBRARY}
    ${SDL2_LIBRARIES}
    -L../../bin
    bgdrtm
    bggfx
    sdlhandler
    ${STDLIBSFLAGS}
    -lm
)
