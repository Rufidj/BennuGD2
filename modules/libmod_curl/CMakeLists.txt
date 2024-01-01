project(mod_curl)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")

set(CMAKE_MODULE_PATH ../../cmake/Modules)

find_package(SDL2 REQUIRED)
if(USE_SDL2_GPU)
    find_package(SDL_GPU REQUIRED)
endif()

add_definitions(-D__LIBMODCURL ${EXTRA_CFLAGS})

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS} ../../core/include ../../core/bgdrtm ../../modules/libmodcurl ${SDL_GPU_INCLUDE_DIR} ${INCLUDE_DIRECTORIES})

file(GLOB SOURCES_LIBMODCURL
     "../../modules/libmod_curl/*.c"
     )

add_library(mod_curl ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMODCURL})

target_link_libraries(mod_curl ${SDL2_LIBRARY} ${SDL2_LIBRARIES} ${SDL_GPU_LIBRARY} -L../../bin bgdrtm ${OGL_LIB} ${VLC_LIB})
