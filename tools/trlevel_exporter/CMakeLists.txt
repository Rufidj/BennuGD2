project(trlevel_exporter)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bin/")

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(trlevel_exporter
    trlevel_exporter.cpp
    "${CMAKE_SOURCE_DIR}/tools/RAYMAP EDITOR/EDUKE32/source/imgui/src/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/tools/RAYMAP EDITOR/EDUKE32/source/imgui/src/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/tools/RAYMAP EDITOR/EDUKE32/source/imgui/src/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/tools/RAYMAP EDITOR/EDUKE32/source/imgui/src/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/tools/RAYMAP EDITOR/EDUKE32/source/imgui/src/imgui_impl_sdl2.cpp"
    "${CMAKE_SOURCE_DIR}/tools/RAYMAP EDITOR/EDUKE32/source/imgui/src/imgui_impl_opengl3.cpp"
)

target_compile_features(trlevel_exporter PRIVATE cxx_std_17)
target_include_directories(trlevel_exporter PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${Qt5Widgets_INCLUDE_DIRS}
    ${Qt5OpenGL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/vendor/sdl-gpu/src/externals/stb_image
    ${CMAKE_SOURCE_DIR}/vendor/sdl-gpu/src/externals/stb_image_write
    "${CMAKE_SOURCE_DIR}/tools/RAYMAP EDITOR/EDUKE32/source/imgui/include"
)

target_link_libraries(trlevel_exporter PRIVATE
    ${SDL2_LIBRARIES}
    Qt5::Widgets
    Qt5::OpenGL
    OpenGL::GL
    -lm
)
