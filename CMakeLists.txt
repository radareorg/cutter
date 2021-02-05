cmake_minimum_required(VERSION 3.5)

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DisallowInSource)
include(Utils)

set(CUTTER_PYTHON_MIN 3.5)

option(CUTTER_USE_BUNDLED_RIZIN "Use rizin from src/rizin submodule instead of searching for it on the system" OFF)
option(CUTTER_USE_ADDITIONAL_RIZIN_PATHS "Search rizin in additional paths which are not part of default system library paths.\
 Disable this option if you are linking against rizin pacakged as proper system library or in a custom path and additional are paths causing problems." ON)
option(CUTTER_ENABLE_PYTHON "Enable Python integration. Requires Python >= ${CUTTER_PYTHON_MIN}." OFF)
option(CUTTER_ENABLE_PYTHON_BINDINGS "Enable generating Python bindings with Shiboken2. Unused if CUTTER_ENABLE_PYTHON=OFF." OFF)
option(CUTTER_ENABLE_CRASH_REPORTS "Enable crash report system. Unused if CUTTER_ENABLE_CRASH_REPORTS=OFF" OFF)
option(CUTTER_APPIMAGE_BUILD "Enable Appimage specific changes. Doesn't cause building of Appimage itself." OFF)
tri_option(CUTTER_ENABLE_KSYNTAXHIGHLIGHTING "Use KSyntaxHighlighting" AUTO)
tri_option(CUTTER_ENABLE_GRAPHVIZ "Enable use of graphviz for graph layout" AUTO)
set(SHIBOKEN_EXTRA_OPTIONS "" CACHE STRING "Extra options for shiboken generator")
set(CUTTER_EXTRA_PLUGIN_DIRS "" CACHE STRING "List of addition plugin locations")
option(CUTTER_ENABLE_DEPENDENCY_DOWNLOADS "Enable downloading of dependencies. Setting to OFF doesn't affect any downloads done by rizin build." OFF)
option(CUTTER_ENABLE_PACKAGING "Enable building platform-specific packages for distributing" OFF)
option(CUTTER_PACKAGE_DEPENDENCIES "During install step include the third party dependencies." OFF)
option(CUTTER_PACKAGE_RZ_GHIDRA "Compile and install rz-ghidra during install step." OFF)
option(CUTTER_PACKAGE_JSDEC "Compile and install jsdec during install step." OFF)
option(CUTTER_ENABLE_TESTS "Build QtTest tests" OFF)
OPTION(CUTTER_QT6 "Use QT6" OFF)

if(NOT CUTTER_ENABLE_PYTHON)
    set(CUTTER_ENABLE_PYTHON_BINDINGS OFF)
endif()

set(CUTTER_VERSION_MAJOR 1)
set(CUTTER_VERSION_MINOR 12)
set(CUTTER_VERSION_PATCH 0)

set(CUTTER_VERSION_FULL "${CUTTER_VERSION_MAJOR}.${CUTTER_VERSION_MINOR}.${CUTTER_VERSION_PATCH}")

project(Cutter VERSION "${CUTTER_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 11)

include(CutterInstallDirs)

if(CUTTER_USE_BUNDLED_RIZIN)
    include(BundledRizin)
    set(RIZIN_TARGET Rizin)
else()
    find_package(Rizin REQUIRED)
    set(RIZIN_TARGET Rizin::librz)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
if (CUTTER_QT6)
    set(QT_PREFIX Qt6)
else()
    set(QT_PREFIX Qt5)
endif()
find_package(${QT_PREFIX} REQUIRED COMPONENTS Core Widgets Gui Svg Network)

if(CUTTER_ENABLE_PYTHON)
    find_package(PythonInterp REQUIRED)
    find_package(PythonLibs ${CUTTER_PYTHON_MIN} REQUIRED)

    include_directories(${PYTHON_INCLUDE_DIRS})
    add_definitions(-DCUTTER_ENABLE_PYTHON)

    if(CUTTER_ENABLE_PYTHON_BINDINGS)
        # 5.12.3 => 5.12
        if("${Qt5_VERSION}" MATCHES "^([0-9]+\\.[0-9]+)\\.[0-9]+")
            set(Shiboken2_VERSION_REQUIRED "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR "Failed to recognize Qt version")
        endif()
        find_package(Shiboken2 "${Shiboken2_VERSION_REQUIRED}" REQUIRED)
        find_package(PySide2 "${Shiboken2_VERSION_REQUIRED}" REQUIRED)

        get_target_property(PYSIDE_INCLUDE_DIR PySide2::pyside2 INTERFACE_INCLUDE_DIRECTORIES)
        list(GET PYSIDE_INCLUDE_DIR 0 PYSIDE_INCLUDE_DIR)
        include_directories(${PYSIDE_INCLUDE_DIR}
            ${PYSIDE_INCLUDE_DIR}/QtCore
            ${PYSIDE_INCLUDE_DIR}/QtGui
            ${PYSIDE_INCLUDE_DIR}/QtWidgets)

        add_definitions(-DCUTTER_ENABLE_PYTHON_BINDINGS)
    endif()
endif()

if(CUTTER_ENABLE_KSYNTAXHIGHLIGHTING)
    if(CUTTER_ENABLE_KSYNTAXHIGHLIGHTING STREQUAL AUTO)
        find_package(KF5SyntaxHighlighting)
        if(KF5SyntaxHighlighting_FOUND)
            set(KSYNTAXHIGHLIGHTING_STATUS ON)
        else()
            set(KSYNTAXHIGHLIGHTING_STATUS "OFF (KSyntaxHighlighting not found)")
        endif()
    else()
        find_package(KF5SyntaxHighlighting REQUIRED)
        set(KSYNTAXHIGHLIGHTING_STATUS ON)
    endif()
else()
    set(KSYNTAXHIGHLIGHTING_STATUS OFF)
endif()

if (CUTTER_ENABLE_GRAPHVIZ)
    if (CUTTER_ENABLE_GRAPHVIZ STREQUAL AUTO)
        find_package(Graphviz)
    else()
        find_package(Graphviz REQUIRED)
    endif()
    set (CUTTER_ENABLE_GRAPHVIZ ${Graphviz_FOUND})
endif()

message(STATUS "")
message(STATUS "Building Cutter version ${CUTTER_VERSION_FULL}")
message(STATUS "Options:")
message(STATUS "- Bundled rizin: ${CUTTER_USE_BUNDLED_RIZIN}")
message(STATUS "- Python: ${CUTTER_ENABLE_PYTHON}")
message(STATUS "- Python Bindings: ${CUTTER_ENABLE_PYTHON_BINDINGS}")
message(STATUS "- Crash Handling: ${CUTTER_ENABLE_CRASH_REPORTS}")
message(STATUS "- KSyntaxHighlighting: ${KSYNTAXHIGHLIGHTING_STATUS}")
message(STATUS "- Graphviz: ${CUTTER_ENABLE_GRAPHVIZ}")
message(STATUS "")

add_subdirectory(src)

if(CUTTER_ENABLE_PACKAGING)
	add_subdirectory(dist)
endif()

if(CUTTER_ENABLE_TESTS)
    add_subdirectory(test)
endif()
