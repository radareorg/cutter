configure_file("${CMAKE_CURRENT_SOURCE_DIR}/CutterConfig.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/CutterConfig.h")

set(SOURCES
    Main.cpp
    core/Cutter.cpp
    dialogs/EditStringDialog.cpp
    dialogs/WriteCommandsDialogs.cpp
    widgets/DisassemblerGraphView.cpp
    widgets/OverviewView.cpp
    common/RichTextPainter.cpp
    dialogs/InitialOptionsDialog.cpp
    dialogs/AboutDialog.cpp
    dialogs/CommentsDialog.cpp
    dialogs/EditInstructionDialog.cpp
    dialogs/FlagDialog.cpp
    dialogs/RemoteDebugDialog.cpp
    dialogs/NativeDebugDialog.cpp
    dialogs/XrefsDialog.cpp
    core/MainWindow.cpp
    common/Helpers.cpp
    common/Highlighter.cpp
    common/MdHighlighter.cpp
    common/DirectionalComboBox.cpp
    dialogs/preferences/AsmOptionsWidget.cpp
    dialogs/NewFileDialog.cpp
    common/AnalTask.cpp
    widgets/CommentsWidget.cpp
    widgets/ConsoleWidget.cpp
    widgets/Dashboard.cpp
    widgets/EntrypointWidget.cpp
    widgets/ExportsWidget.cpp
    widgets/FlagsWidget.cpp
    widgets/FunctionsWidget.cpp
    widgets/ImportsWidget.cpp
    widgets/Omnibar.cpp
    widgets/RelocsWidget.cpp
    widgets/SectionsWidget.cpp
    widgets/SegmentsWidget.cpp
    widgets/StringsWidget.cpp
    widgets/SymbolsWidget.cpp
    menus/DisassemblyContextMenu.cpp
    menus/DecompilerContextMenu.cpp
    widgets/DisassemblyWidget.cpp
    widgets/HexdumpWidget.cpp
    common/Configuration.cpp
    common/Colors.cpp
    common/TempConfig.cpp
    common/SvgIconEngine.cpp
    common/SyntaxHighlighter.cpp
    widgets/DecompilerWidget.cpp
    widgets/VisualNavbar.cpp
    widgets/GraphView.cpp
    dialogs/preferences/PreferencesDialog.cpp
    dialogs/preferences/AppearanceOptionsWidget.cpp
    dialogs/preferences/GraphOptionsWidget.cpp
    dialogs/preferences/PreferenceCategory.cpp
    dialogs/preferences/InitializationFileEditor.cpp
    widgets/QuickFilterView.cpp
    widgets/ClassesWidget.cpp
    widgets/ResourcesWidget.cpp
    widgets/VTablesWidget.cpp
    widgets/TypesWidget.cpp
    widgets/HeadersWidget.cpp
    widgets/SearchWidget.cpp
    CutterApplication.cpp
    dialogs/RizinPluginsDialog.cpp
    widgets/CutterDockWidget.cpp
    widgets/CutterTreeWidget.cpp
    widgets/GraphWidget.cpp
    widgets/OverviewWidget.cpp
    common/JsonTreeItem.cpp
    common/JsonModel.cpp
    dialogs/VersionInfoDialog.cpp
    widgets/ZignaturesWidget.cpp
    common/AsyncTask.cpp
    dialogs/AsyncTaskDialog.cpp
    widgets/StackWidget.cpp
    widgets/RegistersWidget.cpp
    widgets/ThreadsWidget.cpp
    widgets/ProcessesWidget.cpp
    widgets/BacktraceWidget.cpp
    dialogs/MapFileDialog.cpp
    common/CommandTask.cpp
    common/ProgressIndicator.cpp
    common/RizinTask.cpp
    dialogs/RizinTaskDialog.cpp
    widgets/DebugActions.cpp
    widgets/MemoryMapWidget.cpp
    dialogs/preferences/DebugOptionsWidget.cpp
    dialogs/preferences/PluginsOptionsWidget.cpp
    widgets/BreakpointWidget.cpp
    dialogs/BreakpointsDialog.cpp
    dialogs/AttachProcDialog.cpp
    widgets/RegisterRefsWidget.cpp
    dialogs/SetToDataDialog.cpp
    dialogs/EditVariablesDialog.cpp
    dialogs/EditFunctionDialog.cpp
    widgets/CutterTreeView.cpp
    widgets/ComboQuickFilterView.cpp
    dialogs/HexdumpRangeDialog.cpp
    common/CutterSeekable.cpp
    common/RefreshDeferrer.cpp
    dialogs/WelcomeDialog.cpp
    common/RunScriptTask.cpp
    dialogs/EditMethodDialog.cpp
    dialogs/TypesInteractionDialog.cpp
    widgets/SdbWidget.cpp
    plugins/PluginManager.cpp
    common/BasicBlockHighlighter.cpp
    common/BasicInstructionHighlighter.cpp
    dialogs/LinkTypeDialog.cpp
    widgets/ColorPicker.cpp
    common/ColorThemeWorker.cpp
    widgets/ColorThemeComboBox.cpp
    widgets/ColorThemeListView.cpp
    dialogs/preferences/ColorThemeEditDialog.cpp
    common/UpdateWorker.cpp
    widgets/MemoryDockWidget.cpp
    common/BugReporting.cpp
    common/HighDpiPixmap.cpp
    widgets/GraphGridLayout.cpp
    widgets/HexWidget.cpp
    common/SelectionHighlight.cpp
    common/Decompiler.cpp
    menus/AddressableItemContextMenu.cpp
    common/AddressableItemModel.cpp
    widgets/ListDockWidget.cpp
    dialogs/MultitypeFileSaveDialog.cpp
    widgets/BoolToggleDelegate.cpp
    common/IOModesController.cpp
    common/SettingsUpgrade.cpp
    dialogs/LayoutManager.cpp
    common/CutterLayout.cpp
    widgets/GraphHorizontalAdapter.cpp
    common/ResourcePaths.cpp
    widgets/CutterGraphView.cpp
    widgets/SimpleTextGraphView.cpp
    widgets/RizinGraphWidget.cpp
    widgets/CallGraph.cpp
    widgets/AddressableDockWidget.cpp
    dialogs/preferences/AnalOptionsWidget.cpp
    common/DecompilerHighlighter.cpp
)
set(HEADER_FILES
    core/Cutter.h
    core/CutterCommon.h
    core/CutterDescriptions.h
    dialogs/EditStringDialog.h
    dialogs/WriteCommandsDialogs.h
    widgets/DisassemblerGraphView.h
    widgets/OverviewView.h
    common/RichTextPainter.h
    common/CachedFontMetrics.h
    dialogs/AboutDialog.h
    dialogs/preferences/AsmOptionsWidget.h
    dialogs/CommentsDialog.h
    dialogs/EditInstructionDialog.h
    dialogs/FlagDialog.h
    dialogs/RemoteDebugDialog.h
    dialogs/NativeDebugDialog.h
    dialogs/XrefsDialog.h
    common/Helpers.h
    core/MainWindow.h
    common/Highlighter.h
    common/MdHighlighter.h
    common/DirectionalComboBox.h
    dialogs/InitialOptionsDialog.h
    dialogs/NewFileDialog.h
    common/AnalTask.h
    widgets/CommentsWidget.h
    widgets/ConsoleWidget.h
    widgets/Dashboard.h
    widgets/EntrypointWidget.h
    widgets/ExportsWidget.h
    widgets/FlagsWidget.h
    widgets/FunctionsWidget.h
    widgets/ImportsWidget.h
    widgets/Omnibar.h
    widgets/RelocsWidget.h
    widgets/SectionsWidget.h
    widgets/SegmentsWidget.h
    widgets/StringsWidget.h
    widgets/SymbolsWidget.h
    menus/DisassemblyContextMenu.h
    menus/DecompilerContextMenu.h
    widgets/DisassemblyWidget.h
    widgets/HexdumpWidget.h
    common/Configuration.h
    common/Colors.h
    common/TempConfig.h
    common/SvgIconEngine.h
    common/SyntaxHighlighter.h
    widgets/DecompilerWidget.h
    widgets/VisualNavbar.h
    widgets/GraphView.h
    dialogs/preferences/PreferencesDialog.h
    dialogs/preferences/AppearanceOptionsWidget.h
    dialogs/preferences/PreferenceCategory.h
    dialogs/preferences/GraphOptionsWidget.h
    dialogs/preferences/InitializationFileEditor.h
    widgets/QuickFilterView.h
    widgets/ClassesWidget.h
    widgets/ResourcesWidget.h
    CutterApplication.h
    widgets/VTablesWidget.h
    widgets/TypesWidget.h
    widgets/HeadersWidget.h
    widgets/SearchWidget.h
    dialogs/RizinPluginsDialog.h
    widgets/CutterDockWidget.h
    widgets/CutterTreeWidget.h
    widgets/GraphWidget.h
    widgets/OverviewWidget.h
    common/JsonTreeItem.h
    common/JsonModel.h
    dialogs/VersionInfoDialog.h
    widgets/ZignaturesWidget.h
    common/AsyncTask.h
    dialogs/AsyncTaskDialog.h
    widgets/StackWidget.h
    widgets/RegistersWidget.h
    widgets/ThreadsWidget.h
    widgets/ProcessesWidget.h
    widgets/BacktraceWidget.h
    dialogs/MapFileDialog.h
    common/StringsTask.h
    common/FunctionsTask.h
    common/CommandTask.h
    common/ProgressIndicator.h
    plugins/CutterPlugin.h
    common/RizinTask.h
    dialogs/RizinTaskDialog.h
    widgets/DebugActions.h
    widgets/MemoryMapWidget.h
    dialogs/preferences/DebugOptionsWidget.h
    dialogs/preferences/PluginsOptionsWidget.h
    widgets/BreakpointWidget.h
    dialogs/BreakpointsDialog.h
    dialogs/AttachProcDialog.h
    widgets/RegisterRefsWidget.h
    dialogs/SetToDataDialog.h
    common/InitialOptions.h
    dialogs/EditVariablesDialog.h
    dialogs/EditFunctionDialog.h
    widgets/CutterTreeView.h
    widgets/ComboQuickFilterView.h
    dialogs/HexdumpRangeDialog.h
    common/CutterSeekable.h
    common/RefreshDeferrer.h
    dialogs/WelcomeDialog.h
    common/RunScriptTask.h
    common/Json.h
    dialogs/EditMethodDialog.h
    common/CrashHandler.h
    dialogs/TypesInteractionDialog.h
    widgets/SdbWidget.h
    plugins/PluginManager.h
    common/BasicBlockHighlighter.h
    common/BasicInstructionHighlighter.h
    common/UpdateWorker.h
    widgets/ColorPicker.h
    common/ColorThemeWorker.h
    widgets/ColorThemeComboBox.h
    widgets/MemoryDockWidget.h
    widgets/ColorThemeListView.h
    dialogs/preferences/ColorThemeEditDialog.h
    dialogs/LinkTypeDialog.h
    common/BugReporting.h
    common/HighDpiPixmap.h
    widgets/GraphLayout.h
    widgets/GraphGridLayout.h
    widgets/HexWidget.h
    common/SelectionHighlight.h
    common/Decompiler.h
    menus/AddressableItemContextMenu.h
    common/AddressableItemModel.h
    widgets/ListDockWidget.h
    widgets/AddressableItemList.h
    dialogs/MultitypeFileSaveDialog.h
    widgets/BoolToggleDelegate.h
    common/IOModesController.h
    common/SettingsUpgrade.h
    dialogs/LayoutManager.h
    common/CutterLayout.h
    common/BinaryTrees.h
    common/LinkedListPool.h
    widgets/GraphHorizontalAdapter.h
    common/ResourcePaths.h
    widgets/CutterGraphView.h
    widgets/SimpleTextGraphView.h
    widgets/RizinGraphWidget.h
    widgets/CallGraph.h
    widgets/AddressableDockWidget.h
    dialogs/preferences/AnalOptionsWidget.h
    common/DecompilerHighlighter.h
)
set(UI_FILES
    dialogs/AboutDialog.ui
    dialogs/EditStringDialog.ui
    dialogs/Base64EnDecodedWriteDialog.ui
    dialogs/DuplicateFromOffsetDialog.ui
    dialogs/IncrementDecrementDialog.ui
    dialogs/preferences/AsmOptionsWidget.ui
    dialogs/CommentsDialog.ui
    dialogs/EditInstructionDialog.ui
    dialogs/FlagDialog.ui
    dialogs/RemoteDebugDialog.ui
    dialogs/NativeDebugDialog.ui
    dialogs/XrefsDialog.ui
    dialogs/NewFileDialog.ui
    dialogs/InitialOptionsDialog.ui
    dialogs/EditFunctionDialog.ui
    core/MainWindow.ui
    widgets/ConsoleWidget.ui
    widgets/Dashboard.ui
    widgets/EntrypointWidget.ui
    widgets/FlagsWidget.ui
    widgets/StringsWidget.ui
    widgets/HexdumpWidget.ui
    dialogs/preferences/PreferencesDialog.ui
    dialogs/preferences/AppearanceOptionsWidget.ui
    dialogs/preferences/GraphOptionsWidget.ui
    dialogs/preferences/InitializationFileEditor.ui
    widgets/QuickFilterView.ui
    widgets/DecompilerWidget.ui
    widgets/ClassesWidget.ui
    widgets/VTablesWidget.ui
    widgets/TypesWidget.ui
    widgets/SearchWidget.ui
    dialogs/RizinPluginsDialog.ui
    dialogs/VersionInfoDialog.ui
    widgets/ZignaturesWidget.ui
    dialogs/AsyncTaskDialog.ui
    dialogs/RizinTaskDialog.ui
    widgets/StackWidget.ui
    widgets/RegistersWidget.ui
    widgets/ThreadsWidget.ui
    widgets/ProcessesWidget.ui
    widgets/BacktraceWidget.ui
    dialogs/MapFileDialog.ui
    dialogs/preferences/DebugOptionsWidget.ui
    widgets/BreakpointWidget.ui
    dialogs/BreakpointsDialog.ui
    dialogs/AttachProcDialog.ui
    widgets/RegisterRefsWidget.ui
    dialogs/SetToDataDialog.ui
    dialogs/EditVariablesDialog.ui
    widgets/CutterTreeView.ui
    widgets/ComboQuickFilterView.ui
    dialogs/HexdumpRangeDialog.ui
    dialogs/WelcomeDialog.ui
    dialogs/EditMethodDialog.ui
    dialogs/TypesInteractionDialog.ui
    widgets/SdbWidget.ui
    dialogs/LinkTypeDialog.ui
    widgets/ColorPicker.ui
    dialogs/preferences/ColorThemeEditDialog.ui
    widgets/ListDockWidget.ui
    dialogs/LayoutManager.ui
    widgets/RizinGraphWidget.ui
    dialogs/preferences/AnalOptionsWidget.ui
)
set(QRC_FILES
    resources.qrc
    themes/native/native.qrc
    themes/qdarkstyle/dark.qrc
    themes/midnight/midnight.qrc
    themes/lightstyle/light.qrc
)

if (CUTTER_ENABLE_PYTHON)
    list(APPEND SOURCES common/QtResImporter.cpp common/PythonManager.cpp common/PythonAPI.cpp)
    list(APPEND HEADER_FILES common/QtResImporter.h common/PythonManager.h common/PythonAPI.h)
endif()

if(CUTTER_ENABLE_CRASH_REPORTS)
    list(APPEND SOURCES common/CrashHandler.cpp)
endif()

if(CUTTER_ENABLE_PYTHON_BINDINGS)
    set(BINDINGS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bindings")
    set(BINDINGS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/bindings")

    configure_file("${BINDINGS_SRC_DIR}/bindings.xml" "${BINDINGS_BUILD_DIR}/bindings.xml" COPYONLY) # trigger reconfigure if file changes

    execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${BINDINGS_SRC_DIR}/src_list.py" cmake "${BINDINGS_BUILD_DIR}" OUTPUT_VARIABLE BINDINGS_SOURCE)

    set_property(SOURCE ${BINDINGS_SOURCE} PROPERTY SKIP_AUTOGEN ON)

    include_directories("${BINDINGS_BUILD_DIR}/CutterBindings")

    set(SHIBOKEN_OPTIONS)
    if (WIN32)
        list(APPEND SHIBOKEN_OPTIONS --avoid-protected-hack)
    endif()

    add_custom_command(OUTPUT ${BINDINGS_SOURCE}
            COMMAND Shiboken2::shiboken2 --project-file="${BINDINGS_BUILD_DIR}/bindings.txt" ${SHIBOKEN_OPTIONS} ${SHIBOKEN_EXTRA_OPTIONS}
            DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bindings/bindings.xml" "${BINDINGS_BUILD_DIR}/bindings.txt"
            IMPLICIT_DEPENDS CXX "${CMAKE_CURRENT_SOURCE_DIR}/bindings/bindings.h"
            COMMENT "Generating Python bindings with shiboken2")
else()
    set(BINDINGS_SOURCE "")
endif()


if (TARGET Graphviz::GVC)
    list(APPEND SOURCES widgets/GraphvizLayout.cpp)
    list(APPEND HEADER_FILES widgets/GraphvizLayout.h)
endif()

if (WIN32)
    set(PLATFORM_RESOURCES "img/cutter.rc")
else()
    set(PLATFORM_RESOURCES "")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
        OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_definitions(-Wall -Wextra)
    set_source_files_properties(${BINDINGS_SOURCE} PROPERTIES COMPILE_FLAGS -w)
endif()

list(REMOVE_ITEM SOURCES Main.cpp)
add_library(CutterLib STATIC ${UI_FILES} ${QRC_FILES} ${PLATFORM_RESOURCES} ${SOURCES} ${HEADER_FILES} ${BINDINGS_SOURCE})
add_executable(Cutter Main.cpp)
set_target_properties(Cutter PROPERTIES
        OUTPUT_NAME cutter
        RUNTIME_OUTPUT_DIRECTORY ..
        PDB_OUTPUT_DIRECTORY ..
        ENABLE_EXPORTS ON
        CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(Cutter PRIVATE CUTTER_SOURCE_BUILD)

set(CUTTER_INCLUDE_DIRECTORIES core widgets common plugins menus .)
foreach(_dir ${CUTTER_INCLUDE_DIRECTORIES})
    target_include_directories(Cutter PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${_dir}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cutter/${_dir}>
    )
    target_include_directories(CutterLib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${_dir}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cutter/${_dir}>
    )
endforeach()
target_link_libraries(Cutter CutterLib)

if (TARGET Graphviz::GVC)
    target_link_libraries(CutterLib PRIVATE Graphviz::GVC)
    target_compile_definitions(CutterLib PRIVATE CUTTER_ENABLE_GRAPHVIZ)
endif()

if(CUTTER_ENABLE_CRASH_REPORTS)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(CutterLib PRIVATE Threads::Threads)

    add_definitions(-DCUTTER_ENABLE_CRASH_REPORTS)
    if (NOT WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g ")
    endif()
    find_package(Breakpad REQUIRED)
    target_link_libraries(CutterLib PRIVATE Breakpad::client)
endif()


target_link_libraries(CutterLib PUBLIC ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets ${QT_PREFIX}::Gui PRIVATE  ${QT_PREFIX}::Svg ${QT_PREFIX}::Network)
target_link_libraries(CutterLib PUBLIC ${RIZIN_TARGET})
if(CUTTER_ENABLE_PYTHON)
    if (WIN32)
        # On windows some of the Python STABLE API functions are in seperate library
        # which isn't added by CMake.
        foreach(_PYTHON_LIBRARY ${PYTHON_LIBRARIES})
            # Skip extra "optimized" and "debug" values that are only meant for passing to target_link_libraries()
            if((NOT _PYTHON_LIBRARY STREQUAL "optimized") AND (NOT _PYTHON_LIBRARY STREQUAL "debug"))
                get_filename_component(_PYTHON_LIB_DIR ${_PYTHON_LIBRARY} DIRECTORY)
                message(STATUS "Add extra library dir for Python: ${_PYTHON_LIB_DIR}")
                target_link_directories(CutterLib PRIVATE ${_PYTHON_LIB_DIR})
            endif()
        endforeach()
    endif()
    target_link_libraries(CutterLib PRIVATE ${PYTHON_LIBRARIES})
    if(CUTTER_ENABLE_PYTHON_BINDINGS)
        target_link_libraries(CutterLib PRIVATE Shiboken2::libshiboken PySide2::pyside2)

        get_target_property(RAW_BINDINGS_INCLUDE_DIRS CutterLib INCLUDE_DIRECTORIES)
        set(BINDINGS_INCLUDE_DIRS "")
        foreach(_dir ${RAW_BINDINGS_INCLUDE_DIRS})
            string(REGEX REPLACE "\\$<BUILD_INTERFACE:(.*)>" "\\1" _dir ${_dir})
            string(REGEX REPLACE "\\$<INSTALL_INTERFACE:(.*)>" "" _dir ${_dir})
            if (NOT "${_dir}" STREQUAL "")
                list(APPEND BINDINGS_INCLUDE_DIRS "${_dir}")
            endif()
        endforeach()

        if(APPLE AND _qt5Core_install_prefix)
            list(APPEND BINDINGS_INCLUDE_DIRS "${_qt5Core_install_prefix}/include")
            list(APPEND BINDINGS_INCLUDE_DIRS "${_qt5Core_install_prefix}/include/QtCore")
            list(APPEND BINDINGS_INCLUDE_DIRS "${_qt5Core_install_prefix}/include/QtGui")
            list(APPEND BINDINGS_INCLUDE_DIRS "${_qt5Core_install_prefix}/include/QtWidgets")
        endif()
        list(APPEND BINDINGS_INCLUDE_DIRS ${Qt5Core_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS})
        list(APPEND BINDINGS_INCLUDE_DIRS ${Rizin_INCLUDE_DIRS})
        list(APPEND BINDINGS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
        if (NOT WIN32)
            string(REPLACE ";" ":" BINDINGS_INCLUDE_DIRS "${BINDINGS_INCLUDE_DIRS}")
        endif()

        configure_file("${BINDINGS_SRC_DIR}/bindings.txt.in" "${BINDINGS_BUILD_DIR}/bindings.txt")
        add_compile_definitions(WIN32_LEAN_AND_MEAN)
    endif()
endif()

if(TARGET KF5::SyntaxHighlighting)
    target_link_libraries(CutterLib PRIVATE KF5::SyntaxHighlighting)
    target_compile_definitions(CutterLib PRIVATE CUTTER_ENABLE_KSYNTAXHIGHLIGHTING)
endif()

if (CUTTER_APPIMAGE_BUILD)
    target_compile_definitions(Cutter PRIVATE APPIMAGE)
endif()

if (CUTTER_PACKAGE_JSDEC)
	target_compile_definitions(Cutter PRIVATE CUTTER_APPVEYOR_JSDEC)
endif()

if(APPLE AND CUTTER_ENABLE_PACKAGING AND CUTTER_USE_BUNDLED_RIZIN)
	target_compile_definitions(Cutter PRIVATE MACOS_RZ_BUNDLED)
endif()

include(Translations)

# Install files
install(TARGETS Cutter CutterLib
        EXPORT CutterTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION "." # needs to be tested
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Devel
)
install(EXPORT CutterTargets
    NAMESPACE Cutter::
    DESTINATION "${ConfigPackageLocation}"
    COMPONENT Devel)
install(FILES
    ../cmake/CutterConfig.cmake
    DESTINATION ${ConfigPackageLocation}
    COMPONENT Devel
)
foreach(_file ${HEADER_FILES})
    # Can't use target PUBLIC_HEADER option for installing due to multiple directories
    get_filename_component(_header_dir "${_file}" DIRECTORY)
    install (FILES "${_file}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/cutter/${_header_dir}" COMPONENT Devel)
endforeach()

if(UNIX AND NOT APPLE)
    install (FILES "img/cutter.svg"
                DESTINATION "share/icons/hicolor/scalable/apps/")
    install(FILES "re.rizin.cutter.desktop"
        DESTINATION "share/applications"
        COMPONENT Devel)
endif()
