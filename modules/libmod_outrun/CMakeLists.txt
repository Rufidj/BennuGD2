project(mod_outrun)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")

set(CMAKE_MODULE_PATH ../../cmake/Modules)

find_package(SDL2 REQUIRED)

add_definitions(${EXTRA_CFLAGS})

include_directories(
    ${SDL2_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ../../core/include
    ../../core/bgdrtm
    ../../modules/libmod_outrun
    ${INCLUDE_DIRECTORIES}
)

file(GLOB SOURCES_LIBMOD_outrun
     "../../modules/libmod_outrun/*.c"
)

add_library(mod_outrun ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_outrun})

target_link_libraries(mod_outrun ${SDL2_LIBRARY} ${SDL2_LIBRARIES} -L../../bin bgdrtm)
if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0600)
    target_link_libraries(mod_outrun ws2_32)
endif()
if (SWITCH)
    target_link_libraries(mod_outrun nx)
endif()