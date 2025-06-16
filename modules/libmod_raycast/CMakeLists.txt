project(libmod_raycast)  
    
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")    
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")    
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")    
    
set(CMAKE_MODULE_PATH ../../cmake/Modules)    
    
# Encontrar dependencias SDL2 (siguiendo el patrón de libbggfx)    
find_package(SDL2 REQUIRED)    
if(USE_SDL2_GPU)    
    find_package(SDL_GPU REQUIRED)    
endif()    
    
# Definiciones del compilador    
add_definitions(-D__LIBMOD_RAYCAST ${EXTRA_CFLAGS})    
    
# Directorios de include (incluyendo SDL2)    
include_directories(    
    ${SDL2_INCLUDE_DIR}     
    ${SDL2_INCLUDE_DIRS}     
    ../../core/include    
    ../../core/bgdrtm    
    ../../modules/libbggfx    
    ../../modules/libmod_gfx    
    ${SDL_GPU_INCLUDE_DIR}     
    ${INCLUDE_DIRECTORIES}    
)    
    
file(GLOB SOURCES_RAYCAST "*.c")    
    
add_library(libmod_raycast ${LIBRARY_BUILD_TYPE} ${SOURCES_RAYCAST})    
    
# Enlazar con las mismas librerías que libbggfx    
target_link_libraries(libmod_raycast     
    ${SDL2_LIBRARY}     
    ${SDL2_LIBRARIES}     
    ${SDL_GPU_LIBRARY}     
    -L../../bin     
    bgdrtm     
    bggfx    
    ${STDLIBSFLAGS}    
)    
    
# Configuración específica por plataforma    
if(WIN32)    
    set_target_properties(libmod_raycast PROPERTIES PREFIX "")    
    set_target_properties(libmod_raycast PROPERTIES SUFFIX ".dll")    
elseif(APPLE)    
    set_target_properties(libmod_raycast PROPERTIES PREFIX "lib")    
    set_target_properties(libmod_raycast PROPERTIES SUFFIX ".dylib")    
else()    
    set_target_properties(libmod_raycast PROPERTIES PREFIX "lib")    
    set_target_properties(libmod_raycast PROPERTIES SUFFIX ".so")    
endif()