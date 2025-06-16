project(libmod_basic)  
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")  
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")  
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")  
  
set(CMAKE_MODULE_PATH ../../cmake/Modules)  
  
find_package(SDL2 REQUIRED)  
if(USE_SDL2_GPU)  
    find_package(SDL_GPU REQUIRED)  
endif()  
  
add_definitions(-D__LIBMOD_BASIC ${EXTRA_CFLAGS})  
  
include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS} ../../core/include ../../core/bgdrtm ../../modules/libbggfx ${SDL_GPU_INCLUDE_DIR} ${INCLUDE_DIRECTORIES})  
  
file(GLOB SOURCES_LIBMOD_BASIC "*.c")  
  
if(MINGW)  
    set(OGL_LIB -lopengl32)  
endif()  
  
if(APPLE)  
    set(OGL_LIB -L$ENV{SDKROOT}/System/Library/Frameworks/OpenGL.framework/Libraries/ -lGL)  
endif()  
  
add_library(mod_basic ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_BASIC})  
  
target_link_libraries(mod_basic ${SDL2_LIBRARY} ${SDL2_LIBRARIES} ${SDL_GPU_LIBRARY} -L../../bin bgdrtm bggfx ${OGL_LIB} ${STDLIBSFLAGS})