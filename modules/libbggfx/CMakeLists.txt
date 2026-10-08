project(bggfx)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

find_package(SDL2 REQUIRED)
if(USE_SDL2_GPU)
    find_package(SDL_GPU REQUIRED)
endif()

add_definitions(-D__LIBBGFGX ${EXTRA_CFLAGS})

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/core/bgdrtm ${CMAKE_SOURCE_DIR}/modules/libsdlhandler ${CMAKE_SOURCE_DIR}/modules/libbggfx ${SDL_GPU_INCLUDE_DIR} ${INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/vendor/theoraplay)

if(PROSPERO)
    include_directories(${PS5_OPENGL_SDK_ROOT}/include)
endif()

file(GLOB SOURCES_LIBBGGFX
     "${CMAKE_SOURCE_DIR}/modules/libbggfx/*.c"
     "${CMAKE_SOURCE_DIR}/vendor/theoraplay/theoraplay.c"
     )

# PS5: theoraplay (and the libogg/libtheora/libvorbis chain it needs) isn't
# cross-compiled for this target - swap the real g_media_theora.c for a
# stand-in that satisfies the same interface (see g_media_theora_stub.c).
if(PROSPERO)
    list(REMOVE_ITEM SOURCES_LIBBGGFX
         "${CMAKE_SOURCE_DIR}/modules/libbggfx/g_media_theora.c"
         "${CMAKE_SOURCE_DIR}/vendor/theoraplay/theoraplay.c")
else()
    list(REMOVE_ITEM SOURCES_LIBBGGFX "${CMAKE_SOURCE_DIR}/modules/libbggfx/g_media_theora_stub.c")
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
    target_link_libraries(bggfx ${SDL2_LIBRARY} ${SDL2_LIBRARIES} ${SDL_GPU_LIBRARY} -L${CMAKE_SOURCE_DIR}/bin bgdrtm ${OGL_LIB} ${STDLIBSFLAGS} ${ANDROID_PATH_LIB} ogg vorbis theoradec sdlhandler)
endif()
