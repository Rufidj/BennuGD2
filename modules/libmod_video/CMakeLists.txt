project(mod_video)
# libmod_video: FFmpeg video playback for BennuGD2 (independent project).
# Drop (or symlink) this directory in BennuGD2/modules/ and BennuGD2's own build picks it up like any module.
#
# FFmpeg is found in this order:
#   1. -DFFMPEG_INCLUDE_DIRS=... -DFFMPEG_LIBRARIES=...   (explicit; used for the PC test sysroot and for PS5)
#   2. pkg-config (libavformat libavcodec libswscale libswresample libavutil)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

find_package(SDL2 REQUIRED)

# SDL2_mixer headers (same variable libbgsound uses on platforms without a system SDL2_mixer)
set(VIDEO_MIXER_INCLUDE "")
if(SDLMIXER_INCLUDE_DIR)
    set(VIDEO_MIXER_INCLUDE ${SDLMIXER_INCLUDE_DIR})
elseif(SDL2_mixer_INCLUDE_DIRS)
    set(VIDEO_MIXER_INCLUDE ${SDL2_mixer_INCLUDE_DIRS})
endif()

if(NOT FFMPEG_LIBRARIES)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG QUIET libavformat libavcodec libswscale libswresample libavutil)
    endif()
endif()
if(NOT FFMPEG_LIBRARIES)
    message(WARNING "mod_video: FFmpeg not found (set FFMPEG_INCLUDE_DIRS / FFMPEG_LIBRARIES); module skipped")
    return()
endif()

add_definitions(${EXTRA_CFLAGS})

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/core/bgdrtm
    ${CMAKE_SOURCE_DIR}/modules/libbggfx ${CMAKE_SOURCE_DIR}/modules/libsdlhandler
    ${CMAKE_SOURCE_DIR}/modules/libmod_video
    ${SDL_GPU_INCLUDE_DIR} ${VIDEO_MIXER_INCLUDE} ${FFMPEG_INCLUDE_DIRS} ${INCLUDE_DIRECTORIES})

file(GLOB SOURCES_LIBMOD_VIDEO "${CMAKE_SOURCE_DIR}/modules/libmod_video/*.c")

add_library(mod_video ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_VIDEO})
target_compile_definitions(mod_video PRIVATE USE_SDL2_GPU=1)

target_link_libraries(mod_video ${SDL2_LIBRARY} ${SDL2_LIBRARIES} ${SDL_GPU_LIBRARY} ${SDLMIXER_LIBRARY}
    -L${CMAKE_SOURCE_DIR}/bin bgdrtm bggfx ${FFMPEG_LIBRARIES})
if(WIN32)
    target_link_libraries(mod_video ws2_32 iphlpapi)
endif()
