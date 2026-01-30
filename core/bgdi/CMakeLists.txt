project(bgdi)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")

find_package(ZLIB REQUIRED)
message(STATUS "DEBUG: NINTENDO_SWITCH=${NINTENDO_SWITCH} CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")

if (LIBRARY_BUILD_TYPE STREQUAL "STATIC")
    file(GLOB MODULE_HEADERS "${CMAKE_SOURCE_DIR}/modules/*")

    # Emscripten provides SDL2 through ports, so we don't need to find it
    if(NOT EMSCRIPTEN)
        find_package(SDL2 REQUIRED)
        find_package(SDL2_mixer REQUIRED)
        find_package(SDL2_image REQUIRED)
        if(USE_SDL2_GPU)
            find_package(SDL_GPU REQUIRED)
        endif()
    endif()

    include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS} ${SDL_GPU_INCLUDE_DIR} ${SDLMIXER_INCLUDE_DIRS} ${SDL2_mixer_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/core/bgdrtm ${MODULE_HEADERS})


    foreach(module ${MODULE_HEADERS})
        get_filename_component(lib_name ${module} NAME)

        string(REGEX REPLACE "^lib" "" lib_name ${lib_name})

        list(APPEND EXTRA_LIBS "${lib_name}")
    endforeach()

    if(MINGW)
        set(OGL_LIB -lopengl32)
    endif()

    if(LINUX AND NOT PS3_PPU)
        set(OGL_LIB -lGL)
    endif()

    if(NINTENDO_SWITCH OR CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
        message(STATUS "Configuring for Nintendo Switch...")
        set(OGL_LIB -lglad -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx -lm)
        set(OTHER_LIBS -lpng -ljpeg -lwebp -lmpg123 -lmodplug -lopusfile -lopus -lvorbisfile -lvorbis -logg)
    endif()

    if(VITA)
        message(STATUS "Configuring for PS Vita...")
        set(OTHER_LIBS -lpng -ljpeg -lz -lm -lSceDisplay_stub -lSceGxm_stub -lSceCtrl_stub -lSceAudio_stub -lSceAudioIn_stub -lScePower_stub -lSceTouch_stub -lSceCommonDialog_stub -lSceHid_stub -lSceMotion_stub -lSceAppMgr_stub -lSceAppUtil_stub -lSceSysmodule_stub -lvorbisfile -lvorbis -logg -lxmp -lopusfile -lopus -lmodplug -lstdc++ -lwebpdemux -lwebp -lpthread)
    endif()

    if(PS3_PPU)
        set(OTHER_LIBS -L/usr/local/ps3dev/portlibs/ppu/lib -L/usr/local/ps3dev/spu/spu/lib -L/usr/local/ps3dev/ppu/powerpc64-ps3-elf/lib -L/usr/local/ps3dev/portlibs/ppu/lib -L/usr/local/ps3dev/ppu/lib -lm -lgcm_sys -lrsx -lsysutil -lrt -llv2 -lio -laudio -lpng -lpng14 -ltiff -ljpeg)
    endif()

    if(LIBVLC_ENABLED)
        set(VLC_LIB -lvlc)
    endif()

    list(APPEND EXTRA_LIBS ${SDL2_LIBS} ${SDL2_LIBRARY} ${SDL2_LIBRARIES} ${SDLMIXER_LIBRARY} ${SDL2_mixer_LIBRARIES} ${SDL2_IMAGE_LIBRARY} ${SDL2_image_LIBRARIES} ${SDL_GPU_LIBRARY} ${OGL_LIB} ${VLC_LIB})

endif()

if(NOT PS3_PPU)
    set(OTHER_LIBS ${OTHER_LIBS} -lm)
endif()

add_definitions(-D__BGDI__ -DVERSION="2.0.0")
add_definitions($ENV{EXTRA_CFLAGS})
add_definitions(${SDL2_CFLAGS})

include_directories(${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/core/bgdi ${CMAKE_SOURCE_DIR}/core/bgdrtm ${INCLUDE_DIRECTORIES})

if(NINTENDO_SWITCH)
file(GLOB SOURCES
     "${CMAKE_SOURCE_DIR}/core/bgdi/*.cpp"
     "${CMAKE_SOURCE_DIR}/core/bgdi/*.c"
     )
else()
file(GLOB SOURCES
     "${CMAKE_SOURCE_DIR}/core/bgdi/*.c"
     )
endif()

if(MINGW)
   set(SOURCES ${SOURCES} bgdi.rc)
endif()

if(ANDROID)
    if(NOT DEFINED SDL2_INCLUDE_DIR)
        find_package(SDL2 REQUIRED)
    endif()

    include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS})

    add_definitions(-D__ANDROID__ -DUSE_GLES2)
    include_directories(${PREFIX_JNI}/include)
    set(ANDROID_PATH_LIB -L${PREFIX_JNI}/lib)
    set(OGL_LIB GLESv1_CM GLESv2 OpenSLES log android)

    add_library(main SHARED ${SOURCES})
    list(APPEND OTHER_LIBS log)
    target_link_libraries(main  ${SDL2_LIBRARY} ${SDL2_LIBRARIES} -L${CMAKE_SOURCE_DIR}/bin bgdrtm ${OGL_LIB} ${STDLIBSFLAGS} ${ANDROID_PATH_LIB} ${EXTRA_LIBS} ${OTHER_LIBS})
elseif(EMSCRIPTEN)
    add_definitions(-D__EMSCRIPTEN__)
    
    # Create the executable
    add_executable(bgdi ${SOURCES})
    
    # Emscripten-specific linker flags
    set(EMSCRIPTEN_LINK_FLAGS "")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s WASM=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ASSERTIONS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_SDL=2")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_SDL_IMAGE=2")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s SDL2_IMAGE_FORMATS='[\"png\",\"jpg\",\"bmp\"]'")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_SDL_MIXER=2")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_ZLIB=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_OGG=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_VORBIS=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s USE_LIBPNG=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXPORTED_FUNCTIONS='[\"_main\"]'")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\"]'")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s TOTAL_MEMORY=67108864")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s FORCE_FILESYSTEM=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s ASYNCIFY=1")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/core/bgdi/shell.html")
    
    # Preload game.dcb if it exists (user should place their game.dcb in the build directory)
    # set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --preload-file game.dcb")
    
    # Set output to .html (will also generate .js and .wasm)
    set_target_properties(bgdi PROPERTIES SUFFIX ".html")
    set_target_properties(bgdi PROPERTIES LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
    
    target_link_libraries(bgdi -L${CMAKE_SOURCE_DIR}/bin bgdrtm ${EXTRA_LIBS} ${OTHER_LIBS})
    
    # Copy shell.html to build directory for reference
    add_custom_command(TARGET bgdi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/core/bgdi/shell.html
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shell_template.html
        COMMENT "Copying shell.html template to build directory"
    )
else()
    add_executable(bgdi ${SOURCES})
    target_link_libraries(bgdi -L${CMAKE_SOURCE_DIR}/bin bgdrtm -Wl,--start-group ${EXTRA_LIBS} ${OTHER_LIBS} -Wl,--end-group)
endif()

if(VITA)
    add_definitions(-DVITA)
    include("$ENV{VITASDK}/share/vita.cmake")
    
    # Explicitly point to the binary because the macro expects it in the current dir
    set(BGDI_EXEC "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}bgdi")
    
    vita_create_self(bgdi.self "${BGDI_EXEC}" UNSAFE)
    
    # Ensure bgdi is built before trying to convert it
    if(TARGET bgdi-velf)
        add_dependencies(bgdi-velf bgdi)
    endif()

    vita_create_vpk(Joselkiller.vpk BGDI00001 bgdi.self
        VERSION "01.00"
        NAME "Josel Killer"
        FILE "/home/ruben/BennuGD2/games/Joselkiller/Joselkiller.dcb" game.dcb
        FILE "/home/ruben/BennuGD2/games/Joselkiller/fnt" fnt
        FILE "/home/ruben/BennuGD2/games/Joselkiller/fpg" fpg
        FILE "/home/ruben/BennuGD2/games/Joselkiller/sound" sound
        FILE "/home/ruben/BennuGD2/games/Joselkiller/controls.txt" controls.txt
        FILE "/home/ruben/BennuGD2/games/Joselkiller/data.txt" data.txt
        FILE "/home/ruben/BennuGD2/games/Joselkiller/durezas.txt" durezas.txt
        FILE "/home/ruben/BennuGD2/games/Joselkiller/resources.txt" resources.txt
    )
endif()

