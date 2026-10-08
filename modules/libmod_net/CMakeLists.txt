project(mod_net)

# PS5: net.c/net.h use plain BSD sockets (sys/socket.h, netinet/in.h...), which
# this payload SDK's libkernel doesn't expose that way - PS5 networking needs
# its own sockets shim (and sandbox entitlements for listen/bind), which is
# separate work from porting the rest of the engine. Skip the module for now;
# __fake_dl just won't have a libmod_net entry, same as any other module a
# platform doesn't build (see core/make-fakedl.sh: SCOPE is just `ls modules/`,
# so this is the only place that needs to know).
if(PROSPERO)
    return()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${TARGET}/bin/")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

find_package(SDL2 REQUIRED)

add_definitions(${EXTRA_CFLAGS})

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/core/bgdrtm ${CMAKE_SOURCE_DIR}/modules/libmod_net ${INCLUDE_DIRECTORIES})

file(GLOB SOURCES_LIBMOD_net
     "${CMAKE_SOURCE_DIR}/modules/libmod_net/*.c"
     )

add_library(mod_net ${LIBRARY_BUILD_TYPE} ${SOURCES_LIBMOD_net})

target_link_libraries(mod_net ${SDL2_LIBRARY} ${SDL2_LIBRARIES} -L${CMAKE_SOURCE_DIR}/bin bgdrtm)
if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0600)
    target_link_libraries(mod_net ws2_32)
endif()
if (SWITCH)
    target_link_libraries(mod_net nx)
endif()
