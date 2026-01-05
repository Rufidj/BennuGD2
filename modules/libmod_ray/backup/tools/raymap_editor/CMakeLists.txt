cmake_minimum_required(VERSION 3.16)

project(raymap_editor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar zlib para FPG
find_package(ZLIB REQUIRED)

# Buscar Qt5 o Qt6
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

# Buscar OpenGL
find_package(OpenGL REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(raymap_editor
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        fpgloader.h
        fpgloader.cpp
        grideditor.h
        grideditor.cpp
        textureselector.h
        textureselector.cpp
        spriteeditor.h
        spriteeditor.cpp
        cameramarker.h
        cameramarker.cpp
        raymapformat.h
        raymapformat.cpp
        mapdata.h
        visualrenderer.h
        visualrenderer.cpp
        visualmodewidget.h
        visualmodewidget.cpp
        wldimporter.h
        wldimporter.cpp
        raycastrenderer.h
        raycastrenderer.cpp
    )
else()
    add_executable(raymap_editor
        ${PROJECT_SOURCES}
        fpgloader.h
        fpgloader.cpp
        grideditor.h
        grideditor.cpp
        textureselector.h
        textureselector.cpp
        spriteeditor.h
        spriteeditor.cpp
        cameramarker.h
        cameramarker.cpp
        raymapformat.h
        raymapformat.cpp
        mapdata.h
        visualrenderer.h
        visualrenderer.cpp
        visualmodewidget.h
        visualmodewidget.cpp
        wldimporter.h
        wldimporter.cpp
        raycastrenderer.h
        raycastrenderer.cpp
    )
endif()

# Enlazar bibliotecas
target_link_libraries(raymap_editor PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    ${OPENGL_LIBRARIES}
    ZLIB::ZLIB
)

# Configuración de propiedades
set_target_properties(raymap_editor PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.bennugd.raymap_editor
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Instalación
include(GNUInstallDirs)
install(TARGETS raymap_editor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(raymap_editor)
endif()
