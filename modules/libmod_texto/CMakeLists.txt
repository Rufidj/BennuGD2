project(mod_texto)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin/")

set(CMAKE_MODULE_PATH ../../cmake/Modules)

add_definitions(${EXTRA_CFLAGS})

include_directories(../../core/include ../../core/bgdrtm ../../modules/libbgload ${INCLUDE_DIRECTORIES})

# Usar la ruta relativa correcta con CMAKE_CURRENT_SOURCE_DIR
file(GLOB SOURCES_LIBMOD_MOSTRARTEXTO
     "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# Usar la variable correcta en add_library
add_library(mod_texto ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_MOSTRARTEXTO})

# Asegurarse de que las bibliotecas se enlacen correctamente
target_link_libraries(mod_texto ${SDL2_LIBRARY} ${SDL2_LIBRARIES} -L../../bin bgdrtm)

# Mensaje para depurar y verificar los archivos fuente encontrados
message(STATUS "SOURCES_LIBMOD_MOSTRARTEXTO: ${SOURCES_LIBMOD_MOSTRARTEXTO}")

