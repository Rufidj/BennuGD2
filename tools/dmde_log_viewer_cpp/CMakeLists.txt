cmake_minimum_required(VERSION 3.16)
project(recovery_studio_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(recovery_studio_qt
    src/main.cpp
    src/core/ScanEngine.cpp
    src/core/ScanEngine.h
    src/core/VolumeExplorer.cpp
    src/core/VolumeExplorer.h
    src/core/VolumeProbe.cpp
    src/core/VolumeProbe.h
    src/core/LogParser.cpp
    src/core/LogParser.h
    src/core/ScanPlanner.cpp
    src/core/ScanPlanner.h
    src/core/ScanProfile.cpp
    src/core/ScanProfile.h
    src/core/SessionLoader.cpp
    src/core/SessionLoader.h
    src/core/SessionExporter.cpp
    src/core/SessionExporter.h
    src/core/RecoveryExtractor.cpp
    src/core/RecoveryExtractor.h
    src/core/DiskDoctor.cpp
    src/core/DiskDoctor.h
    src/core/RepairAdvisor.cpp
    src/core/RepairAdvisor.h
    src/core/SurfaceScanner.cpp
    src/core/SurfaceScanner.h
    src/core/SmartProbe.cpp
    src/core/SmartProbe.h
    src/core/DiskHealthReport.cpp
    src/core/DiskHealthReport.h
    src/core/DiskHealthReportExporter.cpp
    src/core/DiskHealthReportExporter.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/core/RecoveryNode.h
    src/core/RecoveryTreeModel.cpp
    src/core/RecoveryTreeModel.h
)

target_include_directories(recovery_studio_qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(recovery_studio_qt PRIVATE
    Qt5::Widgets
)

