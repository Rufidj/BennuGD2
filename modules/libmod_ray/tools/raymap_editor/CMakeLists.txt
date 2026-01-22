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
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Network)

# Buscar OpenGL
find_package(OpenGL REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mainwindow_darkmode.cpp
    mainwindow_build.cpp
    mainwindow_project.cpp
    newprojectdialog.h newprojectdialog.cpp
    projectsettingsdialog.h projectsettingsdialog.cpp
    consolewidget.h consolewidget.cpp
    assetbrowser.h assetbrowser.cpp
    buildmanager.h buildmanager.cpp
    bennugdinstaller.h bennugdinstaller.cpp
    codegenerator.h codegenerator.cpp
    processgenerator.h processgenerator.cpp
    projectmanager.h projectmanager.cpp
    codepreviewpanel.h codepreviewpanel.cpp
    codeeditor.h codeeditor.cpp
    codeeditordialog.h codeeditordialog.cpp
    md3loader.h md3loader.cpp
    entitypropertypanel.h entitypropertypanel.cpp
    objtomd3converter.h objtomd3converter.cpp
    objimportdialog.h objimportdialog.cpp
    publishdialog.h publishdialog.cpp
    publisher.h publisher.cpp
    downloader.h downloader.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(raymap_editor
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        fpgloader.h
        fpgloader.cpp
        fpgeditor.h
        fpgeditor.cpp
        effectgenerator.h
        effectgenerator.cpp
        effectgeneratordialog.h
        effectgeneratordialog.cpp
        camerakeyframe.h
        camerapath.h
        camerapath.cpp
        camerapathio.h
        camerapathio.cpp
        camerapatheditor.h
        camerapatheditor.cpp
        camerapathcanvas.h
        camerapathcanvas.cpp
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
        insertboxdialog.h
        insertboxdialog.cpp
        meshgeneratordialog.h
        meshgeneratordialog.cpp
        rampgenerator.h
        rampgenerator.cpp
        rampgeneratordialog.h
        rampgeneratordialog.cpp
        md3generator.h
        md3generator.cpp
        modelpreviewwidget.h
        modelpreviewwidget.cpp
        textureatlasgen.h
        textureatlasgen.cpp
    )
else()
    add_executable(raymap_editor
        ${PROJECT_SOURCES}
        fpgloader.h
        fpgloader.cpp
        fpgeditor.h
        fpgeditor.cpp
        effectgenerator.h
        effectgenerator.cpp
        effectgeneratordialog.h
        effectgeneratordialog.cpp
        camerakeyframe.h
        camerapath.h
        camerapath.cpp
        camerapathio.h
        camerapathio.cpp
        camerapatheditor.h
        camerapatheditor.cpp
        camerapathcanvas.h
        camerapathcanvas.cpp
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
        insertboxdialog.h
        insertboxdialog.cpp
        meshgeneratordialog.h
        meshgeneratordialog.cpp
        rampgenerator.h
        rampgenerator.cpp
        rampgeneratordialog.h
        rampgeneratordialog.cpp
        md3generator.h
        md3generator.cpp
        modelpreviewwidget.h
        modelpreviewwidget.cpp
        textureatlasgen.h
        textureatlasgen.cpp
    )
endif()

# Enlazar bibliotecas
target_link_libraries(raymap_editor PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    Qt${QT_VERSION_MAJOR}::Network
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

# Copy loader_stub.exe to build directory
add_custom_command(TARGET raymap_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/loader_stub.exe"
    "$<TARGET_FILE_DIR:raymap_editor>/loader_stub.exe"
    COMMENT "Copying loader_stub.exe to output directory"
)
