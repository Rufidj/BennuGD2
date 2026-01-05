QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# Nombre del ejecutable
TARGET = raymap_editor
TEMPLATE = app

# Definir directorio de salida
DESTDIR = $$PWD

# Archivos header
HEADERS += \
    mainwindow.h \
    fpgloader.h \
    grideditor.h \
    texturepalette.h \
    spriteeditor.h \
    cameramarker.h \
    raymapformat.h \
    mapdata.h

# Archivos fuente
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    fpgloader.cpp \
    grideditor.cpp \
    texturepalette.cpp \
    spriteeditor.cpp \
    cameramarker.cpp \
    raymapformat.cpp

# Archivos UI
FORMS += \
    mainwindow.ui

# Configuración de compilación
QMAKE_CXXFLAGS += -Wall -Wextra

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
