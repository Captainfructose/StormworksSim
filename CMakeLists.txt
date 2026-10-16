cmake_minimum_required(VERSION 3.20)
project(StormworksSim VERSION 1.0.0 LANGUAGES CXX)

# ── C++ Standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt6 Setup ─────────────────────────────────────────────────────────────────
# Qt6 requires automoc for signals/slots and autouic for .ui files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Xml)

# ── Source Files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp

    # Core simulation engine
    src/core/Node.cpp
    src/core/Microcontroller.cpp
    src/core/nodes/LogicNodes.cpp
    src/core/nodes/MathNodes.cpp
    src/core/nodes/IONodes.cpp
    src/core/nodes/CompositeNodes.cpp

    # GUI
    src/gui/MainWindow.cpp
    src/gui/NodeGraphView.cpp
    src/gui/NodeGraphItem.cpp
    src/gui/ConnectionItem.cpp
    src/gui/IOPanel.cpp
    src/gui/NodePalette.cpp

    # Resources (icons etc.)
    resources.qrc
)

set(HEADERS
    src/core/Node.h
    src/core/Connection.h
    src/core/Microcontroller.h
    src/core/nodes/LogicNodes.h
    src/core/nodes/MathNodes.h
    src/core/nodes/IONodes.h
    src/core/nodes/StatefulNodes.h
    src/core/nodes/CompositeNodes.h

    src/gui/MainWindow.h
    src/gui/NodeGraphView.h
    src/gui/NodeGraphItem.h
    src/gui/ConnectionItem.h
    src/gui/IOPanel.h
    src/gui/NodePalette.h
    src/gui/NodeCatalog.h
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(StormworksSim ${SOURCES} ${HEADERS})

# ── Include Directories ───────────────────────────────────────────────────────
target_include_directories(StormworksSim PRIVATE src)

# ── Link Qt6 ──────────────────────────────────────────────────────────────────
target_link_libraries(StormworksSim PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Xml
)

# ── Windows: hide the console window in release builds ────────────────────────
# Note: Only set WIN32_EXECUTABLE in Release to avoid MinGW entry point issues
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(StormworksSim PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── Put the exe in the project root ───────────────────────────────────────────
# The per-config variants keep Debug/Release builds from landing in subfolders.
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(StormworksSim PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_SOURCE_DIR})
endforeach()

# Qt's DLLs and plugins must sit beside the exe, so deploy them after each build
if(WIN32)
    get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qtbin "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qtbin}")
    if(WINDEPLOYQT)
        add_custom_command(TARGET StormworksSim POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations "$<TARGET_FILE:StormworksSim>"
            COMMENT "Deploying Qt runtime next to the exe")
    endif()
endif()
