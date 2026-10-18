project(bggfx)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

find_package(SDL2 REQUIRED)
if(USE_SDL2_GPU)
    find_package(SDL_GPU REQUIRED)
endif()

# FreeType2 (TTF support, backported from PixTudio - SplinterGU/BennuGD2#6): not vendored
# for the PS5 payload SDK, so it's required on every other target but skipped on PROSPERO
# (g_font_ttf.c itself is swapped out for g_font_ttf_stub.c below).
if(NOT PROSPERO)
    find_package(Freetype REQUIRED)
endif()

add_definitions(-D__LIBBGFGX ${EXTRA_CFLAGS})

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/core/bgdrtm ${CMAKE_SOURCE_DIR}/modules/libsdlhandler ${CMAKE_SOURCE_DIR}/modules/libbggfx ${SDL_GPU_INCLUDE_DIR} ${INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/vendor/theoraplay ${FREETYPE_INCLUDE_DIRS})

if(PROSPERO)
    include_directories(${PS5_OPENGL_SDK_ROOT}/include)
endif()

file(GLOB SOURCES_LIBBGGFX
     "${CMAKE_SOURCE_DIR}/modules/libbggfx/*.c"
     "${CMAKE_SOURCE_DIR}/vendor/theoraplay/theoraplay.c"
     )

# No FreeType2 on PROSPERO (see above) - swap the real TTF backend for the no-op stub,
# same pattern as the Theora stub right below.
if(PROSPERO)
    list(REMOVE_ITEM SOURCES_LIBBGGFX "${CMAKE_SOURCE_DIR}/modules/libbggfx/g_font_ttf.c")
else()
    list(REMOVE_ITEM SOURCES_LIBBGGFX "${CMAKE_SOURCE_DIR}/modules/libbggfx/g_font_ttf_stub.c")
endif()

# Real Theora decoding needs libogg/libvorbis/libtheoradec; where they aren't available
# (no system dev packages, e.g. mingw/Windows with nothing vendored) MEDIA_LOAD/MEDIA_PLAY
# fall back to the no-op stub instead of failing the whole module's build.
if(PROSPERO)
    # PS5: always the real decoder, resolved from the PacBrew sysroot (build.sh's
    # -DPS5_PACBREW_PREFIX, not a generic system search).
    list(REMOVE_ITEM SOURCES_LIBBGGFX "${CMAKE_SOURCE_DIR}/modules/libbggfx/g_media_theora_stub.c")
    include_directories(${PS5_PACBREW_PREFIX}/include)
    set(THEORA_REAL TRUE)
else()
    find_path(THEORADEC_INCLUDE_DIR theora/theoradec.h)
    find_library(THEORADEC_LIBRARY NAMES theoradec)
    find_library(VORBIS_LIBRARY NAMES vorbis)
    find_library(OGG_LIBRARY NAMES ogg)
    if(THEORADEC_INCLUDE_DIR AND THEORADEC_LIBRARY AND VORBIS_LIBRARY AND OGG_LIBRARY)
        list(REMOVE_ITEM SOURCES_LIBBGGFX "${CMAKE_SOURCE_DIR}/modules/libbggfx/g_media_theora_stub.c")
        include_directories(${THEORADEC_INCLUDE_DIR})
        set(THEORA_REAL TRUE)
    else()
        message(STATUS "bggfx: libtheoradec/vorbis/ogg not found, MEDIA_LOAD/MEDIA_PLAY will be a no-op stub")
        list(REMOVE_ITEM SOURCES_LIBBGGFX "${CMAKE_SOURCE_DIR}/modules/libbggfx/g_media_theora.c"
                                           "${CMAKE_SOURCE_DIR}/vendor/theoraplay/theoraplay.c")
        set(THEORA_REAL FALSE)
    endif()
endif()

if(MINGW)
    set(OGL_LIB -lopengl32)
endif()

if(APPLE)
    set(OGL_LIB -L$ENV{SDKROOT}/System/Library/Frameworks/OpenGL.framework/Libraries/ -lGL)
endif()

if(ANDROID)
    add_definitions(-D__ANDROID__ -DUSE_GLES2)
    include_directories(${PREFIX_JNI}/include)
    set(ANDROID_PATH_LIB -L${PREFIX_JNI}/lib)
    set(OGL_LIB GLESv1_CM GLESv2 OpenSLES log android)
endif()

add_library(bggfx ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBBGGFX})

if(PROSPERO)
    # The real GL driver archives (Mesa/gallium) and libc++ runtime are only
    # resolved at eboot-packaging time (see libmod_3d's CMakeLists.txt for the
    # same note) - SDL_GPU_LIBRARY itself links here since it's genuinely a
    # dependency of this module's own code (g_video.c etc.), same as SDL2.
    target_link_libraries(bggfx ${SDL2_LIBRARY} ${SDL_GPU_LIBRARY} -L${CMAKE_SOURCE_DIR}/bin bgdrtm ${STDLIBSFLAGS} sdlhandler)
else()
    set(THEORA_LIBS)
    if(THEORA_REAL)
        if(PROSPERO)
            set(THEORA_LIBS ogg vorbis theoradec)
        else()
            set(THEORA_LIBS ${THEORADEC_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})
        endif()
    endif()
    target_link_libraries(bggfx ${SDL2_LIBRARY} ${SDL2_LIBRARIES} ${SDL_GPU_LIBRARY} -L${CMAKE_SOURCE_DIR}/bin bgdrtm ${OGL_LIB} ${STDLIBSFLAGS} ${ANDROID_PATH_LIB} ${THEORA_LIBS} sdlhandler ${FREETYPE_LIBRARIES})
endif()
